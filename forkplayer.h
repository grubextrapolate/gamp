#ifndef ForkPlayer_H
#define ForkPlayer_H

#include "gamp.h"

extern void playerPlay(ITEM *);
extern void playerStop();
extern void playerPause();
extern void initialize();
extern void playerForward();
extern void playerRewind();
extern int isPlaying();
extern void update();
extern void position(double);
extern int isPaused();
extern void playerKill();
extern int event(int);

extern void forkIt();
extern void childDied(int);
extern void send_msg(PControlMsg);
extern void get_msg(PControlMsg);
extern void parse_msg(PControlMsg);
extern void playerNext();
extern void playerPrev();
extern int sendQuery(PControlMsg);

extern void songTime(int);
extern void songPosition(double);
extern void audioInfo(AudioInfo*);

extern void playerStep();
extern void pendingmsg(int);
extern void *check_msg();

extern void initsigchild();
extern void songkill();

int dontFork;
int fork_pid;
int doUpdate;
int curState;
int msend[2];
int mreceive[2];
AudioInfo info;
int backendNext;

#endif
