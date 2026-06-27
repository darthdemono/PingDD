# ============================================================================
# Universal Makefile for PingDD (Windows x86, Windows ARM64, Linux x86_64, Linux ARM/ARM64)
# ============================================================================

# ----------------------------------------------------------------------------
# Target OS selection
# ----------------------------------------------------------------------------
TARGET_OS ?= auto
TARGET_OS := $(strip $(TARGET_OS))

ifeq ($(TARGET_OS), auto)
  ifeq ($(OS),Windows_NT)
    # If building natively on Windows-on-ARM, PROCESSOR_ARCHITECTURE is often ARM64.
    # For cross builds from x86 Windows, explicitly set TARGET_OS=winarm64.
    ifeq ($(PROCESSOR_ARCHITECTURE),ARM64)
      TARGET_OS := winarm64
    else
      TARGET_OS := win32
    endif
  else
    UNAME_S := $(shell uname -s)
    UNAME_M := $(shell uname -m)

    ifeq ($(UNAME_S),Linux)
      # Auto-detect common Linux ARM machine types
      ifeq ($(UNAME_M),aarch64)
        TARGET_OS := linuxarm64
      else ifneq (,$(filter armv7l armv6l armhf,$(UNAME_M)))
        TARGET_OS := linuxarm
      else
        TARGET_OS := linux
      endif
    else
      $(error Unknown host OS; set TARGET_OS=win32/winarm64/linux/linuxarm/linuxarm64)
    endif
  endif
endif

