# ============================================================================
# Universal Makefile for PingDD (Windows 32-bit, Linux)
#
# Outputs:
#   Windows 32-bit : bin/win/x86/pingdd.exe   (TARGET_OS=win32)
#   Linux          : bin/linux/pingdd         (TARGET_OS=linux)
#
# Default TARGET_OS:
#   - On Windows hosts: win32
#   - On Linux hosts  : linux
# ============================================================================

# ----------------------------------------------------------------------------
# Target OS selection
# ----------------------------------------------------------------------------
TARGET_OS ?= auto
TARGET_OS := $(strip $(TARGET_OS))

ifeq ($(TARGET_OS), auto)
  ifeq ($(OS),Windows_NT)
    TARGET_OS := win32
  else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
      TARGET_OS := linux
    else
      $(error Unknown host OS; set TARGET_OS=win32/linux)
    endif
  endif
endif

TARGET_OS := $(strip $(TARGET_OS))

# ----------------------------------------------------------------------------
# Common settings
# ----------------------------------------------------------------------------
CFLAGS_COMMON = -Wall -Wextra -std=c99 -Isrc/lib
SOURCES       = $(wildcard src/*.c)
HEADERS       = $(wildcard src/lib/*.h)

# Defaults (overridden per OS)
CC      =
CFLAGS  =
LDFLAGS =
BINDIR  =
OBJDIR  =
EXEC    =
RC_OBJ  =

# ----------------------------------------------------------------------------
# Per-OS configuration
# ----------------------------------------------------------------------------
ifeq ($(TARGET_OS), win32)
    CC      = i686-w64-mingw32-gcc
    CFLAGS  = $(CFLAGS_COMMON) -static
    LDFLAGS = -lws2_32
    BINDIR  = bin/win/x86
    OBJDIR  = obj/win/x86
    EXEC    = $(BINDIR)/pingdd.exe
    RC_OBJ  = $(OBJDIR)/version.res
else ifeq ($(TARGET_OS), linux)
    # Linux native
    CC      = gcc
    CFLAGS  = $(CFLAGS_COMMON)
    LDFLAGS =
    BINDIR  = bin/linux
    OBJDIR  = obj/linux
    EXEC    = $(BINDIR)/pingdd
    RC_OBJ  =
else
    $(error Unknown TARGET_OS '$(TARGET_OS)' (use win32, linux))
endif

OBJECTS = $(SOURCES:src/%.c=$(OBJDIR)/%.o)

# ----------------------------------------------------------------------------
# Phony targets
# ----------------------------------------------------------------------------
.PHONY: all clean win32 linux debug info help

# Default build
all: $(EXEC)

# OS shortcuts
win32:
	$(MAKE) TARGET_OS=win32

linux:
	$(MAKE) TARGET_OS=linux

# ----------------------------------------------------------------------------
# Build rules
# ----------------------------------------------------------------------------

# Link
$(EXEC): $(BINDIR) $(OBJDIR) $(OBJECTS) $(RC_OBJ)
	$(CC) $(OBJECTS) $(RC_OBJ) $(LDFLAGS) -o $@

# Compile
$(OBJDIR)/%.o: src/%.c $(HEADERS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Windows resource (only used when RC_OBJ is non-empty)
$(OBJDIR)/version.res: version.rc | $(OBJDIR)
	windres -O coff version.rc -o $@

# Directories (MSYS2/Linux/macOS; used on CI and in MSYS shells)
$(BINDIR) $(OBJDIR):
	mkdir -p $@

# ----------------------------------------------------------------------------
# Utility targets
# ----------------------------------------------------------------------------

clean:
	rm -rf bin obj

debug: CFLAGS += -g
debug: all

info:
	@echo "TARGET_OS = $(TARGET_OS)"
	@echo "CC        = $(CC)"
	@echo "Sources   = $(notdir $(SOURCES))"
	@echo "Objects   = $(notdir $(OBJECTS))"
	@echo "Target    = $(EXEC)"

help:
	@echo "make               # Auto-detect host (Windows->win32, Linux->linux)"
	@echo "make win32         # Windows 32-bit  -> bin/win/x86/pingdd.exe"
	@echo "make linux         # Linux native    -> bin/linux/pingdd"
	@echo "make clean         # Remove bin/ and obj/"
	@echo "make debug         # Build with debug info"
	@echo "make info          # Show build configuration"
