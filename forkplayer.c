#include "forkplayer.h"
#include <signal.h>
#include <sys/socket.h>

#include <unistd.h>
#include <sys/types.h>

#include <sys/wait.h>

#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/time.h>
#include <limits.h>
#include <sys/uio.h>

#if defined(OS_Linux) && !defined(SCM_RIGHTS)
# define SCM_RIGHTS 1
#endif

#include <sys/un.h>
#include <errno.h>

#include <math.h>

#if defined(OS_Solaris) || defined(OS_SunOS)
# include <stropts.h>
#endif

#if defined(OS_Linux) || defined(OS_BSD)
# include <sysexits.h>
#endif

#define MSGIN         mreceive[0]
#define MSGOUT        msend[1]
#define MSGERR        merr[1]

#ifdef EX__MAX
# define FORK_ERROR     EX__MAX + 1
#else /* EX__MAX is usually 77/78 on most machines. */
# define FORK_ERROR     80
#endif

#define FORK_CLEAR    31337

/*
 * fork off the player backend (ie: amp, mpg123 or something similar) and
 * setup communications with it.
 */
void forkIt() {
   int i;

   if(dontFork)
      return;

   /*
    * Need this one to stop the `broken pipe' messages if the fork/exec
    * failed.
    */
   signal(SIGPIPE, SIG_IGN);

#if !defined(OS_Solaris) && !defined(OS_SunOS)
   if(socketpair(AF_UNIX, SOCK_STREAM, 0, msend) < 0)
      die("forkIt: socketpair msend");
   if(socketpair(AF_UNIX, SOCK_STREAM, 0, mreceive) < 0)
      die("forkIt: socketpair mreceive");
# if !defined(DEBUG)
   if(socketpair(AF_UNIX, SOCK_STREAM, 0, merr) < 0)
      die("forkIt: socketpair merr");
# endif
#else
   if (pipe(msend) < 0)
      die("forkIt: pipe msend");
   if(pipe(mreceive) < 0)
      die("forkIt: pipe mreceive");
# if !defined(DEBUG)
   if(pipe(merr) < 0)
      die("forkIt: pipe merr");
# endif
#endif

   if((fork_pid = fork()) == 0) {
      dup2(msend[0], STDIN_FILENO);

      close(msend[0]);
      close(msend[1]);

      dup2(mreceive[1], STDOUT_FILENO);

      close(mreceive[0]);
      close(mreceive[1]);

#if !defined(DEBUG)
      dup2(merr[1], STDERR_FILENO);

      close(merr[0]);
      close(merr[1]);
#endif

      execlp(configuration.player.player,
             configuration.player.player,
             configuration.player.arg1,
             configuration.player.arg2,
             configuration.player.arg3,
             configuration.player.arg4,
             configuration.player.arg5, NULL);

      for(i = 0; i < FOPEN_MAX; i++)
         close(i);

      exit(FORK_ERROR);

   }
   close(msend[0]);
   close(mreceive[1]);

#if !defined(DEBUG)
   close(merr[1]);
#endif

   if(doUpdate)
      update();
}

void *check_msg() {

   TControlMsg msg;
   fd_set rfds;
   int retval;
   int ret;

   for(;;) {

      FD_ZERO(&rfds);
      FD_SET(MSGIN, &rfds);

      retval = select(MSGIN+1, &rfds, NULL, NULL, NULL);

      if (retval > 0) {
         ret = read(MSGIN, &msg, sizeof(TControlMsg));

         if (ret == sizeof(TControlMsg)) parse_msg(&msg);

      } else if (retval == 0) {
         debug("check_msg: retval = 0\n");
      } else { /* retval < 0 */
         debug("check_msg: retval = %d, errno = %d\n", retval, errno);
      }
   }

   return(NULL);
}

