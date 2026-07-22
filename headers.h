/* Common header for MinIRC. Handles both Win32 (winsock) and POSIX/Linux,
   and declares everything. */
#ifndef MINIRC_HEADERS_H
#define MINIRC_HEADERS_H

#ifdef _WIN32

/* We need at least WinXP-level APIs for getaddrinfo. */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#define SOCK_EWOULDBLOCK WSAEWOULDBLOCK

#else /* POSIX */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

/* Map the Win32 socket vocabulary onto POSIX so the rest of the code can
   stay platform-agnostic. */
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr    SOCKADDR;
typedef struct sockaddr   *LPSOCKADDR;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#define closesocket(s) close(s)
#define SOCK_EWOULDBLOCK EWOULDBLOCK

#endif /* _WIN32 */

/* Platform helpers, implemented per-OS in the .c files. */
int  net_init(void);
void net_cleanup(void);
int  sock_errno(void);
void msleep(int ms);
void kbd_init(void);
void kbd_restore(void);
int  kbd_kbhit(void);
int  kbd_getch(void);

SOCKET tcp_connect(char *hostname, int port);
int tcp_poll_line (SOCKET socketnr, char *buf, int maxsize, int stopchar, int stripIAC);
int tcp_sendchar (SOCKET socketnr, int character);
int tcp_send (SOCKET socketnr, char *buf, int bytes);
int tcp_recv (SOCKET socketnr, char *buf, int bytes);
int tcp_socketerror (SOCKET socketnr);
int tcp_socketopen(void);
int getkey (void);
int kbd_stuffbuf (char *buf, int maxchars);
void irc_login (SOCKET socketnr, char *argv[]);
void irc_parse(SOCKET socketnr, char *inbuf);
int irc_cmd_PING(SOCKET socketnr, char *inbuf);
int irc_cmd_fromuser(SOCKET socketnr, char *inbuf);
void from_latin (char *text);
void to_latin (char *text);
int irc_alias (char *buf);
void irc_alias_replace (char *buf, char *alias, char *replacestring);
void irc_CTCP_reply (SOCKET socketnr, char *nickname, char *message);
void kbd_showbuf (char *kbdbuf);
void kbd_clearline(void);
int  display_columns(void);
void version_string (char *text, int startup);
SOCKET identd_startup(void);
int identd_poll(SOCKET socketnr, char *nickname);
void trim(char *text);
void gettime(char *timestring);
void time2kbdbuf (char *kbdbuf);
void irc_msg_insert_colon (char *buf);
void irc_autoreply_off(void);
int fast_loop(void);
void set_priority(int priority);
int second_since_last(int init);

extern char IRC_ACTIVE_CHANNEL[600];
extern int CLIENT_SAYS_QUIT;

#define VERSION "0.1.6"
/* Fallback terminal width, used only when the runtime query fails. */
#define DISPLAY_ASSUME_COLUMNS 80

#endif /* MINIRC_HEADERS_H */
