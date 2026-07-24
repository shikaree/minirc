/* MinIRC main function. */

#include "headers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifndef _WIN32
#include <sys/resource.h>
#endif


int IRC_USE_IDENTD=0;
SOCKET socketnr;
SOCKET IDENTD_SOCKETNR=INVALID_SOCKET;
char IRC_ACTIVE_CHANNEL[600];
#ifdef _WIN32
HANDLE PROCESS_HANDLE;
#endif

int check_arguments(int argc, char *argv[], int *port);

int main(int argc, char *argv[])
{
 char kbdbuf[600]="[##:##:##]    ",inbuf[600],  gotline=0, *program_info;
 int port;
 size_t promptlen;

 /* We echo keystrokes ourselves and move the cursor with backspaces, so
    stdout must be unbuffered. Without this a Linux tty line-buffers output:
    typed characters stay invisible until a newline and the cursor desyncs
    so lines don't start at column 0. */
 setvbuf(stdout, NULL, _IONBF, 0);

 /* Show program info. This could be fun if out of memory. */
 program_info=(char *)malloc(256);
 version_string (program_info,1);
 printf("%s\n",program_info);
 free(program_info);

 /* Check all arguments. */
 if (!check_arguments(argc, argv, &port)) {
   printf("--- ERROR: Bad or too few arguments.\n\n");
   printf("Usage: MinIRC [nickname] [server] <port> <-i>\n");
   exit(1);
  }

 /* Init prompt. */
 time2kbdbuf (kbdbuf);

 /* Try to connect. */
 printf("*** Connecting to server %s at port %i...\n",argv[2],port);
 socketnr=tcp_connect(argv[2],port);

 if (socketnr==INVALID_SOCKET) {
   printf("*** ERROR: Unable to connect.\n");
   net_cleanup();
   exit(2);
  }

 /* StartUp IdentD */
 if (IRC_USE_IDENTD)
  IDENTD_SOCKETNR=identd_startup();

 /* Log in to server. */
 irc_login (socketnr, argv);

 /* Get current process handle (used for priority changes on Windows). */
#ifdef _WIN32
 PROCESS_HANDLE=GetCurrentProcess();
#endif

 /* Put the terminal into unbuffered/no-echo mode for our own line editing. */
 kbd_init();

 /* Initialize timer for priority change. */
 second_since_last(1);

 /* tcp_poll_line only appends, so start with a clean buffer. */
 inbuf[0]=0;
 inbuf[1]=0;

 for(;;){
  /* Poll IdentD. We have to close port 113 directly after...*/
  if (IRC_USE_IDENTD && IDENTD_SOCKETNR!=INVALID_SOCKET)
    if(identd_poll(IDENTD_SOCKETNR,argv[1])) {
      closesocket(IDENTD_SOCKETNR);
      IDENTD_SOCKETNR=INVALID_SOCKET;
     }

  /* Fast loop code - makes MinIRC use less CPU time. */
  while (!fast_loop());

  /* Did the connection close? */
  if (!tcp_socketopen()) {
    printf("\n*** Connection closed.\n");
    closesocket(socketnr);
    if (IDENTD_SOCKETNR!=INVALID_SOCKET)
     closesocket(IDENTD_SOCKETNR);
    net_cleanup();
    exit(0);
   }

  gotline=tcp_poll_line (socketnr, inbuf, 599, 10);

   if (gotline) {
     /* Clear the command line for server output. */
     kbd_clearline();
     /* Parse and print. */
     irc_parse(socketnr,inbuf);
     /* We have to update the kbdbuf now to restore what was previously there, */
     kbd_showbuf(kbdbuf);

     inbuf[0]=0;
     inbuf[1]=0;
    }

   /* Stuff da buf. Specs says max 512, we cut down a little just in case. */
   if (kbd_stuffbuf (kbdbuf, 480)) {
     /* First clear. */
     kbd_clearline();
     /* Print the whole buffer and then new line.
        (Can't use kbd_showbuf here. */
     printf("%s\n",kbdbuf);

     /* Remove prompt from buffer. The prompt is "[hh:mm:ss] " plus
        " #channel" when a channel is active. Overlapping strcpy is
        undefined behaviour, so memmove. */
     promptlen=11;
     if (strlen(IRC_ACTIVE_CHANNEL)>0)
      promptlen+=strlen(IRC_ACTIVE_CHANNEL)+1;
     memmove(kbdbuf,kbdbuf+promptlen,strlen(kbdbuf+promptlen)+1);

     /* Convert to LATIN-1 */
     to_latin(kbdbuf);

     /* If the alias is to a command to be send, then send it. */
     if (irc_alias(kbdbuf)) {
        tcp_send (socketnr,kbdbuf,strlen(kbdbuf));
        /* Send "\n". */
        tcp_sendchar (socketnr, 13);
        tcp_sendchar (socketnr, 10);
      }
     /* No need to redraw if we want to quit. */
     if (!CLIENT_SAYS_QUIT) {
       time2kbdbuf (kbdbuf);
       kbd_showbuf(kbdbuf);
      }
    }

  }

 return 0;
}