void songkill() {
   int status;
   int dn = FALSE;

   debug("songkill called. must have had a sigchld. errno = %d\n", errno);
   if(waitpid(-1, &status, WNOHANG) == fork_pid) {
      dn = TRUE;
      if (WIFEXITED(status) && WEXITSTATUS(status) == FORK_ERROR) {
         /*
          * Stop all forks until the current line of execution is done.
          * Once it returns to the main loop, ie "interactive" jukebox
          * is in the works again, make certain the "no fork" is removed.
          */
         dontFork = TRUE;
         event(FORK_CLEAR);

         dn = FALSE;
      }
      childDied(dn);
   }

   signal(SIGCHLD, songkill);
}

int event(int ev) {
   if(ev == FORK_CLEAR) {
      /*
       * A posted event to clear the 'dontFork' bool value.
       */
      dontFork = FALSE;
      return TRUE;
   }
   return FALSE;
}

void parse_msg(PControlMsg msg) {
   AudioInfo ai;
   int err;
   double percent;
   char tmp[255];

   if ((msg->type != MSG_FRAMES) && (msg->type != MSG_POSITION))
      debug("parse_msg: msg->type = %d\n", msg->type);
   switch(msg->type) {
      case MSG_NEXT:
         backendNext = TRUE;
         playerNext();
         break;
      case MSG_INFO: {
         while((err = read(MSGIN, &ai, sizeof(AudioInfo))) < 0) {
            if(err == EINTR)
               continue;
            break;
         }
         updateAudioInfo(&ai);
         break;
      }
      case MSG_FRAMES:
         updateSongTime(msg->data);
         break;
      case MSG_AUDIOFAILURE:
         die("parse_msg: Audiofailure");
         break;
      case MSG_POSITION:

         if(curSong->size <= 0)
            updateSongPosition(1.0);
         else {
            percent = (double)msg->data / curSong->size;
            if (percent > 1) percent = 1;
            updateSongPosition(percent);
         }
         break;
      case MSG_RESPONSE:
         debug("parse_msg: msg->data = %d\n", msg->data);
         switch(msg->data) {
            case FORWARD_BEGIN:
            case FORWARD_STEP:
            case FORWARD_END:
               curState &= ~forwardStep;
               break;
            case REWIND_BEGIN:
            case REWIND_STEP:
            case REWIND_END:
               curState &= ~rewindStep;
               break;
         }
         break;
      case MSG_PRIORITY:
         if(msg->data == FAILURE)
            die("parse_msg: "
               "Attempt to change priority on the player "
               "failed. To change scheduling policy you "
               "need have superuser priviliges or have the "
               "player setuid root. Do *NOT*, I repeat *NOT*, "
               "set the graphical front as setuid root, as " 
               "it is highly unsafe.");
         break;

      case INFO_BITRATE:
         info.bitrate = msg->data;
         updateInfo(&info, INFO_BITRATE);
         break;
      case INFO_FREQUENCY:
         info.frequency = msg->data;
         updateInfo(&info, INFO_FREQUENCY);
         break;
      case INFO_MODE:
         info.mode = msg->data;
         updateInfo(&info, INFO_MODE);
         break;
      case INFO_CRC:
         info.crc = msg->data;
         updateInfo(&info, INFO_CRC);
         break;
      case INFO_COPYRIGHT:
         info.copyright = msg->data;
         updateInfo(&info, INFO_COPYRIGHT);
         break;
      case INFO_ORIGINAL:
         info.original = msg->data;
         updateInfo(&info, INFO_ORIGINAL);
         break;
      case INFO_IDENT: {
         read(MSGIN, tmp, msg->data);
         tmp[msg->data] = '\0';
         info.ident = tmp;
         updateInfo(&info, INFO_IDENT);
         break;
      }
   }
}

/*
 * moves to the current position in the song?
 */
void position(double fpos) {
   int pos;
   TControlMsg msg;

   if(fpos > 100.0)
      fpos = 100.0;
   if(fpos < 0.0)
      fpos = 0.0;

   pos = (int)floor(fpos * (double)curSong->size);

   msg.type = MSG_SEEK;
   msg.data = pos;

   send_msg(&msg);
}

