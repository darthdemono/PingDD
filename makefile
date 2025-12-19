# ULTRA-SIMPLE Cross-Platform Makefile for PingDD - FIXED
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Isrc/lib -static
LDFLAGS = -lws2_32

# Source files
SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:src/%.c=obj/%.o)
EXEC = bin/pingdd.exe
RC_OBJ = obj/version.res

# Headers
HEADERS = $(wildcard src/lib/*.h)

.PHONY: all clean windows linux debug info help

all: $(EXEC)

# Link - FIXED spacing/tabs
$(EXEC): bin $(OBJECTS) $(RC_OBJ)
	$(CC) $(OBJECTS) $(RC_OBJ) $(LDFLAGS) -o $@

# Compile - FIXED spacing/tabs
obj/%.o: src/%.c $(HEADERS) | obj
	$(CC) $(CFLAGS) -c $< -o $@

# Resource - FIXED windres syntax
obj/version.res: version.rc | obj
	windres -O coff version.rc -o $@

# Directories - FIXED spacing
bin obj:
	-mkdir $@

clean:
	-rmdir /s /q bin 2>nul
	-rmdir /s /q obj 2>nul

# Linux version
linux:
	$(MAKE) LDFLAGS='' EXEC='bin_linux/pingdd' RC_OBJ=''

# Debug
debug: CFLAGS += -g
debug: all

# Info - FIXED spacing
info:
	@echo "Sources: $(notdir $(SOURCES))"
	@echo "Objects: $(notdir $(OBJECTS))"
	@echo "Target: $(EXEC)"

# Help - FIXED spacing
help:
	@echo "make         # Windows (bin/pingdd.exe)"
	@echo "make linux   # Linux (bin_linux/pingdd)"
	@echo "make clean   # Clean"
	@echo "make debug   # Debug build"
	@echo "make info    # Show build info"
	@echo "make help    # This help"
