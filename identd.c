#include "headers.h"
#include <stdio.h>
#include <string.h>
/* MinIRC IdentD Server. */

/* Main startup function. Assumes winsock has been initialized.
   Returns INVALID_SOCKET if failed, the listening socket if successfull. */
SOCKET identd_startup(void) {
 SOCKADDR_IN sin;
 SOCKET socketnr;

 /* Initialize the socket. */
 socketnr=socket(AF_INET, SOCK_STREAM,0);

 /* Did we fail in doing that? */
 if (socketnr==INVALID_SOCKET) {
   printf("*** IdentD ERROR: Unable to initialize socket.\n");
   return INVALID_SOCKET;
  }

 /* Bind to the socket. */
 memset(&sin,0,sizeof(sin));
 sin.sin_family = AF_INET;
 sin.sin_addr.s_addr = INADDR_ANY;
 sin.sin_port = htons(113);

 if (bind(socketnr,(LPSOCKADDR)&sin,sizeof(sin))!=0) {
   printf("*** IdentD ERROR: Could not bind to port 113.\n");
   closesocket(socketnr);
   return INVALID_SOCKET;
  }

 /* Listen once here instead of over and over in identd_poll. */
 if (listen(socketnr,5)!=0) {
   printf("*** IdentD ERROR: Could not listen on socket.\n");
   closesocket(socketnr);
   return INVALID_SOCKET;
  }

 return socketnr;
}



/* The main server function. Accepts incoming connections and replies.
   Returns 1 when a reply has been sent (so we are done), otherwise 0. */
int identd_poll(SOCKET socketnr, char *nickname) {
 SOCKET socketnr_in;
 socklen_t sin_size;
 int i;
 int bytes;
 char buf[256],outbuf[512];
 SOCKADDR sin;
 fd_set readfds;
 struct timeval zero_timeout={0,0};
 struct timeval read_timeout={2,0};

 /* Reset buffer. */
 buf[0]=0;
 outbuf[0]=0;

 /* Only accept when a connection is actually waiting - a bare accept()
    blocks and would hang the whole client until someone connects. */
 FD_ZERO(&readfds);
 FD_SET(socketnr,&readfds);
 if (select((int)socketnr+1,&readfds,NULL,NULL,&zero_timeout)!=1)
  return 0;

 /* Accept incoming. */
 sin_size=sizeof(sin);
 socketnr_in=accept(socketnr, &sin,&sin_size);

 if (socketnr_in==INVALID_SOCKET)
  return 0;

 printf("*** IdentD connection established.\n");

 /* Give the server a couple of seconds to send its question. */
 FD_ZERO(&readfds);
 FD_SET(socketnr_in,&readfds);
 if (select((int)socketnr_in+1,&readfds,NULL,NULL,&read_timeout)!=1) {
   printf("*** IdentD connection timed out.\n");
   closesocket(socketnr_in);
   return 0;
  }

 /* Receive data from socket. */
 bytes=recv (socketnr_in, buf, 254,0);

 /* Oups...he close too early. */
 if (bytes==SOCKET_ERROR || bytes==0) {
   printf("*** IdentD connection closed prematurely.\n");
   closesocket(socketnr_in);
   return 0;
  }

 /* Terminate the buffer, and remove \r\n (just in case). */
 buf[bytes]=0;
 for (i=0;buf[i]!=0;i++)
  if (buf[i]==13 || buf[i]==10) {
    buf[i]=0;
    break;
   }

 /* Make the response and send. */
 strcpy(outbuf,buf);
 strcat (outbuf, " : USERID : UNIX : ");
 strcat (outbuf, nickname);
 strcat (outbuf,"\r\n");
 send (socketnr_in,outbuf, (int)strlen(outbuf),0);

 /* Close the socket properly this time. */
 closesocket(socketnr_in);

 printf("*** IdentD session closed.\n");
 return 1;
}