/*
 * reads a pending message from the supplied file descriptor.
 */
void pendingmsg(int fd) {
   TControlMsg msg;
   
   read(fd, &msg, sizeof(TControlMsg));

   parse_msg(&msg);   
}

/*
 * send a message to the backend telling it to quit and then die.
 */
void playerKill() {
   TControlMsg msg;

   /*
    * Fork on quit?
    */
   if(fork_pid == -1)
      return;

   msg.type = MSG_QUIT;
   msg.data = TRUE;

   send_msg(&msg);
}

/*
 * tell the user that the player died and exit.
 */
void childDied(int dontNotify) {
   if(fork_pid != -1) {
      fork_pid = -1;
      
      close(MSGOUT);
      close(MSGIN);

      doUpdate = TRUE;

      if(!dontNotify)
         die("childDied: "
             "Execution of the player failed. Make sure "
             "you have specified the correct path and "
             "that the permissions are correctly set.\n");
   }
}

/*
 * send a message to the player.
 */
void send_msg(PControlMsg msg) {

   debug("send_msg: sending msg->type = %d\n", msg->type);
   if(dontFork)
      return;

   if(fork_pid == -1)
      forkIt();

   write(MSGOUT, msg, sizeof(TControlMsg));
}

/*
 * read in an incoming message into the PControlMsg structure.
 */
void get_msg(PControlMsg msg) {
   read(MSGIN, msg, sizeof(TControlMsg));
}

/*
 * update the backend player of current config info?
 */
void update() {
   TControlMsg msg;

   doUpdate = FALSE;

   msg.type = MSG_BUFFER;
   msg.data = configuration.player.buffer;

   send_msg(&msg);

   msg.type = MSG_BUFAHEAD;
   msg.data = configuration.player.playahead;

   send_msg(&msg);

   msg.type = MSG_PRIORITY;
   msg.data = configuration.player.realtime ? PRIORITY_REALTIME : 
      (configuration.player.priority? PRIORITY_NICE : PRIORITY_NORMAL);

   send_msg(&msg);

   msg.type = MSG_RELEASE;
   msg.data = configuration.player.release;
   
   send_msg(&msg);
}

/*
 * initialize?
 */
void initialize() {
   doUpdate = FALSE;
   dontFork = FALSE;
   backendNext = FALSE;

   curState = 0;

   fork_pid = -1;

   signal(SIGCHLD, songkill);
}

/*
 * advance to next song
 */
void playerNext() {
   curSong = getNextSong();
   playerPlay(curSong);
}

/*
 * go back to prev song
 */
void playerPrev() {
   curSong = getPrevSong();
   playerPlay(curSong);
}

/*
 * pause playback
 */
void playerPause() {
   TControlMsg msg;

   if (curState & playState)
      curState |= playState | pauseState;

   msg.type = MSG_CTRL;
   msg.data = PLAY_PAUSE;
   
   send_msg(&msg);
}

/*
 * move a step forward or backwards (depending on state) within current
 * track.
 */
void playerStep() {
   TControlMsg msg;

   msg.type = MSG_CTRL;

   if((curState & forwardState) && !(curState & forwardStep)) {
      msg.data = FORWARD_STEP;
      curState |= forwardStep;
   } else if((curState & rewindState) && !(curState & rewindStep)) {
      msg.data = REWIND_STEP;
      curState |= rewindStep;
   } else
      return;

   send_msg(&msg);
}

/*
 * begin rewinding current track
 */
void playerRewind() {
   TControlMsg msg;

   if(!(curState & rewindState)) {

      msg.type = MSG_CTRL;
      msg.data = REWIND_BEGIN;
      send_msg(&msg);

      curState |= rewindState | rewindStep;
   } else {

      msg.type = MSG_CTRL;
      msg.data = REWIND_END;
      send_msg(&msg);

      curState &= ~rewindState;
   }
}

/*
 * begin fast forwarding current track
 */
