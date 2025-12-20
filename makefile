# ============================================================================
# Universal Makefile for PingDD (Windows 32-bit, Linux)
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

# ----------------------------------------------------------------------------
# Common settings
# ----------------------------------------------------------------------------
CFLAGS_COMMON = -W -Wall -Wextra -Werror -std=c99 -Isrc/lib -fno-omit-frame-pointer
SOURCES       = $(wildcard src/*.c)
HEADERS       = $(wildcard src/lib/*.h)

# Defaults (overridden per OS)
CC      =
CFLAGS  =
LDFLAGS =
BINDIR  =
OBJDIR  =
EXEC    =

# ----------------------------------------------------------------------------
# Per-OS configuration
# ----------------------------------------------------------------------------
ifeq ($(TARGET_OS), win32)
    CC      = i686-w64-mingw32-gcc
    CFLAGS  = $(CFLAGS_COMMON) -static
    LDFLAGS = -lws2_32
    BINDIR  = bin/win
    OBJDIR  = obj/win
    EXEC    = $(BINDIR)/pingdd.exe
else ifeq ($(TARGET_OS), linux)
    CC      = gcc
    CFLAGS  = $(CFLAGS_COMMON) -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE
    LDFLAGS =
    BINDIR  = bin/linux
    OBJDIR  = obj/linux
    EXEC    = $(BINDIR)/pingdd
else
    $(error Unknown TARGET_OS '$(TARGET_OS)' (use win32, linux))
endif

OBJECTS = $(SOURCES:src/%.c=$(OBJDIR)/%.o)

# ----------------------------------------------------------------------------
# Portable commands (detect environment)
# ----------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
  ifneq ($(findstring /,$(SHELL)),)
    # MSYS2/Git Bash (has Unix tools)
    RM      = rm -rf
    MKDIR_P = mkdir -p
  else
    # Pure Windows CMD
    RM      = rmdir /s /q 2>nul || del /s /q 2>nul
    MKDIR_P = if not exist "$(subst /,\,$(@))" mkdir "$(subst /,\,$(@))"
  endif
else
  # Linux/Unix
  RM      = rm -rf
  MKDIR_P = mkdir -p
endif

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
# Build rules (SIMPLIFIED - no complex conditionals in rules)
# ----------------------------------------------------------------------------

# Ensure directories exist (SIMPLE portable rule)
$(BINDIR) $(OBJDIR):
	$(MKDIR_P) $@

# Link
$(EXEC): $(BINDIR) $(OBJDIR) $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Compile
$(OBJDIR)/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# ----------------------------------------------------------------------------
# Utility targets
# ----------------------------------------------------------------------------
clean:
	$(RM) bin obj
	@echo "Cleaned bin/ and obj/"

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
	@echo "make win32         # Windows 32-bit  -> bin/win/pingdd.exe"
	@echo "make linux         # Linux native    -> bin/linux/pingdd"
	@echo "make clean         # Remove bin/ and obj/"
	@echo "make debug         # Build with debug info"
	@echo "make info          # Show build configuration"
