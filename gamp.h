#ifndef GAMP_H
#define GAMP_H

#include <curses.h>
#include "list.h"

/* include amp stuff */
#include "amp.h"
#include <fcntl.h>
#include <stdio.h>
#define AUDIO
#include "audio.h"
#include "formats.h"
#include "getbits.h"
#include "huffman.h"
#include "layer3.h"
#include "transform.h"

/* include our stuff */
#include "list.h"
#include <math.h>
#include <curses.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

/* include id3 stuff (from id3ren) */
#include "genre.h" 

#define TAGLEN_TAG 3
#define TAGLEN_SONG 30
#define TAGLEN_ARTIST 30
#define TAGLEN_ALBUM 30
#define TAGLEN_YEAR 4
#define TAGLEN_COMMENT 30
#define TAGLEN_GENRE 1
  
typedef struct ID3_struct
{
  char tag[TAGLEN_TAG+1];
  char songname[TAGLEN_SONG+1];
  char artist[TAGLEN_ARTIST+1];
  char album[TAGLEN_ALBUM+1];
  char year[TAGLEN_YEAR+1];
  char comment[TAGLEN_COMMENT+1];
  int genre;
} ID3_tag;

#define CONT 0
#define QUIT 1
#define PLAYLIST 2
#define PLAYER 3

#define STOP 4
#define NEXT 5
#define PREV 6
#define FFWD 7
#define REW 8
#define PAUSE 9
#define PLAY 10
#define START 11

#define OPENED 20
#define CLOSED 21

#define ELAPSED 30
#define REMAINING 31

#define NO_SPECTRUM 40
#define SHOW_SPECTRUM 41

#define NUM_BANDS 18
extern int bar_heights[32];

extern char cwd[256];
extern char tmpstr[150];

#define LOGO_X 50
#define LOGO_Y 20
extern char logo[LOGO_Y][LOGO_X];
extern int logo_width;
extern int logo_height;

extern pthread_t player_thread; /* the player thread */
extern pthread_mutex_t mut;
extern pthread_cond_t cond;

extern int stop;   /* stopping condition for the program */
extern int play;
extern int current;

extern int time_mode;
extern int length;

extern int ch;
extern int currentVolume;
extern char filename[150];
extern char tmpstring[150];
extern char tmpname[150];
extern char start_dir[150];

extern int first_loop;
extern int last_loop;
extern struct AUDIO_HEADER header;
extern int cnt, err;

/* our windows */
extern WINDOW *dirwin, *playwin, *helpwin, *titlewin, *mainwin;

extern int file_status;
extern ID3_tag *ptrtag;

extern void show_filename(WINDOW *, STRLIST *, int);
extern void clear_filename(WINDOW *);
extern void statusDisplay(WINDOW *, WINDOW *, int, int, struct AUDIO_HEADER *, int);
extern int inline processHeader(struct AUDIO_HEADER *, int);

extern int id3_seek_header (FILE *fp, char *fn);
extern int id3_read_file (char *dest, unsigned long size, FILE *fp, char *fn);
extern int read_tag (FILE *fp, char *fn);
extern char *getName(char *filename);
extern int get_time(struct AUDIO_HEADER *header);
extern int get_frame_size(struct AUDIO_HEADER *header);
extern double time_per_frame(struct AUDIO_HEADER *header);
extern double bits_per_frame(struct AUDIO_HEADER *header);
extern int ismp3(char *name);
extern void strtrunc(char *str, int length);
extern void strtrim(char *string);
extern void read_logo();

#endif /* GAMP_H */
