/* Socket functions for TCPlib with its own buffering. All system buffering
   is ignored. */
#include "headers.h"
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <signal.h>
#endif

static int TRANSMIT_ERROR_CODE=0;
static int CONNECTION_CLOSED=0;

/* Bring the network stack up. On Windows that means WSAStartup; on POSIX
   the only thing we need is to stop send() on a dead socket from killing us
   with SIGPIPE. */
int net_init(void) {
#ifdef _WIN32
 WSADATA wsaData;
 return WSAStartup(MAKEWORD(2,2),&wsaData);
#else
 signal(SIGPIPE,SIG_IGN);
 return 0;
#endif
}

/* Tear the network stack down. No-op on POSIX. */
void net_cleanup(void) {
#ifdef _WIN32
 WSACleanup();
#endif
}

/* Last socket error, the portable way. */
int sock_errno(void) {
#ifdef _WIN32
 return WSAGetLastError();
#else
 return errno;
#endif
}

/* Function to make a TCP connection to a hostname/ip at a specified port.
   Returns the socket or INVALID_SOCKET if error. */
SOCKET tcp_connect(char *hostname, int port)
{
 struct addrinfo hints,*addresses,*address;
 SOCKET socketnr=INVALID_SOCKET;
 char portstring[16];

 /* Bring up the network stack (WSAStartup on Windows, SIGPIPE guard on POSIX). */
 if (net_init()!=0)
  return INVALID_SOCKET;

 /* Look up the hostname. getaddrinfo may give us several addresses. */
 memset(&hints,0,sizeof(hints));
 hints.ai_family=AF_INET;
 hints.ai_socktype=SOCK_STREAM;
 hints.ai_protocol=IPPROTO_TCP;
 sprintf(portstring,"%d",port);

 if (getaddrinfo(hostname,portstring,&hints,&addresses)!=0)
  return INVALID_SOCKET;

 /* Try every address we got until one accepts the connection. */
 for (address=addresses;address!=NULL;address=address->ai_next) {
   socketnr=socket(address->ai_family,address->ai_socktype,address->ai_protocol);
   if (socketnr==INVALID_SOCKET)
    continue;

   if (connect(socketnr,address->ai_addr,(int)address->ai_addrlen)==0)
    break;

   closesocket(socketnr);
   socketnr=INVALID_SOCKET;
  }

 freeaddrinfo(addresses);
 return socketnr;
}



/* This function reads from a socket. When the end of line char or
   the maxsize is reached the function returns 1. buf MUST be an
   \0-terminated array!
   The function only adds to buf. When returnvalue 1 is returned from
   the function, the user must set buf[0]=0 or tcp_poll_line
   will just continue adding to the buffer.
   You cannot use any other receive function when using stripIAC, or
   you'll loose bytes.

   With stripIAC set the reader understands the full TELNET framing an IRC
   server may interleave with the data (RFC 854), not just fixed 3-byte
   commands: WILL/WONT/DO/DONT + option, the stand-alone commands, IAC IAC
   for a literal 0xFF, and IAC SB ... IAC SE subnegotiations. A lone 0xFF
   that is not followed by a command byte is treated as data, so a stray
   0xFF in a message no longer eats the bytes after it. */
int tcp_poll_line (SOCKET socketnr, char *buf, int maxsize, int stopchar, int stripIAC) {
 int len;
 unsigned char receivebuf[2];
 unsigned char c;
 /* TELNET IAC parser state, kept across calls (one byte per call):
    0 normal, 1 just saw IAC, 2 expecting an option byte to discard,
    3 inside a subnegotiation, 4 inside a subnegotiation having seen IAC. */
 static int iac_state=0;

 /* Get data from network buffer. */
 if (!tcp_recv(socketnr,(char *)receivebuf,1))
  return 0;    /* No data, goodbye. (shouldn't happen, since we have select) */

 c=receivebuf[0];

 if (stripIAC) {
   switch (iac_state) {

    case 1:  /* previous byte was IAC (255); c says what kind of command */
     if (c==250) {                 /* SB - a subnegotiation begins */
       iac_state=3;
       return 0;
      }
     if (c>=251 && c<=254) {       /* WILL/WONT/DO/DONT - one option follows */
       iac_state=2;
       return 0;
      }
     if (c>=240 && c<=249) {       /* stand-alone two-byte command */
       iac_state=0;
       return 0;
      }
     /* c==255 is a literal 0xFF (IAC IAC); anything else means the IAC was
        not a real command, so treat that 0xFF as data too. Emit the pending
        0xFF, then let c fall through to be stored (unless it was the second
        IAC, which the emitted byte already represents). */
     iac_state=0;
     len=strlen(buf);
     if (len+1<maxsize) {
       buf[len]=255;
       buf[len+1]=0;
      }
     if (c==255)
      return 0;
     break;

    case 2:  /* the option byte after WILL/WONT/DO/DONT - discard it */
     iac_state=0;
     return 0;

    case 3:  /* inside a subnegotiation - swallow until IAC SE */
     if (c==255)
      iac_state=4;
     return 0;

    case 4:  /* inside a subnegotiation and just saw IAC */
     if (c==240)              /* SE - subnegotiation ends */
      iac_state=0;
     else if (c!=255)         /* some other command, keep swallowing */
      iac_state=3;
     return 0;

    default: /* iac_state 0 - normal data */
     if (c==255) {
       iac_state=1;
       return 0;
      }
     break;
   }
  }

 /* Paste in to buffer. */
 len=strlen(buf);
 buf[len]=c;
 buf[len+1]=0;

 /* Check if we reached end of buffer. */
 if (len+1>=maxsize)
  return 1;

 /* Check if we received the stopchar. */
 if (c==stopchar)
  return 1;

 return 0;
}



