#include <stdio.h>
#include "gamp.h"

extern int id3_seek_header (FILE *, char *);
extern int id3_read_file (char *, unsigned long, FILE *, char *);
extern int read_tag (FILE *, char *, ID3_tag *);
extern void getID3(ITEM *);
extern int get_time(AudioInfo *);
extern int ismp3(char *);
extern int isoog(char *);
extern int ism3u(char *);
extern char *strpad(ITEM *, int);
extern void strtrim(char *, int);
extern void strleadtrim(char *);

