#include "headers.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static char IRC_SERVER_NICK[600];
static char IRC_AUTOREPLY[600];
/* Just for irc_autoreply_off. Thank you VERY much stefan! grrr. */
extern SOCKET socketnr;

int CLIENT_SAYS_QUIT=0;
int IRC_AUTOREPLY_ON=0;

/* Function to log in. Returns nothing. */
void irc_login (SOCKET socketnr, char *argv[]) {
 char buf[700];
 fd_set writefds, readfds;

 /* Sync. Block until the socket is writable and the server has sent
    its greeting. */
 FD_ZERO(&writefds);
 FD_SET(socketnr,&writefds);
 select ((int)socketnr+1,NULL,&writefds,NULL,NULL);

 FD_ZERO(&readfds);
 FD_SET(socketnr,&readfds);
 select ((int)socketnr+1,&readfds,NULL,NULL,NULL);

 /* Log in. Built with snprintf so a long nick/server pair can't overflow
    buf (argv[1] and argv[2] are already length-checked in main). */
 printf("*** Logging in...");

 snprintf(buf,sizeof(buf),"USER %s %s %s :%s\r\n",
          argv[1],argv[2],argv[2],argv[1]);
 tcp_send (socketnr, buf, strlen(buf));
 printf("[USER]...");

 snprintf(buf,sizeof(buf),"NICK %s\r\n",argv[1]);
 tcp_send (socketnr, buf, strlen(buf));
 printf("[NICK].\n");
}



/* Parse IRC responses. */
void irc_parse(SOCKET socketnr, char *inbuf) {

 /* Check for common commands. */
 if (irc_cmd_PING(socketnr,inbuf))
  return;

 if (irc_cmd_fromuser(socketnr, inbuf))
  return;


 return;
}



/* PING command. */
int irc_cmd_PING(SOCKET socketnr, char *inbuf) {
 /* Sized for "PONG " + a full 599-byte input line, so a server can't
    overflow us with a long PING token. */
 char PONG[608]="PONG ";
 int offset;

 /* Have we got PING? (\1 is for CTCP ping performed by undernet etc.) */
 if (strncmp(inbuf,"PING",4)!=0 && strncmp(inbuf,"\1PING",5)!=0)
  return 0;

 /* I go a char further when ctcp ping is received. Just in case. */
 offset=(inbuf[0]==1)?6:5;

 /* Only append the argument if the line is actually that long, so a bare
    "PING"/"\1PING" can't make us read past the terminator. */
 if ((int)strlen(inbuf)>=offset)
  strcat(PONG, (char *)(inbuf+offset));

 /* Respond with PONG. */
 tcp_send(socketnr,PONG,strlen(PONG));
 tcp_sendchar(socketnr,13);
 tcp_sendchar(socketnr,10);

 return 1;
}


/* Pretty-print a WHO reply (numeric 352). The raw form is
   "352 <me> <chan> <user> <host> <server> <nick> <flags> :<hop> <realname>"
   and the generic server-message path would only show the realname, so we
   format the useful columns ourselves. */
static void irc_print_who(char *message) {
 char work[1300];
 char *chan,*user,*host,*nick,*flags,*real;
 int len;

 strcpy(work,message);

 /* Drop the trailing CR/LF. */
 len=strlen(work);
 while (len>0 && (work[len-1]=='\r' || work[len-1]=='\n'))
  work[--len]=0;

 /* Split off the ":<hop> <realname>" trailer at the first ':'. */
 real=strchr(work,':');
 if (real!=NULL) {
   *real++=0;
   /* Skip the leading hop-count token. */
   while (*real==32) real++;
   while (*real!=0 && *real!=32) real++;
   while (*real==32) real++;
  }

 strtok(work," ");          /* "352"          */
 strtok(NULL," ");          /* our own nick   */
 chan =strtok(NULL," ");
 user =strtok(NULL," ");
 host =strtok(NULL," ");
 strtok(NULL," ");          /* server name    */
 nick =strtok(NULL," ");
 flags=strtok(NULL," ");

 /* Malformed reply - fall back to the raw line. */
 if (nick==NULL || user==NULL || host==NULL) {
   printf("--- %s",message);
   return;
  }

 printf("--- %-16s (%s@%s)",nick,user,host);
 if (flags!=NULL)
  printf(" %s",flags);
 if (chan!=NULL)
  printf(" %s",chan);
 if (real!=NULL && *real!=0)
  printf(" %s",real);
 printf("\n");
}



