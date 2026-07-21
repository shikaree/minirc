# Makefile for MinIRC - builds on both MinGW-w64 (Windows) and POSIX (Linux).
#
# Build:    make          (use mingw32-make on Windows)
# Debug:    make DEBUG=1
# Clean:    make clean

CC     = gcc
CFLAGS = -Wall -O2

ifdef DEBUG
CFLAGS = -Wall -g -O0
endif

# Platform differences: Windows needs winsock, POSIX needs nothing extra.
ifeq ($(OS),Windows_NT)
  EXE    = MinIRC.exe
  LDLIBS = -lws2_32
  CLEAN  = cmd /c "del /q $(OBJS) $(EXE) 2>nul"
else
  EXE    = minirc
  LDLIBS =
  CLEAN  = rm -f $(OBJS) $(EXE)
endif

OBJS = main.o irc.o tcp_socket.o keyboard.o identd.o

all: $(EXE)

$(EXE): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): headers.h

clean:
	-$(CLEAN)

.PHONY: all clean