/* Checks command line arguments. */
int check_arguments(int argc, char *argv[], int *port) {
 int i;
 *port=6667;

 /* Too few arguments. */
 if (argc<3)
  return 0;

 /* Sanity check the lengths - these get copied into fixed buffers. */
 if (strlen(argv[1])>100 || strlen(argv[2])>200)
  return 0;

 if (argc==3)
   return 1;

  for (i=3;i<argc;i++)
   if (strcmp(argv[i],"-i")==0)
    IRC_USE_IDENTD=1;
   else
    if (atoi(argv[i])>0)
     *port=atoi(argv[i]);

 return 1;
}



/* Function to return a string containing program version and info. */
void version_string (char *text, int startup) {
 char fixdate[32];
 strcpy (text,"MinIRC ");
 strcat (text,VERSION);
 if (startup)
  strcat (text," by Andreas Westling.\n");
 else
  strcat (text," by Andreas Westling. ");

 strcat (text,"Compiled ");

 /* Fix date zero. */
 strcpy (fixdate, __DATE__);
 if (fixdate[4]==32)
  fixdate[4]='0';

 strcat (text,fixdate);
 strcat (text," at ");
 strcat (text,__TIME__);
#ifdef __GNUC__
 strcat (text," using GCC ");
 strcat (text,__VERSION__);
#else
 strcat (text," using a non-GCC compiler");
#endif
 strcat (text,".");
 return;
}



/* General trim function. */
void trim(char *text) {
 int len;

 /* Trim left. */
 while (text[0]==32 && strlen(text)>=2) {
   len=strlen(text);
   memmove(text,(char *)(text+1),strlen(text)-1);
   text[len-1]=0;
  }

 /* Just in case. */
 if (text[0]==32 && strlen(text)==1) {
   text[0]=0;
   text[1]=1;
  }
}



/* Returns a string with the local time. Portable across Win32 and POSIX. */
void gettime(char *timestring) {
 time_t t=time(NULL);
 struct tm *lt=localtime(&t);

 /* This 'might' bug out. */
 if (lt==NULL || strftime(timestring,9,"%H:%M:%S",lt)==0)
  strcpy(timestring,"##:##:##");

 return;
}



/* Copies prompt to kbdbuf. */
void time2kbdbuf (char *kbdbuf) {
 char now[256];
 char active_not_latin[600];

 /* To handle latin channel names. */
 strcpy(active_not_latin,IRC_ACTIVE_CHANNEL);
 from_latin(active_not_latin);

 /* Get current time. */
 gettime(now);

 /* Put prompt back. */
 strcpy(kbdbuf,"[");
 strcat(kbdbuf,now);
 if (strlen(active_not_latin)>0)
  strcat(kbdbuf," ");
 strcat(kbdbuf,active_not_latin);
 strcat(kbdbuf,"] ");
}



/* Portable millisecond sleep. */
void msleep(int ms) {
#ifdef _WIN32
 Sleep(ms);
#else
 usleep(ms*1000);
#endif
}



/* The fast loop. To be VERY optimized. */
int fast_loop(void) {
 fd_set readfds;
 struct timeval zero_timeout={0,0};
 int nfds;

 /* Check if input from socket waiting. Also watch the IdentD listener,
    or an incoming ident connection sits unanswered until the IRC server
    happens to send us something. */
 FD_ZERO(&readfds);
 FD_SET(socketnr,&readfds);
 nfds=(int)socketnr;
 if (IDENTD_SOCKETNR!=INVALID_SOCKET) {
   FD_SET(IDENTD_SOCKETNR,&readfds);
   if ((int)IDENTD_SOCKETNR>nfds)
    nfds=(int)IDENTD_SOCKETNR;
  }

 if (select (nfds+1,&readfds,NULL,NULL,&zero_timeout)!=0)
  return 1;

 if (kbd_kbhit()) {
   /* Set normal priority and timer. */
   set_priority(0);

   /* Reset the timer. */
   second_since_last(1);
   return 1;
  }

 /* Check if we want to change priority. */
 set_priority(1);

 /* Nothing to do. Take a short nap instead of burning 100% CPU. */
 msleep(10);
 return 0;

}



/* Function to set priority. 0 = normal else idle. */
void set_priority(int priority) {
 static int current_priority=-1;

 /* No need to do this many times... */
 if (priority==current_priority)
  return;

 /* Are we allowed...1 sek delay between changes...
    Applies only when idle priority is to be set. */
 if (!second_since_last(0) && priority)
  return;

#ifdef _WIN32
 if (priority)
  SetPriorityClass(PROCESS_HANDLE,IDLE_PRIORITY_CLASS);
 else
  SetPriorityClass(PROCESS_HANDLE,NORMAL_PRIORITY_CLASS);
#else
 /* POSIX: nice level 10 when idle, back to 0 when active. */
 setpriority(PRIO_PROCESS,0,priority?10:0);
#endif

 current_priority=priority;

 return;
}



/* Function to see if a second has elapsed. */
int second_since_last(int init) {
 static unsigned int last_second;
 int this_second;
 unsigned int second;
 char now[256];

 /* Already inited? */
 if (init!=0 && last_second==255)
  return 0;

 /* Request for init? */
 if (init)
  last_second=255;

 /* Get the seconds parametar. */
 gettime(now);
 second=atoi((char *)(now+6));

 if (last_second==255)
  last_second=second;

 /* Handle wrapping. Not very well I might add... */
 this_second=second;
 if (last_second>this_second)
  this_second=60+this_second;

 /* Ok, at least 1 sek has elapsed. */
 if (this_second>last_second+8) {
   last_second=second;
   return 1;
  }

 return 0;
}