/* Turn a raw seconds count into a readable "4d 7h 20m 12s" duration, leaving
   off the larger units when they are zero. */
static void format_duration(long secs, char *out) {
 char part[32];
 long d,h,m;

 if (secs<0) secs=0;
 d=secs/86400; secs%=86400;
 h=secs/3600;  secs%=3600;
 m=secs/60;    secs%=60;

 out[0]=0;
 if (d>0)            { sprintf(part,"%ldd ",d); strcat(out,part); }
 if (d>0 || h>0)     { sprintf(part,"%ldh ",h); strcat(out,part); }
 if (d>0 || h>0 || m>0) { sprintf(part,"%ldm ",m); strcat(out,part); }
 sprintf(part,"%lds",secs);
 strcat(out,part);
}



/* Pretty-print the common WHOIS/WHOWAS numerics. Without this the generic
   server-message path shows only the text after the last ':' and drops the
   nick / host / idle columns that live before it. */
static void irc_print_whois(int numeric, char *message) {
 char work[1300];
 char *nick,*a,*b,*trailer;
 int len;

 strcpy(work,message);
 len=strlen(work);
 while (len>0 && (work[len-1]=='\r' || work[len-1]=='\n'))
  work[--len]=0;

 /* Split the ":<text>" trailer off at the first ':'. */
 trailer=strchr(work,':');
 if (trailer!=NULL)
  *trailer++=0;

 strtok(work," ");          /* numeric        */
 strtok(NULL," ");          /* our own nick   */
 nick=strtok(NULL," ");     /* subject nick   */
 a   =strtok(NULL," ");     /* first extra    */
 b   =strtok(NULL," ");     /* second extra   */

 if (nick==NULL) {
   printf("--- %s",message);
   return;
  }

 switch (numeric) {
  case 311:                 /* nick user host * :realname   */
  case 314:                 /* WHOWAS, same layout          */
   printf("--- %s is %s@%s",nick, a?a:"?", b?b:"?");
   if (trailer!=NULL && *trailer)
    printf(" (%s)",trailer);
   printf("\n");
   break;
  case 312:                 /* nick server :serverinfo      */
   printf("--- %s using %s",nick, a?a:"?");
   if (trailer!=NULL && *trailer)
    printf(" (%s)",trailer);
   printf("\n");
   break;
  case 317:                 /* nick idle [signon] :text     */
   printf("--- %s has been idle ",nick);
   if (a!=NULL) {
     char dur[64];
     format_duration(strtol(a,NULL,10),dur);
     printf("%s",dur);
    }
   else
    printf("?");
   if (b!=NULL) {
     time_t signon=(time_t)strtoul(b,NULL,10);
     char *ts=ctime(&signon);
     if (ts!=NULL) {
       ts[strlen(ts)-1]=0;  /* drop ctime()'s trailing newline */
       printf(", signed on %s",ts);
      }
    }
   printf("\n");
   break;
  case 319:                 /* nick :channels               */
   printf("--- %s on %s\n",nick, (trailer&&*trailer)?trailer:"(none)");
   break;
  case 330:                 /* nick account :is logged in.. */
   printf("--- %s is logged in as %s\n",nick, a?a:"?");
   break;
  case 301:                 /* nick :awaymsg                */
   printf("--- %s is away: %s\n",nick, (trailer&&*trailer)?trailer:"");
   break;
  default:                  /* 313/338/671/...: nick :text  */
   if (trailer!=NULL && *trailer)
    printf("--- %s %s\n",nick,trailer);
   else
    printf("--- %s\n",nick);
   break;
 }
}



