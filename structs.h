#ifndef STRUCTS_H
#define STRUCTS_H

#define LOGO_MAX_X 100
#define LOGO_MAX_Y 40

typedef struct ITEM {
   char *path; /* song or directory name with full path */
   char *name; /* name of song or dir (pointer to within item.path */
   AudioInfo *info; /* song info or NULL if directory */
   int size;
   int length;
   ID3_tag *id3;
   int isfile;
   int marked;
   struct ITEM *next; /* next/prev for linked list */
   struct ITEM *prev;
} ITEM;

typedef struct ITEMLIST {
   ITEM *head; /* head of list */
   ITEM *tail; /* tail of list */
   int num; /* number of items in list */
} ITEMLIST;

typedef struct PLAYERCONF {

   int buffer;
   int playahead;
   int forcetomono;
   int release;
   int realtime;
   int priority;
   int mixer;
   char *player; /* mp3 player */
   char *arg1; /* first argument for player (or null) */
   char *arg2; /* second ... */
   char *arg3; /* third ... */
   char *arg4; /* i think you've got the idea by now... */
   char *arg5; /* and one more... */

} PLAYERCONF;

typedef struct CONFIGURATION {

   int dirty; /* has config changed? if so write on exit */
   PLAYERCONF player; /* player-specific config options */
   int displaySpectrum; /* TRUE or FALSE */
   int timeMode; /* REMAINING or ELAPSED */
   int startWith; /* PLAYER or PLAYLIST */
   int ignoreID3; /* TRUE or FALSE */
   char *logoFile; /* full path to gamp logo file */
   int logoHeight; /* height of logo (in lines) */
   int logoWidth; /* width of logo (in cols) */
   char **logo; /* logo */
   char *configFile; /* full path to config file */
   int ignoreCase; /* ignore filename case when sorting */
   int repeatMode; /* repeat none, one, all */
   int stepTimeout; /* delay (in milliseconds) between ffwd/rew steps */

} CONFIGURATION;

#endif /* STRUCTS_H */
