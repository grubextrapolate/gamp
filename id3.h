#ifndef ID3_H
#define ID3_H

#define TAGLEN_TAG 3
#define TAGLEN_SONG 30
#define TAGLEN_ARTIST 30
#define TAGLEN_ALBUM 30
#define TAGLEN_YEAR 4
#define TAGLEN_COMMENT 30
#define TAGLEN_GENRE 1

typedef struct ID3_struct {
  char tag[TAGLEN_TAG+1];
  char songname[TAGLEN_SONG+1];
  char artist[TAGLEN_ARTIST+1];
  char album[TAGLEN_ALBUM+1];
  char year[TAGLEN_YEAR+1];
  char comment[TAGLEN_COMMENT+1];
  int track;
  int genre;  
} ID3_tag;

#endif /* ID3_H */