# ----------------------------------------------------------------------------
# Common settings
# ----------------------------------------------------------------------------
# Vendored mbedTLS (third_party submodule) provides HTTPS probing.
MBEDTLS_DIR   = third_party/mbedtls
MBEDTLS_SRCS  = $(wildcard $(MBEDTLS_DIR)/library/*.c)
MBEDTLS_INC   = -isystem $(MBEDTLS_DIR)/include -isystem third_party
MBEDTLS_DEF   = -DMBEDTLS_USER_CONFIG_FILE='"mbedtls_pingdd_config.h"'

# Hooks for the quality targets (asan / coverage) to inject flags.
EXTRA_CFLAGS  ?=
EXTRA_LDFLAGS ?=

CFLAGS_COMMON = -W -Wall -Wextra -Werror -std=c99 -Isrc/lib -fno-omit-frame-pointer $(MBEDTLS_INC) $(MBEDTLS_DEF) $(EXTRA_CFLAGS)
SOURCES       = $(wildcard src/*.c)
HEADERS       = $(wildcard src/lib/*.h)

# Optional: allow disabling static builds if toolchain lacks static libs
STATIC ?= 1
ifeq ($(STATIC),1)
  CFLAGS_STATIC = -static
else
  CFLAGS_STATIC =
endif

# ----------------------------------------------------------------------------
# Per-OS configuration
# ----------------------------------------------------------------------------
CC      =
CFLAGS  =
LDFLAGS =
BINDIR  =
OBJDIR  =
EXEC    =
EXEC_SUFFIX ?=

RC      =
RCFLAGS =
RES     =

ifeq ($(TARGET_OS), win32)
    CC      = i686-w64-mingw32-gcc
    # Prefer a plain 'windres' (MSYS2); fall back to the cross-prefixed name
    # used by Fedora/Debian mingw packages.
    RC      := $(shell command -v windres 2>/dev/null || command -v i686-w64-mingw32-windres 2>/dev/null || echo windres)
    CFLAGS  = $(CFLAGS_COMMON) $(CFLAGS_STATIC)
    LDFLAGS = -liphlpapi -lws2_32 -lbcrypt
    BINDIR  = bin/win-x86
    OBJDIR  = obj/win-x86
    EXEC    = $(BINDIR)/pingdd.exe
    RES     = $(OBJDIR)/version.res.o
    RCFLAGS = -I.

else ifeq ($(TARGET_OS), winarm64)
    CC      = aarch64-w64-mingw32-gcc
    # llvm-mingw ships llvm-windres; some packagings use the prefixed name.
    RC      := $(shell command -v llvm-windres 2>/dev/null || command -v aarch64-w64-mingw32-windres 2>/dev/null || echo llvm-windres)
    CFLAGS  = $(CFLAGS_COMMON) $(CFLAGS_STATIC)
    LDFLAGS = -liphlpapi -lws2_32 -lbcrypt
    BINDIR  = bin/win-arm64
    OBJDIR  = obj/win-arm64
    EXEC    = $(BINDIR)/pingdd.exe
    RES     = $(OBJDIR)/version.res.o
    RCFLAGS = --target=aarch64-w64-mingw32 -I.

else ifeq ($(TARGET_OS), linux)
    CC      = gcc
    CFLAGS  = $(CFLAGS_COMMON) -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE $(CFLAGS_STATIC)
    LDFLAGS = -lm -lpthread
    BINDIR  = bin/linux
    OBJDIR  = obj/linux
    EXEC    = $(BINDIR)/pingdd$(EXEC_SUFFIX)

else ifeq ($(TARGET_OS), linuxarm)
    # Common cross compiler name for ARM 32-bit hard-float: arm-linux-gnueabihf-gcc. [web:384]
    CC      = arm-linux-gnueabihf-gcc
    CFLAGS  = $(CFLAGS_COMMON) -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE $(CFLAGS_STATIC)
    LDFLAGS = -lm -lpthread
    BINDIR  = bin/linux-arm
    OBJDIR  = obj/linux-arm
    EXEC    = $(BINDIR)/pingdd$(EXEC_SUFFIX)

else ifeq ($(TARGET_OS), linuxarm64)
    # Common cross compiler name for ARM64: aarch64-linux-gnu-gcc. [web:384]
    CC      = aarch64-linux-gnu-gcc
    CFLAGS  = $(CFLAGS_COMMON) -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE $(CFLAGS_STATIC)
    LDFLAGS = -lm -lpthread
    BINDIR  = bin/linux-arm64
    OBJDIR  = obj/linux-arm64
    EXEC    = $(BINDIR)/pingdd$(EXEC_SUFFIX)

else
    $(error Unknown TARGET_OS '$(TARGET_OS)' (use win32, winarm64, linux, linuxarm, linuxarm64))
endif

OBJECTS = $(SOURCES:src/%.c=$(OBJDIR)/%.o)

# mbedTLS objects compiled separately (relaxed warnings; it is third-party).
MBEDTLS_OBJS = $(MBEDTLS_SRCS:$(MBEDTLS_DIR)/library/%.c=$(OBJDIR)/mbedtls/%.o)
OBJECTS += $(MBEDTLS_OBJS)

ifneq (,$(filter win32 winarm64,$(TARGET_OS)))
  ifneq ($(RES),)
    OBJECTS += $(RES)
  endif
endif

# ----------------------------------------------------------------------------
# Portable commands (detect environment) - unchanged
# ----------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
  ifneq ($(findstring /,$(SHELL)),)
    RM      = rm -rf
    MKDIR_P = mkdir -p
  else
    RM      = rmdir /s /q 2>nul || del /s /q 2>nul
    MKDIR_P = if not exist "$(subst /,\,$(@))" mkdir "$(subst /,\,$(@))"
  endif
else
  RM      = rm -rf
  MKDIR_P = mkdir -p
endif

# ----------------------------------------------------------------------------
# Phony targets
# ----------------------------------------------------------------------------
.PHONY: all clean win32 winarm64 linux linuxarm linuxarm64 debug info help \
        test unit asan coverage analyze fuzz

all: $(EXEC)

win32:
	$(MAKE) TARGET_OS=win32

winarm64:
	$(MAKE) TARGET_OS=winarm64

linux:
	$(MAKE) TARGET_OS=linux

linuxarm:
	$(MAKE) TARGET_OS=linuxarm

linuxarm64:
	$(MAKE) TARGET_OS=linuxarm64

# ----------------------------------------------------------------------------
# Build rules - unchanged
# ----------------------------------------------------------------------------
$(BINDIR) $(OBJDIR):
	$(MKDIR_P) $@

$(EXEC): $(BINDIR) $(OBJDIR) $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(EXTRA_LDFLAGS) -o $@

ifneq (,$(filter win32 winarm64,$(TARGET_OS)))
$(RES): version.rc | $(OBJDIR)
	$(RC) $(RCFLAGS) -i $< -o $@
endif

$(OBJDIR)/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Third-party mbedTLS: compile with warnings off (-w); not our code to police.
$(OBJDIR)/mbedtls:
	$(MKDIR_P) $@

$(OBJDIR)/mbedtls/%.o: $(MBEDTLS_DIR)/library/%.c | $(OBJDIR)/mbedtls
	$(CC) -w -std=c99 $(CFLAGS_STATIC) $(EXTRA_CFLAGS) $(MBEDTLS_INC) $(MBEDTLS_DEF) -c $< -o $@

clean:
	$(RM) bin obj
	@echo "Cleaned bin/ and obj/"

debug: CFLAGS += -g
debug: all

# ----------------------------------------------------------------------------
# Quality targets: tests, sanitizers, coverage, static analysis, fuzzing
# ----------------------------------------------------------------------------

# Standalone unit tests for dependency-free pure modules.
unit:
	$(MKDIR_P) obj
	$(CC) -W -Wall -Wextra -std=c99 -Isrc/lib $(EXTRA_CFLAGS) \
	  tests/unit_stats.c src/stats.c -lm $(EXTRA_LDFLAGS) -o obj/unit_stats
	./obj/unit_stats

# Build, then run the functional harness and the unit tests.
test: linux unit
	bash tests/run_tests.sh

# AddressSanitizer + UBSan build (no -static; sanitizers need dynamic libs).
# Clean first so every object is rebuilt with instrumentation.
asan:
	$(MAKE) clean
	$(MAKE) TARGET_OS=linux STATIC=0 \
	  EXTRA_CFLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -g" \
	  EXTRA_LDFLAGS="-fsanitize=address,undefined"
	@echo "ASan/UBSan binary: bin/linux/pingdd"

# gcov coverage build, run the tests, then summarise per-source coverage.
coverage:
	$(MAKE) clean
	$(MAKE) TARGET_OS=linux STATIC=0 EXTRA_CFLAGS="--coverage -g" \
	  EXTRA_LDFLAGS="--coverage"
	-bash tests/run_tests.sh
	$(CC) -W -Wall -Wextra -std=c99 -Isrc/lib --coverage \
	  tests/unit_stats.c src/stats.c -lm -o obj/unit_stats_cov && ./obj/unit_stats_cov
	@echo "--- gcov (src) ---"
	gcov -o obj/linux $(SOURCES) 2>/dev/null | grep -A1 "File 'src/" || true

# Static analysis (no-op if cppcheck is not installed).
analyze:
	@command -v cppcheck >/dev/null 2>&1 || { echo "cppcheck not installed; skipping"; exit 0; }
	cppcheck --std=c99 --enable=warning,performance,portability --error-exitcode=1 \
	  --suppress=missingIncludeSystem -Isrc/lib src/

# libFuzzer harness over the URL parser (needs clang).
fuzz:
	@command -v clang >/dev/null 2>&1 || { echo "clang not installed; skipping fuzz"; exit 0; }
	$(MKDIR_P) obj/fuzz
	for f in $(MBEDTLS_SRCS); do \
	  clang -w -std=c99 $(MBEDTLS_INC) $(MBEDTLS_DEF) -fsanitize=fuzzer-no-link,address \
	    -c $$f -o obj/fuzz/$$(basename $$f .c).o; done
	clang -std=c99 -Isrc/lib $(MBEDTLS_INC) $(MBEDTLS_DEF) -DPINGDD_FUZZ_URL \
	  -fsanitize=fuzzer,address,undefined \
	  src/http.c obj/fuzz/*.o -o obj/fuzz/fuzz_url
	./obj/fuzz/fuzz_url -runs=100000 -max_len=2048

info:
	@echo "TARGET_OS = $(TARGET_OS)"
	@echo "CC        = $(CC)"
	@echo "Target    = $(EXEC)"

help:
	@echo "make                     # Auto-detect host"
	@echo "make win32               # Windows x86      -> bin/win-x86/pingdd.exe"
	@echo "make winarm64            # Windows ARM64    -> bin/win-arm64/pingdd.exe"
	@echo "make linux               # Linux native     -> bin/linux/pingdd"
	@echo "make linuxarm            # Linux ARM 32-bit -> bin/linux-arm/pingdd"
	@echo "make linuxarm64          # Linux ARM64      -> bin/linux-arm64/pingdd"
	@echo "make STATIC=0 <target>   # Disable -static if toolchain lacks static libs"
