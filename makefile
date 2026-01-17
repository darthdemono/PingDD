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
CFLAGS_COMMON = -W -Wall -Wextra -Werror -std=c99 -Isrc/lib -fno-omit-frame-pointer
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
    RC      = i686-w64-mingw32-windres
    CFLAGS  = $(CFLAGS_COMMON) $(CFLAGS_STATIC)
    LDFLAGS = -lws2_32
    BINDIR  = bin/win-x86
    OBJDIR  = obj/win-x86
    EXEC    = $(BINDIR)/pingdd.exe
    RES     = $(OBJDIR)/version.res.o
    RCFLAGS = -I.

else ifeq ($(TARGET_OS), winarm64)
    CC      = aarch64-w64-mingw32-gcc
    RC      = aarch64-w64-mingw32-windres
    CFLAGS  = $(CFLAGS_COMMON) $(CFLAGS_STATIC)
    LDFLAGS = -lws2_32
    BINDIR  = bin/win-arm64
    OBJDIR  = obj/win-arm64
    EXEC    = $(BINDIR)/pingdd.exe
    RES     = $(OBJDIR)/version.res.o
    RCFLAGS = -I.

else ifeq ($(TARGET_OS), linux)
    CC      = gcc
    CFLAGS  = $(CFLAGS_COMMON) -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE $(CFLAGS_STATIC)
    LDFLAGS =
    BINDIR  = bin/linux
    OBJDIR  = obj/linux
    EXEC    = $(BINDIR)/pingdd$(EXEC_SUFFIX)

else ifeq ($(TARGET_OS), linuxarm)
    # Common cross compiler name for ARM 32-bit hard-float: arm-linux-gnueabihf-gcc. [web:384]
    CC      = arm-linux-gnueabihf-gcc
    CFLAGS  = $(CFLAGS_COMMON) -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE $(CFLAGS_STATIC)
    LDFLAGS =
    BINDIR  = bin/linux-arm
    OBJDIR  = obj/linux-arm
    EXEC    = $(BINDIR)/pingdd$(EXEC_SUFFIX)

else ifeq ($(TARGET_OS), linuxarm64)
    # Common cross compiler name for ARM64: aarch64-linux-gnu-gcc. [web:384]
    CC      = aarch64-linux-gnu-gcc
    CFLAGS  = $(CFLAGS_COMMON) -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE $(CFLAGS_STATIC)
    LDFLAGS =
    BINDIR  = bin/linux-arm64
    OBJDIR  = obj/linux-arm64
    EXEC    = $(BINDIR)/pingdd$(EXEC_SUFFIX)

else
    $(error Unknown TARGET_OS '$(TARGET_OS)' (use win32, winarm64, linux, linuxarm, linuxarm64))
endif

OBJECTS = $(SOURCES:src/%.c=$(OBJDIR)/%.o)

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
.PHONY: all clean win32 winarm64 linux linuxarm linuxarm64 debug info help

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
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

ifneq (,$(filter win32 winarm64,$(TARGET_OS)))
$(RES): version.rc | $(OBJDIR)
	$(RC) $(RCFLAGS) -i $< -o $@
endif

$(OBJDIR)/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) bin obj
	@echo "Cleaned bin/ and obj/"

debug: CFLAGS += -g
debug: all

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
