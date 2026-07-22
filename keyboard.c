/* Keyboard routines for TCPlib. Small and easy. */
#include "headers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32

#include <conio.h>

/* On Windows conio already gives us unbuffered, no-echo keyboard input. */
void kbd_init(void)    {}
void kbd_restore(void) {}
int  kbd_kbhit(void)   { return kbhit(); }
int  kbd_getch(void)   { return getch(); }

/* Current console width in columns. */
int display_columns(void) {
 CONSOLE_SCREEN_BUFFER_INFO csbi;
 if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&csbi)) {
   int w=csbi.srWindow.Right-csbi.srWindow.Left+1;
   if (w>0)
    return w;
  }
 return DISPLAY_ASSUME_COLUMNS;
}

#else /* POSIX */

#include <termios.h>
#include <sys/ioctl.h>

static struct termios KBD_SAVED_TERM;
static int KBD_RAW=0;

/* Put the terminal back the way we found it. Registered with atexit so every
   exit path restores it. */
void kbd_restore(void) {
 if (KBD_RAW) {
   tcsetattr(STDIN_FILENO,TCSANOW,&KBD_SAVED_TERM);
   KBD_RAW=0;
  }
}

/* Switch stdin to unbuffered, no-echo (cbreak) mode so we can read keys one
   at a time - the POSIX equivalent of what conio gives us for free. */
void kbd_init(void) {
 struct termios t;

 if (!isatty(STDIN_FILENO))
  return;

 tcgetattr(STDIN_FILENO,&KBD_SAVED_TERM);
 t=KBD_SAVED_TERM;
 t.c_lflag &= ~(ICANON|ECHO);        /* no line buffering, no echo */
 t.c_iflag &= ~(ICRNL|INLCR|IGNCR);  /* keep Enter as CR (13), like Windows */
 t.c_cc[VMIN]=1;
 t.c_cc[VTIME]=0;
 tcsetattr(STDIN_FILENO,TCSANOW,&t);
 KBD_RAW=1;
 atexit(kbd_restore);
}

/* Non-blocking "is a key waiting?" via select on stdin. */
int kbd_kbhit(void) {
 fd_set readfds;
 struct timeval zero_timeout={0,0};

 FD_ZERO(&readfds);
 FD_SET(STDIN_FILENO,&readfds);
 return select(STDIN_FILENO+1,&readfds,NULL,NULL,&zero_timeout)>0;
}

/* Read one key. Normalizes to the codes the rest of the code expects:
   DEL becomes backspace, and ANSI escape sequences (arrow keys etc.) are
   swallowed and reported as a bare ESC, which getkey() ignores. */
int kbd_getch(void) {
 unsigned char c;

 if (read(STDIN_FILENO,&c,1)!=1)
  return 0;

 if (c==127)          /* DEL -> backspace */
  return 8;

 if (c==27 && kbd_kbhit()) {
   unsigned char d;
   if (read(STDIN_FILENO,&d,1)==1 && (d=='[' || d=='O'))
    /* Consume up to and including the final byte of the CSI/SS3 sequence. */
    while (kbd_kbhit()) {
      unsigned char e;
      if (read(STDIN_FILENO,&e,1)!=1)
       break;
      if (e>=0x40 && e<=0x7E)
       break;
     }
   return 27;
  }

 return c;
}

/* Current terminal width in columns, via TIOCGWINSZ. */
int display_columns(void) {
 struct winsize ws;
 if (ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0 && ws.ws_col>0)
  return ws.ws_col;
 return DISPLAY_ASSUME_COLUMNS;
}

#endif /* _WIN32 */

char KBD_LAST_COMMAND[600];

/* Reads a key from keyboard buffer without requiering return. */
int getkey (void) {
 static int highkey_is_comming=0;
 int i,j=0;

 i=kbd_kbhit();
 if (i>0)
  j=kbd_getch();
 else
  return 0;

 /* Extended keys arrive as two codes, prefixed with 0 or 224. */
 if (j==0 || j==224) {
   highkey_is_comming=1;
   return 0;
  }

 if (highkey_is_comming) {
   j=j*100;
   highkey_is_comming=0;
  }

 /* Ignore 14>31 */
 if (j>=14 && j<=31)
  return 0;

 return j;
}



/* Fills keyboard buffer ut to maxchars characters.
   Reset buffer by setting buf[0]=0 and buf[1]=1.
   Returns 1 when enter has been presset. */
int kbd_stuffbuf (char *buf, int maxchars) {
 int i,len;

 i=getkey();

 if (i==0)
  return 0;

 /* Key up. - Disabled for now. */
 /*
 if (i==7200) {
   if (strlen(KBD_LAST_COMMAND)>0)
    kbd_clearline();
    strcpy(buf,KBD_LAST_COMMAND);
    kbd_showbuf(buf);
   return 0;
  }
  */
 /* No more function keys, please. */
 if (i>255)
  return 0;

 if (i==13) {
   strcpy(KBD_LAST_COMMAND,buf);
   return 1;
  }

 /* We MUST handle backspace! */
 if (i==8) {
  /* Length if IRC_ACTIVE_CHANNEL always 1 too low. */
  len=strlen(IRC_ACTIVE_CHANNEL);
  if (len>0)
   len++;

  if ((int)strlen(buf)>len+11) {
    /* Cut the buffer by one. */
    buf[strlen(buf)-1]=0;
    /* Are we over the edge? */
    if ((int)strlen(buf)>=display_columns()-2) {
      kbd_clearline();
      kbd_showbuf(buf);
     }
    /* We are not over the edge, just erase one character from screen. */
    else
     printf("%c %c",i,i);

    return 0;
   }
  else
   return 0;
 }

 /* Not backspace, are we over maxchars? */
 if ((int)strlen(buf)>=maxchars)
  return 0;

 /* Resize buf by 1. */
 len=strlen(buf);
 buf[len]=i;
 buf[len+1]=0;

 /* Are we over the edge? */
 if ((int)strlen(buf)>=display_columns()-1) {
   kbd_clearline();
   kbd_showbuf(buf);
  }
 /* No we aren't, just print the character. */
 else
  printf("%c",i);

 return 0;
}



/* Function to clear command line. */
void kbd_clearline(void) {
#ifdef _WIN32
 /* The Windows console has no auto-left-margin, so the classic
    backspace/space/backspace dance is safe here. */
 int i,cols=display_columns();

 for (i=0;i<cols-1;i++)
  printf("%c",8);
 for (i=0;i<cols-1;i++)
  printf("%c",32);
 for (i=0;i<cols-1;i++)
  printf("%c",8);
#else
 /* Return to column 0 and erase to end of line with ANSI (VT100 EL). Robust
    across xterm, PuTTY, screen and tmux. The old backspace+space method broke
    on terminals that have the auto-left-margin (bw) capability - GNU screen
    among them - where the surplus backspaces wrap up into previous lines and
    clobber earlier output (the "disappearing lines"). */
 printf("\r\033[K");
#endif

 return;
}



/* This function shows the command line cut for the terminal width. */
void kbd_showbuf (char *kbdbuf) {
 char *startptr;
 int cols=display_columns();

 /* No sense in cutting if we have less than a screen width of chars. */
 if ((int)strlen(kbdbuf)<cols-1) {
   printf("%s",kbdbuf);
   return;
  }

 /* Set up the pointer. */
 startptr=(char *) ((kbdbuf+strlen(kbdbuf))-(cols-2));

 /* Print $-sign to show that we're out of bound. */
 printf("$");

 /* Show the cutted line. */
 printf("%s",startptr);

 return;
}
