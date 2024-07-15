# Compiler options
CC := g++
RC := windres
CFLAGS := -DNAME="\"PingDD\"" -DVERSION="\"1.0.0\"" -DAUTHOR="\"Jubair Hasan (Joy)\""
LDFLAGS := -lws2_32

# Directories
SRCDIR := ./src
BINDIR_X64 := ./bin/x64
OBJDIR_X64 := ./obj/x64

# Source files
SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS_X64 := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR_X64)/%.o,$(SRCS))
RC_FILE := version.rc
RC_OBJ_X64 := $(OBJDIR_X64)/version.res

# Executable
TARGET_X64 := $(BINDIR_X64)/pingdd

.PHONY: all clean

all: $(TARGET_X64)

# Build rules for x64 target
$(TARGET_X64): $(OBJS_X64) $(RC_OBJ_X64) | $(BINDIR_X64)
	$(CC) -m64 $^ -o $@ $(LDFLAGS)

# Object file compilation rules for x64
$(OBJDIR_X64)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR_X64)
	$(CC) $(CFLAGS) -m64 -c $< -o $@

# Resource compilation rule for x64
$(RC_OBJ_X64): $(RC_FILE) | $(OBJDIR_X64)
	$(RC) -O coff $< -o $@

# Directory creation targets (using PowerShell on Windows)
$(BINDIR_X64):
	powershell -Command "New-Item -ItemType Directory -Force -Path $(BINDIR_X64)"

$(OBJDIR_X64):
	powershell -Command "New-Item -ItemType Directory -Force -Path $(OBJDIR_X64)"

# Clean target
clean:
	rm -rf $(BINDIR_X64) $(OBJDIR_X64)
