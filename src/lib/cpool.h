/**
 * @file cpool.h
 * @brief Persistent concurrent probe pool.
 *
 * A fixed set of worker threads is created once and reused for every probe
 * cycle, instead of spawning and joining threads per cycle. This keeps
 * concurrent monitoring cheap even for large target fleets: thread creation is
 * paid once, and the worker count is capped regardless of target count.
 */
#ifndef PINGDD_CPOOL_H
#define PINGDD_CPOOL_H

#include "standard.h"
#include "targets.h"

/** @brief Upper bound on worker threads in the pool. */
#define CPOOL_MAX_THREADS 64U

/** @brief Opaque concurrent pool handle. */
typedef struct cpool cpool_t;

/**
 * @brief Probe a single target once, storing the result in its scratch fields.
 *
 * Shared by the sequential and concurrent schedulers.
 *
 * @param[in,out] t       Target to probe.
 * @param[in]     timeout Per-probe timeout in milliseconds.
 */
void Probe_One(probe_target_t *t, uint32_t timeout);

/**
 * @brief Create a pool of reusable worker threads over a target set.
 *
 * @param[in] rt      Array of resolved target pointers (borrowed, must outlive
 *                    the pool).
 * @param[in] rc      Number of targets.
 * @param[in] timeout Per-probe timeout in milliseconds.
 * @return Pool handle, or NULL on failure.
 */
cpool_t *Cpool_Create(probe_target_t **rt, size_t rc, uint32_t timeout);

/**
 * @brief Probe every target once using the worker pool; blocks until the whole
 * cycle is complete.
 *
 * @param[in] pool Pool handle.
 */
void Cpool_RunCycle(cpool_t *pool);

/**
 * @brief Stop the workers and free the pool.
 * @param[in] pool Pool handle (may be NULL).
 */
void Cpool_Destroy(cpool_t *pool);

#endif /* PINGDD_CPOOL_H */