/* Function to send an integer as a character to the socket
   Be careful, don't send \0!
   Returns 1 if successfull. */
int tcp_sendchar (SOCKET socketnr, int character) {
 char buf[2];

 buf[0]=character;
 return tcp_send (socketnr,buf,1);
}


/* Like send(), but does select() before to prevent hangups. */
int tcp_send (SOCKET socketnr, char *buf, int bytes) {
 struct timeval zero_timeout={0,0};
 fd_set writefds;
 int bytes_sent, selected;

 /* Check if we can send. */
 FD_ZERO(&writefds);
 FD_SET(socketnr,&writefds);

 selected=select ((int)socketnr+1,NULL,&writefds,NULL,&zero_timeout);
 if (selected==SOCKET_ERROR) {
   TRANSMIT_ERROR_CODE=sock_errno();
   return 0;
  }
 if (selected==0)
  return 0;

 bytes_sent=send(socketnr, buf, bytes,0);

 if (bytes_sent==SOCKET_ERROR) {
   /* EWOULDBLOCK just means "not right now", not a dead socket. */
   if (sock_errno()!=SOCK_EWOULDBLOCK)
    TRANSMIT_ERROR_CODE=sock_errno();
   return 0;
  }

 return bytes_sent;
}



/* Like recv(), but does select() before to prevent hangups. */
int tcp_recv (SOCKET socketnr, char *buf, int bytes) {
 struct timeval zero_timeout={0,0};
 fd_set readfds;
 int bytes_received, selected;

 /* Check if we have more data waiting. */
 FD_ZERO(&readfds);
 FD_SET(socketnr,&readfds);

 selected=select ((int)socketnr+1,&readfds,NULL,NULL,&zero_timeout);
 if (selected==SOCKET_ERROR) {
   TRANSMIT_ERROR_CODE=sock_errno();
   return 0;
  }
 if (selected==0)
  return 0;

 bytes_received=recv(socketnr, buf, bytes,0);

 /* recv() returning 0 means the other side closed the connection. */
 if (bytes_received==0) {
   CONNECTION_CLOSED=1;
   return 0;
  }

 if (bytes_received==SOCKET_ERROR) {
   /* EWOULDBLOCK just means "not right now", not a dead socket. */
   if (sock_errno()!=SOCK_EWOULDBLOCK)
    TRANSMIT_ERROR_CODE=sock_errno();
   return 0;
  }

 return bytes_received;
}



/* Function to check wether a socket has error or not. */
int tcp_socketerror (SOCKET socketnr) {
 struct timeval zero_timeout={0,0};
 fd_set readfds;

 /* Check if we can select() the socket. */
 FD_ZERO(&readfds);
 FD_SET(socketnr,&readfds);

 if (select ((int)socketnr+1,&readfds,NULL,NULL,&zero_timeout)!=SOCKET_ERROR)
  return 0;

 return sock_errno();
}


/* Function to see if the connection is still alive. recv/send above set
   flags when the connection actually dies. (The old check on the global
   WSAGetLastError() tripped on stale errors from unrelated calls and
   never noticed a graceful close at all.) */
int tcp_socketopen(void) {
 if (TRANSMIT_ERROR_CODE!=0 || CONNECTION_CLOSED || CLIENT_SAYS_QUIT>0)
  return 0;
 else
  return 1;
}
