#ifndef GAMP_H
#define GAMP_H

#include "util.h"
#include "controldata.h"
#include "id3.h"
#include "structs.h"
#include "forkplayer.h"
#include "list.h"
#include "gamp-util.h"
#include "parser.h"
#include "logo.h"
#include "input.h"

#include <stdlib.h>
#include CURSES_LOC
#include <pthread.h>

#ifndef TRUE
  #define TRUE 1
#endif
#ifndef FALSE
  #define FALSE 0
#endif

/* version
*/
#define         MAJOR           1
#define         MINOR           0
#define         PATCH           0

#define MAX(a,b)  ((a) > (b) ? (a) : (b))
#define MIN(a,b)  ((a) < (b) ? (a) : (b))

#define MAX_STRLEN 1024

#define QUIT 1
#define PLAYLIST 2   
#define PLAYER 3
#define GOPLAY 4

#define repeatNone 0
#define repeatOne 1
#define repeatAll 2

#define ELAPSED 30
#define REMAINING 31

#define forwardState 1
#define forwardStep 2 
#define rewindState 4 
#define rewindStep 8  
#define playState 16  
#define stopState 32  
#define pauseState 64 

extern void initDirlist(char *, ITEMLIST **);
extern void finishPlayer();
extern void initPlayer();
extern int playPlaylist();
extern void fillwin(WINDOW *, ITEM *, ITEMLIST *, int, ITEM *);
extern void finishEditor();
extern void initEditor();
extern int editPlaylist();
extern int addMp3(char *);
extern int readM3u(char *);
extern int writeM3u(char *);
extern void updateInfo(AudioInfo *ai, int type);
extern void updateAudioInfo(AudioInfo *ai);
extern void updateSongTime(int time); /* time = frame number */
extern void updateSongPosition(double pos); /* (percentage, 0..1) */
extern int songOpen(ITEM *sng); /* attempts to open song and returns file
                                   descriptor if successful. */
extern int songSize(ITEM *sng); /* returns length */
extern void infoReset(AudioInfo *inf); /* resets info to default values? */
extern void updatePlaying(ITEM *sng);
extern void updateStopped();
extern void showFilename(WINDOW *, ITEM *);
extern void toggleHelpWin();
extern void updateInfoWin();
extern void toggleInfoWin();
extern void updateMiniWin(ITEM *);
extern void toggleMiniWin();
extern void showLogo(WINDOW *, int, int);
extern ITEM *getNextSong();
extern ITEM *getPrevSong();
extern void newSetup(CONFIGURATION *);
extern void processArgs(int, char **, CONFIGURATION *);
extern void displayUsage();
extern void displayVersion();
extern void cleanup();
extern int exists(char *);

/* playlist editor windows */
extern WINDOW *dirwin; /* direcory list window */
extern WINDOW *listwin; /* playlist window */

/* player windows */
extern WINDOW *titlewin; /* song time/title window */
extern WINDOW *mainwin; /* main window */
extern WINDOW *progwin; /* progress window */

/* popup windows */
extern WINDOW *infowin; /* popup info window */
extern WINDOW *helpwin; /* popup help window */
extern WINDOW *miniwin; /* popup mini-list window */

/* playlist and current directory list */
extern ITEMLIST *playlist;
extern ITEMLIST *dirlist;

/* player configuration */
extern CONFIGURATION configuration;

/* current and last-known-playing song */
extern ITEM *curSong;
extern ITEM *lastSong;

/* the msg watcher thread */
extern pthread_t msg_thread;

/* function (PLAYER, PLAYLIST, GOPLAY, QUIT) */
extern int func;

#endif /* GAMP_H */
