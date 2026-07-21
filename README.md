# MinIRC

A minimal console IRC client for Win32 and Linux by Andreas Westling.

This is really old code I made a refresh of in 2026.


## Building

### Windows (MinGW-w64)

Requires MinGW-w64 (e.g. WinLibs via `winget install BrechtSanders.WinLibs.POSIX.UCRT`):

```
mingw32-make            # release build -> MinIRC.exe
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
