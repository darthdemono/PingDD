/**
 * @file cpool.c
 * @brief Persistent concurrent probe pool implementation.
 */

#include "cpool.h"

#include "socket.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* --------------------------------------------------------------------------
 * Portable mutex / condition-variable / thread wrappers
 * ------------------------------------------------------------------------ */

#ifdef _WIN32
typedef CRITICAL_SECTION mtx_t;
typedef CONDITION_VARIABLE cnd_t;
typedef HANDLE thr_t;
static void mtx_init(mtx_t *m) { InitializeCriticalSection(m); }
static void mtx_destroy(mtx_t *m) { DeleteCriticalSection(m); }
static void mtx_lock(mtx_t *m) { EnterCriticalSection(m); }
static void mtx_unlock(mtx_t *m) { LeaveCriticalSection(m); }
static void cnd_init(cnd_t *c) { InitializeConditionVariable(c); }
static void cnd_destroy(cnd_t *c) { (void)c; }
static void cnd_wait(cnd_t *c, mtx_t *m) {
  (void)SleepConditionVariableCS(c, m, INFINITE);
}
static void cnd_signal(cnd_t *c) { WakeConditionVariable(c); }
static void cnd_broadcast(cnd_t *c) { WakeAllConditionVariable(c); }
#else
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef pthread_t thr_t;
static void mtx_init(mtx_t *m) { (void)pthread_mutex_init(m, NULL); }
static void mtx_destroy(mtx_t *m) { (void)pthread_mutex_destroy(m); }
static void mtx_lock(mtx_t *m) { (void)pthread_mutex_lock(m); }
static void mtx_unlock(mtx_t *m) { (void)pthread_mutex_unlock(m); }
static void cnd_init(cnd_t *c) { (void)pthread_cond_init(c, NULL); }
static void cnd_destroy(cnd_t *c) { (void)pthread_cond_destroy(c); }
static void cnd_wait(cnd_t *c, mtx_t *m) { (void)pthread_cond_wait(c, m); }
static void cnd_signal(cnd_t *c) { (void)pthread_cond_signal(c); }
static void cnd_broadcast(cnd_t *c) { (void)pthread_cond_broadcast(c); }
#endif

/* --------------------------------------------------------------------------
 * Shared probe routine
 * ------------------------------------------------------------------------ */

void Probe_One(probe_target_t *t, uint32_t timeout) {
  double rtt = 0.0;
  char ip[64] = {0};
  t->LastResult = Connect(&t->Resolved, timeout, &rtt, ip, sizeof(ip));
  t->LastRtt = rtt;
  (void)strncpy(t->LastIp, ip, sizeof(t->LastIp) - 1U);
  t->LastIp[sizeof(t->LastIp) - 1U] = '\0';
}

/* --------------------------------------------------------------------------
 * Pool
 * ------------------------------------------------------------------------ */

struct cpool {
  thr_t *Threads;
  size_t NThreads;

  mtx_t Mtx;
  cnd_t StartCv;
  cnd_t DoneCv;

  probe_target_t **Rt; /* borrowed */
  size_t Rc;
  uint32_t Timeout;

  size_t WorkIndex;     /* next target to claim this cycle */
  size_t Remaining;     /* workers still busy this cycle */
  unsigned long Gen;    /* incremented per cycle to release workers */
  int Shutdown;
};

#ifdef _WIN32
static DWORD WINAPI WorkerMain(LPVOID arg)
#else
static void *WorkerMain(void *arg)
#endif
{
  cpool_t *p = (cpool_t *)arg;
  /* Baseline 0 == "no cycle processed yet". Seeding from p->Gen here would
   * race a worker that starts after the first RunCycle has already bumped Gen,
   * causing it to skip that cycle and leave Remaining stuck above zero. */
  unsigned long seen = 0;

  mtx_lock(&p->Mtx);
  for (;;) {
    while ((p->Shutdown == 0) && (p->Gen == seen)) {
      cnd_wait(&p->StartCv, &p->Mtx);
    }
    if (p->Shutdown != 0) {
      break;
    }
    seen = p->Gen;

    /* Claim and probe targets until the cycle's work is exhausted. */
    for (;;) {
      size_t idx = 0;
      if (p->WorkIndex >= p->Rc) {
        break;
      }
      idx = p->WorkIndex++;
      mtx_unlock(&p->Mtx);
      Probe_One(p->Rt[idx], p->Timeout);
      mtx_lock(&p->Mtx);
    }

    if (--p->Remaining == 0U) {
      cnd_signal(&p->DoneCv);
    }
  }
  mtx_unlock(&p->Mtx);

#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

cpool_t *Cpool_Create(probe_target_t **rt, size_t rc, uint32_t timeout) {
  cpool_t *p = NULL;
  size_t nthreads = rc;
  size_t i = 0;

  if ((rt == NULL) || (rc == 0U)) {
    return NULL;
  }
  if (nthreads > CPOOL_MAX_THREADS) {
    nthreads = CPOOL_MAX_THREADS;
  }

  p = (cpool_t *)calloc(1, sizeof(*p));
  if (p == NULL) {
    return NULL;
  }
  p->Threads = (thr_t *)calloc(nthreads, sizeof(thr_t));
  if (p->Threads == NULL) {
    free(p);
    return NULL;
  }
  p->NThreads = nthreads;
  p->Rt = rt;
  p->Rc = rc;
  p->Timeout = timeout;
  p->WorkIndex = rc; /* no work pending until first cycle */
  p->Remaining = 0;
  p->Gen = 0;
  p->Shutdown = 0;
  mtx_init(&p->Mtx);
  cnd_init(&p->StartCv);
  cnd_init(&p->DoneCv);

  for (i = 0; i < nthreads; i++) {
#ifdef _WIN32
    p->Threads[i] = CreateThread(NULL, 0, WorkerMain, p, 0, NULL);
    if (p->Threads[i] == NULL) {
      p->NThreads = i;
      break;
    }
#else
    if (pthread_create(&p->Threads[i], NULL, WorkerMain, p) != 0) {
      p->NThreads = i;
      break;
    }
#endif
  }

  if (p->NThreads == 0U) {
    Cpool_Destroy(p);
    return NULL;
  }
  return p;
}

void Cpool_RunCycle(cpool_t *pool) {
  if (pool == NULL) {
    return;
  }
  mtx_lock(&pool->Mtx);
  pool->WorkIndex = 0;
  pool->Remaining = pool->NThreads;
  pool->Gen++;
  cnd_broadcast(&pool->StartCv);
  while (pool->Remaining > 0U) {
    cnd_wait(&pool->DoneCv, &pool->Mtx);
  }
  mtx_unlock(&pool->Mtx);
}

void Cpool_Destroy(cpool_t *pool) {
  size_t i = 0;

  if (pool == NULL) {
    return;
  }
  mtx_lock(&pool->Mtx);
  pool->Shutdown = 1;
  cnd_broadcast(&pool->StartCv);
  mtx_unlock(&pool->Mtx);

  for (i = 0; i < pool->NThreads; i++) {
#ifdef _WIN32
    if (pool->Threads[i] != NULL) {
      (void)WaitForSingleObject(pool->Threads[i], INFINITE);
      (void)CloseHandle(pool->Threads[i]);
    }
#else
    (void)pthread_join(pool->Threads[i], NULL);
#endif
  }

  mtx_destroy(&pool->Mtx);
  cnd_destroy(&pool->StartCv);
  cnd_destroy(&pool->DoneCv);
  free(pool->Threads);
  free(pool);
}
