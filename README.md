# MinIRC

A minimal console IRC client for Win32 and Linux by Andreas Westling.

This is really old code I made a refresh of in 2026.

## Download

Built automatically by GitHub Actions on every push to `main`, no install
needed — both link the system `msvcrt.dll`, so there is no runtime to install:

- [**MinIRC.exe**](https://github.com/shikaree/minirc/releases/latest/download/MinIRC.exe)
  — 64-bit, for 64-bit Windows XP and later.
- [**MinIRC32.exe**](https://github.com/shikaree/minirc/releases/latest/download/MinIRC32.exe)
  — 32-bit, runs on both 32- and 64-bit Windows (XP and later).

## Building

### Windows (MinGW-w64)

Requires MinGW-w64 (e.g. WinLibs via `winget install BrechtSanders.WinLibs.POSIX.MSVCRT`):

```
mingw32-make            # release build -> MinIRC.exe (64-bit)
mingw32-make win32      # 32-bit build -> MinIRC32.exe (needs an i686 toolchain)
mingw32-make DEBUG=1    # debug build (-g, no optimization)
mingw32-make clean
```

Or without make:

```
gcc -Wall -O2 -o MinIRC.exe main.c irc.c tcp_socket.c keyboard.c identd.c -lws2_32
```

### Linux / POSIX

Requires gcc (or clang) and make:

```
make                    # release build -> minirc
make DEBUG=1            # debug build (-g, no optimization)
make clean
```

Or without make:

```
gcc -Wall -O2 -o minirc main.c irc.c tcp_socket.c keyboard.c identd.c
```

Notes:

- The object files (`*.o`) are platform-specific. Run a `clean` when
  switching the same source tree between the Windows and Linux builds.
- `mingw32-make win32` builds a 32-bit `MinIRC32.exe` (runs on 32- *and*
  64-bit Windows via WOW64). The stock 64-bit MinGW-w64 is not multilib, so
  this needs a separate i686 toolchain; point `CC32` at it if it is not named
  `i686-w64-mingw32-gcc`, e.g. `mingw32-make win32 CC32=gcc` from an msys2
  MINGW32 shell or a WinLibs i686 install.
- The built-in IdentD responder (`-i`) binds port 113, which needs root on
  Linux. Without root, just start without `-i`.

## Usage

```
MinIRC [nickname] [server] <port> <-i>
```

- `port` defaults to 6667
- `-i` starts the built-in IdentD responder on port 113

Commands start with `/` (e.g. `/JOIN #channel`, `/QUIT`). Anything else is
sent to the active channel. Aliases: `/MSG`, `/QUERY`, `/ME`, `/CTCP`,
`/W` (names in active channel), `/AUTO <message>` (autoreply/away),
`#nick` sets the active target.