/* Handling of commands from a user. (Starting with ':'). */
int irc_cmd_fromuser(SOCKET socketnr, char *inbuf) {
 char username[600];
 char nickname[600];
 char message[1300];
 char *tok;
 int i,j;

 if (inbuf[0]!=':')
  return 0;

 /* Translate the message. */

 from_latin(inbuf);

 /* Get the username. */
 for (i=1;i<(int)strlen(inbuf);i++) {
   if (inbuf[i]==32)
    break;
   username[i-1]=inbuf[i];
  }
 username[i-1]=0;

 /* Get the message. If there was no space there is no message either. */
 if (i<(int)strlen(inbuf))
  strcpy(message,(char *)inbuf+(i+1));
 else
  message[0]=0;

 /* Get the nickname. If not found, return full user. */
 for (i=0;i<(int)strlen(username);i++) {
   if (username[i]=='!')
    break;
   nickname[i]=username[i];
  }
  nickname[i]=0;

 /* Is this the first nick we've ever got? In that case it is the
    servername. */
 if (strlen(IRC_SERVER_NICK)==0)
  strcpy(IRC_SERVER_NICK,nickname);

  /* Do we have a PRIVMSG? */
 tok=strstr(message,"PRIVMSG");
 if (tok==NULL) {
   /* Check if this is a server status message. If else, just print. */
   if (strcmp(IRC_SERVER_NICK,nickname)==0) {
     /* WHO reply - format the columns instead of dropping them. */
     if (strncmp(message,"352 ",4)==0) {
       irc_print_who(message);
       return 1;
      }
     /* WHOIS/WHOWAS replies - same idea, keep the columns. */
     if (isdigit((unsigned char)message[0]) &&
         isdigit((unsigned char)message[1]) &&
         isdigit((unsigned char)message[2]) && message[3]==32) {
       j=atoi(message);
       if (j==301 || j==311 || j==312 || j==313 || j==314 || j==317 ||
           j==319 || j==330 || j==338 || j==671) {
         irc_print_whois(j,message);
         return 1;
        }
      }
     /* Search for ':'. */
     j=0;
     for (i=0;i<(int)strlen(message);i++)
      if (message[i]==':')
       j=i;

     if (j==0 && strncmp(message, IRC_SERVER_NICK,strlen(IRC_SERVER_NICK))!=0)
      /* Status message, like whois and stuff. */
      printf("--- %s",message);
     else
      /* Startup message. */
      printf("--- %s",(char *)(message+j));
     return 1;
    }
   else {
      printf("<%s> %s",nickname, message);
     return 1;
    }
  }

 /* Strip PRIVMSG. Overlapping copy, so memmove. */
 memmove(message,(char *)(tok+8),strlen((char *)(tok+8))+1);

 /* Check if we have a channel. */
 if (message[0]=='&' || message[0]=='#') {
   /* Check for ACTION. */
   tok=strstr(message,":\1ACTION");
   if (tok!=NULL) {

     /* Just blank out trailing \1...easiest. */
     message[strlen(message)-3]=32;

     /* Cut out ACTION */
     memmove (tok,(char *)(tok+9),((message+strlen(message))-(tok+6)));

     /* Nice save huh? :) Saves me much work... */
     printf("*%s in %s",nickname, message);
    }
   else
    printf("<%s> %s",nickname, message);
  }
 else {
   /* Is this a CTCP message? */
   if (strstr(message,"\1")!=NULL)
    irc_CTCP_reply (socketnr, nickname, message);
   else {
     printf("<%s> MSG %s",nickname, message);
     /* Check if we got to send an autoreply. */
     if (IRC_AUTOREPLY_ON && strcmp(IRC_SERVER_NICK,nickname)!=0) {
       printf("[AUTOREPLY SENT TO %s]\n",nickname);
       /* Reuse message. */
       strcpy(message, "NOTICE ");
       strcat(message, nickname);
       strcat(message," :");
       strcat(message,IRC_AUTOREPLY);
       tcp_send(socketnr,message,strlen(message));
       tcp_sendchar(socketnr,13);
       tcp_sendchar(socketnr,10);
      }
    }
  }

 return 1;
}



/* Translate to LATIN-1 (ISO 8859-1)*/
void to_latin (char *text) {
 unsigned char *utext=(unsigned char *)text;
 int i;

 for (i=0;utext[i]!=0;i++) {
   if (utext[i]==134) utext[i]=229;
   if (utext[i]==132) utext[i]=228;
   if (utext[i]==148) utext[i]=246;
   if (utext[i]==143) utext[i]=197;
   if (utext[i]==142) utext[i]=196;
   if (utext[i]==153) utext[i]=214;
  }
 return;
}



/* Translate from LATIN-1 (ISO 8859-1)*/
void from_latin (char *text) {
 unsigned char *utext=(unsigned char *)text;
 int i;

 for (i=0;utext[i]!=0;i++) {

   if (utext[i]==229) utext[i]=134;
   if (utext[i]==228) utext[i]=132;
   if (utext[i]==246) utext[i]=148;
   if (utext[i]==197) utext[i]=143;
   if (utext[i]==196) utext[i]=142;
   if (utext[i]==214) utext[i]=153;
  }
 return;
}