void playerForward() {
   TControlMsg msg;

   if(!(curState & forwardState)) {

      msg.type = MSG_CTRL;
      msg.data = FORWARD_BEGIN;
      send_msg(&msg);

      curState |= forwardState | forwardStep;
   } else {

      msg.type = MSG_CTRL;
      msg.data = FORWARD_END;
      send_msg(&msg);

      curState &= ~forwardState;
   }
}

/*
 * stop playback of current track
 */
void playerStop() {
   TControlMsg msg;

   /*
    * Stop something that doesn't exist?
    */
   if(fork_pid == -1)
      return;

   msg.type = MSG_CTRL;
   msg.data = PLAY_STOP;

   send_msg(&msg);

   curState = stopState;
}

/*
 * query the player and wait for a response.
 */
int sendQuery(PControlMsg msg) {
   TControlMsg rmsg;
   
   send_msg(msg);
   for(;;) {
      get_msg(&rmsg);
      if(rmsg.type != MSG_RESPONSE)
         parse_msg(&rmsg);
      else
         break;
   }

   return rmsg.data;
}

/*
 * checks if the player is paused.
 */
int isPaused() {
   TControlMsg msg;

   msg.type = MSG_QUERY;
   msg.data = QUERY_PAUSED;
   return sendQuery(&msg);
}

/*
 * checks if the player is playing.
 */
int isPlaying() {
   TControlMsg msg;

   msg.type = MSG_QUERY;
   msg.data = QUERY_PLAYING;

   return sendQuery(&msg);
}

/*
 * tells the player to play the specified song
 */
void playerPlay(ITEM *sng) {
   int fd = 0;
   TControlMsg msg;
   int ret = 0;

#if !defined(OS_Solaris) && !defined(OS_SunOS)
   struct msghdr hdr;
   struct m_cmsghdr fdhdr;
   struct iovec iov[1];
   char data[2];
#endif

   if(sng != NULL) {

      if (backendNext) backendNext = FALSE;
      if (sng != curSong) curSong = sng;
      lastSong = curSong;
      curState = playState;
      fd = songOpen(sng);
      curSong->size = songSize(sng);

      infoReset(&info);
      updatePlaying(sng);

      if(fd >= 0) {
#if !defined(OS_Solaris) && !defined(OS_SunOS)
         /*
          * Reset the audio info.
          * We do this if cuz if we have a player
          * that doesn't support or send the MSG_INFO.
          */ 

         msg.type = MSG_SONG;
         msg.data = 0;
         
         send_msg(&msg);

         iov[0].iov_base = data;
         iov[0].iov_len = 2;

         hdr.msg_iov = iov;
         hdr.msg_iovlen = 1; 
         hdr.msg_name = NULL;
         hdr.msg_namelen = 0;
         hdr.msg_flags = 0;
         
         fdhdr.cmsg_len = sizeof(struct m_cmsghdr);
         fdhdr.cmsg_level = SOL_SOCKET;
         fdhdr.cmsg_type = SCM_RIGHTS;
         fdhdr.fd = fd;

#if !defined(OS_BSD)
         hdr.msg_control = &fdhdr;
#else
         hdr.msg_control = (char *) &fdhdr;
#endif

         hdr.msg_controllen = sizeof(struct m_cmsghdr);

         if((ret = sendmsg(MSGOUT, &hdr, 0)) < 0)
            die("playerPlay: sendmsg: %d", ret);
#else
         msg.type = MSG_SONG;
         msg.data = 0;
         send_msg(&msg);
         if(ioctl(MSGOUT, I_SENDFD, fd) < 0)
            die("playerPlay: ioctl(I_SENDFD)");
#endif
         close(fd);
      }

   } else { /* end of playlist */

      curSong = lastSong; /* last known playable song */

      if (backendNext) {
         backendNext = FALSE;
         curSong = NULL;
         playerStop();
         updateStopped();
      }
/*      playerStop();
      msg.type = MSG_RELEASE;
      msg.data = configuration.player.release;
      
      send_msg(&msg);*/
   }
}
