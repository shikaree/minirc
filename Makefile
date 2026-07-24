# Makefile for MinIRC - builds on both MinGW-w64 (Windows) and POSIX (Linux).
#
# Build:    make          (use mingw32-make on Windows)
# 32-bit:   make win32    (32-bit Windows exe; needs an i686 MinGW-w64 - see CC32)
# Debug:    make DEBUG=1
# Clean:    make clean

CC     = gcc
CFLAGS = -Wall -O2

# Compiler for the 32-bit `win32` target. The stock 64-bit MinGW-w64
# toolchains are not multilib, so plain `-m32` cannot link; you need a
# separate i686 toolchain. Override CC32 if yours has a different name -
# e.g. inside an msys2 MINGW32 shell or a WinLibs i686 install it is just
# `gcc`:   mingw32-make win32 CC32=gcc
CC32   = i686-w64-mingw32-gcc

ifdef DEBUG
CFLAGS = -Wall -g -O0
endif

# Platform differences: Windows needs winsock, POSIX needs nothing extra.
ifeq ($(OS),Windows_NT)
  EXE    = MinIRC.exe
  LDLIBS = -lws2_32
  CLEAN  = cmd /c "del /q $(OBJS) $(EXE) $(EXE32) 2>nul"
else
  EXE    = minirc
  LDLIBS =
  CLEAN  = rm -f $(OBJS) $(EXE) $(EXE32)
endif

# The 32-bit build is always a Windows exe, whatever the build host.
EXE32  = MinIRC32.exe

OBJS = main.o irc.o tcp_socket.o keyboard.o identd.o
SRCS = $(OBJS:.o=.c)

all: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): headers.h

# 32-bit Windows build - runs on 32-bit *and* 64-bit Windows (WOW64).
# Compiled straight from source into a separate exe so it never links the
# 64-bit .o files a normal `make` leaves behind.
win32:
	$(CC32) $(CFLAGS) $(LDFLAGS) -o $(EXE32) $(SRCS) -lws2_32

clean:
	-$(CLEAN)

.PHONY: all win32 clean