/* Processor for IRC aliases. Is done preferably after '/' is removed! */
int irc_alias (char *buf) {
 int send_command=1,i,len;
 char tempbuf[600], upperbuf[600], lowerbuf[600];

 /* OK, now when you have typed - YOU'RE BACK!!! */
 if (IRC_AUTOREPLY_ON)
 irc_autoreply_off();

 /* Just in case. */
 trim(buf);
 /* '/' in front=command. Nothing in front=':'. */
 if (buf[0]=='/') {
   /* To handle empty command - special case. */
   if (strlen(buf)==1 || buf[1]==32)
    strcpy(buf,"UNKNOWN_COMMAND");
   else
    memmove(buf,(char *)(buf+1),strlen((char *)(buf+1))+1);
  }
 else {
   strcpy (tempbuf,":");
   strcat (tempbuf,buf);
   strcpy (buf,tempbuf);
  }

 /* Copy buf to upperbuf and make all characters uppercase (for comparison).*/
 strcpy(upperbuf,buf);
 strcpy(lowerbuf,buf);
 for (i=0;i<(int)strlen(upperbuf);i++) {
   upperbuf[i]=toupper((unsigned char)upperbuf[i]);
   lowerbuf[i]=tolower((unsigned char)lowerbuf[i]);
  }

 /* First, handle QUIT message and set a flag. */
 if (strncmp(upperbuf,"QUIT",4)==0 && (strlen(upperbuf)==4 || upperbuf[4]==32)) {
   /* Do we have a quit message? If not set default. */
   if (strlen(buf)==4 || buf[4]==13) {
     strcpy (buf,"QUIT MinIRC ");
     strcat (buf,VERSION);
     strcat (buf," by Andreas Westling");
    }
   /* Insert ':' */
   strcpy(tempbuf,"QUIT :");
   strcat (tempbuf,(char *)(buf+4));
   strcpy(buf,tempbuf);
   CLIENT_SAYS_QUIT=1;
   return 1;
  }


 /* MSG and QUERY are aliases for PRIVMSG. */
 if (strncmp(upperbuf,"MSG ",4)==0) {
   irc_alias_replace(buf,"MSG","PRIVMSG");
   irc_msg_insert_colon (buf);
  }

 if (strncmp(upperbuf,"QUERY ",6)==0) {
   irc_alias_replace(buf,"QUERY","PRIVMSG");
   irc_msg_insert_colon (buf);
  }

 /* PART...just for setting active channel to ZERO. */
 if (strncmp(upperbuf,"PART ",5)==0) {
   /* See that it is the active channel we are leaving. */
   if (strlen(upperbuf)<7)
    return 1;

   strcpy(tempbuf, (char *)(lowerbuf+5));
   trim(tempbuf);

   if (strncmp(IRC_ACTIVE_CHANNEL,tempbuf,strlen(IRC_ACTIVE_CHANNEL))==0)
    IRC_ACTIVE_CHANNEL[0]=0;
   return 1;
  }


 /* We must wrap join to be able to set active channel. */
 if (strncmp(upperbuf,"JOIN ",5)==0) {
   /* Loop to find # or & */
   for (i=5;i<(int)strlen(buf);i++)
    if (buf[i]=='&' || buf[i]=='#')
     break;
   strcpy(IRC_ACTIVE_CHANNEL,(char *)(buf+i));
  }


 /* Autoreply. By request of StefanH. */
 if (strncmp(upperbuf,"AUTO",4)==0) {
   /* We don't know this command if this applies. */
   if (strlen(buf)>4 && buf[4]!=32)
    return 1;

   /* Look for ':' */
   for (i=0;i<(int)strlen(buf);i++)
    if (buf[i]==':')
     break;

   /* We got ':' ? */
   if (strlen(buf)!=4) {
     /* We got ':' ? */
     if (i<(int)strlen(buf))
      strcpy(IRC_AUTOREPLY,(char *)(buf+i+1));
     else
     strcpy(IRC_AUTOREPLY,(char *)(buf+4));
     IRC_AUTOREPLY_ON=1;
     printf("[AUTOREPLY ON]\n");
     /* Set away. */
     strcpy(buf,"AWAY :");
     strcat(buf,IRC_AUTOREPLY);
     return 1;
    }
   /* We got nothing=OFF? */
   else {
     irc_autoreply_off();
     return 0;
    }
  }


 /* VER alias. */
 if (strncmp(upperbuf,"VER",3)==0) {
    strcpy (tempbuf,"*** ");
    version_string (upperbuf,0);
    strcat (tempbuf,upperbuf);
    printf("%s\n",tempbuf);
    return 0;
   }


 /* Userinfo alias. (Length guard so three copies still fit in buf.) */
 if (strncmp(upperbuf,"USERINFO ",9)==0 && strlen(buf)<150) {
   strcpy(tempbuf,buf);
   strcpy(buf,"USERHOST ");
   strcat(buf,(char *)(tempbuf+9));
   strcat(buf,"\r\nWHOIS ");
   strcat(buf,(char *)(tempbuf+9));
   strcat(buf,"\r\nTRACE ");
   strcat(buf,(char *)(tempbuf+9));
  }


 /* ACTION, like /me in mIRC.
    Action messages are composed like this:
    PRIVMSG [nickname] :[0x1]ACTION text[0x1] */
 if (strncmp(upperbuf,"ME ",3)==0) {
   /* Cut n' paste. */
   strcpy(tempbuf,"PRIVMSG ");
   strcat(tempbuf,IRC_ACTIVE_CHANNEL);
   strcat(tempbuf," :\1ACTION");
   irc_alias_replace(buf,"ME",tempbuf);
   strcat(buf,"\1");
  }



 /* CTCP...almost like ME but more PERSONAL. :-E*/
 if (strncmp(upperbuf,"CTCP ",5)==0) {
  memmove(buf,(char *)(buf+5),strlen((char *)(buf+5))+1);
  trim(buf);
  len=strlen(buf);
  /* Find space. */
  for (i=1;i<len;i++)
   if (buf[i]==32)
    break;

  if (i>=len-1){
    printf("[NOT ENOUGH PARAMETERS FOR CTCP COMMAND]\n");
    return 0;
   }

  /* Save ctcp and nickname. */
  strcpy(upperbuf,(char *)(buf+i));
  /* Cut for fancy. */
  upperbuf[(strlen(buf)-i)+1]=0;
  upperbuf[strlen(buf)-i]=0;
  buf[i]=0;
  buf[i+1]=0;
  /* Gotta trim so we'll find ':'. I trim nickname too :) */
  trim(buf);
  trim(upperbuf);
  /* Remove ':'. */
  upperbuf[strlen(upperbuf)+1]=0;
  if (upperbuf[0]==':') {
    upperbuf[0]=1;
    strcpy(tempbuf,upperbuf);
   }
  else {
    strcpy (tempbuf,"\1");
    strcat (tempbuf,upperbuf);
   }

  /* First word in CTCP should be capital. */
  for (i=0;i<(int)strlen(tempbuf);i++)
   if (tempbuf[i]!=32)
    tempbuf[i]=toupper((unsigned char)tempbuf[i]);
   else
    break;
  /* Send the damn thing now. */
  strcpy(upperbuf,"PRIVMSG ");
  strcat(upperbuf,buf);
  strcpy(buf,upperbuf);
  strcat(buf," :");
  strcat(buf,tempbuf);
  strcat(buf,"\1");
  return 1;
 }


 /* Set active channel alias. (new version, adapted for nicknames too.. */
  if (buf[0]=='#') {
   strcpy(IRC_ACTIVE_CHANNEL,(char *)(buf+1));
   return 0;
  }

 /* W = NAMES #Channel...be careful. Check that we have an active channel
    to prevent server flood out. */
 if (upperbuf[0]=='W' && (strlen(upperbuf)==1 || upperbuf[1]==32 || upperbuf[1]==13)) {
   if (strlen(IRC_ACTIVE_CHANNEL)==0) {
     printf("[ERROR: ACTIVE CHANNEL NOT SET]\n");
     return 0;
    }
   strcpy(buf,"NAMES ");
   strcat(buf,IRC_ACTIVE_CHANNEL);
   return 1;
  }


 /* Send to active channel alias.
    : [message] = PRIVMSG [active channel] : [message]. */
 if (buf[0]==':') {
   if (strlen(IRC_ACTIVE_CHANNEL)!=0) {
     strcpy(tempbuf,"PRIVMSG ");
     strcat(tempbuf,IRC_ACTIVE_CHANNEL);
     strcat(tempbuf," :");
     irc_alias_replace(buf,":",tempbuf);
    }
   else {
     printf("[ERROR: ACTIVE CHANNEL NOT SET]\n");
     return 0;
    }
  }

 return send_command;
}


/* Replaces an alias in a string. */
void irc_alias_replace (char *buf, char *alias, char *replacestring) {
 /* Cut the alias. Overlapping copy, so memmove. */
 memmove(buf,(char *)(buf+strlen(alias)),strlen((char *)(buf+strlen(alias)))+1);

 /* Make place for the replacestring. */
 memmove ((char *)(buf+strlen(replacestring)), buf,strlen(buf)+1);

 /* Paste the replacestring in. */
 memcpy (buf,replacestring,strlen(replacestring));

 return;
}



/* Replies to CTCP requests. */
void irc_CTCP_reply (SOCKET socketnr, char *nickname, char *message) {
 char CTCPbuf[1300];
 char CTCPmsg[600];
 char CTCPresponse[600];

 int i,copy=-1,len;

 /* Clear up, so we don't repeat. */
 CTCPmsg[0]=0;
 CTCPresponse[0]=0;

 /* Get the CTCP command. CTCPs = Actions. */
 for(i=0;i<(int)strlen(message);i++) {
   if (message[i]==1)
    copy=-copy;
   else
    if (copy==1) {
      len=strlen(CTCPmsg);
      CTCPmsg[len]=message[i];
      CTCPmsg[len+1]=0;
     }
  }

 /* VERSION CTCP - Autoreply*/
 if (strcmp(CTCPmsg,"VERSION")==0) {
   /* We destroy CTCPbuf here... why not? */
   version_string (CTCPbuf,0);
   strcat (CTCPresponse,"VERSION ");
   strcat (CTCPresponse,CTCPbuf);
  }

 /* PING CTCP - echo the payload straight back so the sender can measure
    round-trip time. */
 if (strncmp(CTCPmsg,"PING",4)==0 && (CTCPmsg[4]==0 || CTCPmsg[4]==32))
  strcpy (CTCPresponse,CTCPmsg);

 /* TIME CTCP - reply with our local date/time. */
 if (strcmp(CTCPmsg,"TIME")==0) {
   time_t now=time(NULL);
   char *timestr=ctime(&now);
   if (timestr!=NULL) {
     strcpy (CTCPresponse,"TIME ");
     strcat (CTCPresponse,timestr);
     /* ctime() ends with '\n' which we don't want inside the CTCP. */
     CTCPresponse[strlen(CTCPresponse)-1]=0;
    }
  }

 /* Print that we have got a CTCP message. */
 printf("[CTCP from %s: %s -",nickname,CTCPmsg);

 /* Return CTCP response if known. */
 if (strlen(CTCPresponse)!=0) {
   strcpy(CTCPbuf,"NOTICE ");
   strcat(CTCPbuf,nickname);
   strcat(CTCPbuf," :\1");
   strcat(CTCPbuf,CTCPresponse);
   strcat(CTCPbuf,"\1\r\n");

   tcp_send(socketnr,CTCPbuf,strlen(CTCPbuf));
   printf(" response sent]\n");
  }
 else
   printf(" unknown (ignored)]\n");

 return;
}



/* Function for inserting ':' in front of message. */
void irc_msg_insert_colon (char *buf) {
 char tempbuf[600], channel[600];
 int i;
 /* Remove PRIVMSG. */
 strcpy(tempbuf,(char *)(buf+8));

 /* Remove nick/channel. */
 for (i=0;i<(int)strlen(tempbuf);i++)
  if (tempbuf[i]==32)
   break;

 /* No space? Let the server give error. */
 if (i==(int)strlen(tempbuf)-1)
  return;

 /* Copy the channel and remove it from buf. */
 strncpy(channel,tempbuf,i);
 channel[i]=0;

 /* Copy the rest. */
 strcpy(tempbuf,(char *)(buf+i+8));

 /* Get buf ready. */
 strcpy(buf, "PRIVMSG ");
 strcat(buf, channel);
 strcat(buf," :");
 strcat(buf,tempbuf);

}


/* Sets autoreply off. */
void irc_autoreply_off(void) {
 char buf[32];

 IRC_AUTOREPLY_ON=0;
 printf("[AUTOREPLY OFF]\n");
 /* Set back */
 strcpy(buf,"AWAY");

 /* Send. */
 tcp_send (socketnr,buf,strlen(buf));
 /* Send "\n". */
 tcp_sendchar (socketnr, 13);
 tcp_sendchar (socketnr, 10);

 return;
}
