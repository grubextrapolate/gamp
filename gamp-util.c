
#include "gamp-util.h"
#include <string.h>
#include <ctype.h>

/*
 * moves the file pointer to where the id3 header should be.
 */
int id3_seek_header(FILE *fp, char *fn) {

   if (fseek(fp, -128, SEEK_END) < 0) {
      return FALSE;
   }

   return TRUE;
}

/*
 * reads info from file into id3tag.
 */
int id3_read_file(char *dest, unsigned long size, FILE *fp, char *fn) { 
   if (fread(dest, size, 1, fp) != 1) {
      die("id3_read_file: Error reading %s", fn);
      fclose(fp);
      return FALSE;
   }

   if (ferror(fp) != 0) {
      die("id3_read_file: Error reading %s", fn);
      clearerr(fp);
      fclose(fp);   
      return FALSE;  
   }

   if (feof(fp)) {
      die("id3_read_file: Premature end of file in %s", fn);
      fclose(fp);
      return FALSE;
   }

   return TRUE;
}

/*
 * reads an id3tag from the open file fp. returns TRUE if successful and
 * FALSE otherwise. the id3 tag info is stored in ptrtag.
 */
int read_tag(FILE *fp, char *fn, ID3_tag *ptrtag) {  

   if (ptrtag == NULL)
      die("read_tag: ptrtag shoudnt be NULL, but it is\n");

   if (id3_seek_header(fp, fn) == FALSE)
      return FALSE;

   if (!id3_read_file(ptrtag->tag, (sizeof(ptrtag->tag)-1), fp, fn))
      return FALSE;

   if (strncmp(ptrtag->tag, "TAG", 3) != 0)
      return FALSE;

   if (!id3_read_file(ptrtag->songname, (sizeof(ptrtag->songname)-1), fp, fn))
      return FALSE;
   if (!id3_read_file(ptrtag->artist, (sizeof(ptrtag->artist)-1), fp, fn))
      return FALSE;
   if (!id3_read_file(ptrtag->album, (sizeof(ptrtag->album)-1), fp, fn))
      return FALSE;
   if (!id3_read_file(ptrtag->year, (sizeof(ptrtag->year)-1), fp, fn))
      return FALSE;
   if (!id3_read_file(ptrtag->comment, (sizeof(ptrtag->comment)-1), fp, fn))
      return FALSE;

   ptrtag->genre = fgetc(fp);

   if (ptrtag->genre == EOF) {
      fclose(fp);
      die("read_tag: Error reading %s", fn);
      return FALSE;
   }

   strtrim(ptrtag->artist, TAGLEN_ARTIST);
   ptrtag->artist[TAGLEN_ARTIST] = '\0';

   strtrim(ptrtag->songname, TAGLEN_SONG);
   ptrtag->songname[TAGLEN_SONG] = '\0';

   strtrim(ptrtag->album, TAGLEN_ALBUM);
   ptrtag->album[TAGLEN_ALBUM] = '\0';

   strtrim(ptrtag->year, TAGLEN_YEAR);
   ptrtag->year[TAGLEN_YEAR] = '\0';

   strtrim(ptrtag->comment, TAGLEN_COMMENT);
   if ((ptrtag->comment[TAGLEN_COMMENT - 2] == '\0') &&
       (ptrtag->comment[TAGLEN_COMMENT - 1] != '\0')) {
      ptrtag->track = ptrtag->comment[TAGLEN_COMMENT - 1];
   } else {
      ptrtag->track = 0;
      ptrtag->comment[TAGLEN_COMMENT] = '\0';
   }

   return TRUE;
}

/*
 * attempts to read an id3 tag from a file. sets item->id3 = NULL if 
 * configuration.ignoreID3 is TRUE.
 */
void getID3(ITEM *item) {

   FILE *mp3file;
   int result;
   ID3_tag *ptrtag;

   ptrtag = (ID3_tag *)malloc(sizeof(ID3_tag));
   if (ptrtag == NULL) die("getName: malloc failure\n");   

   if (configuration.ignoreID3 == FALSE) {
      mp3file = fopen(item->path, "r");

      result = read_tag(mp3file, item->path, ptrtag);

      fclose(mp3file);

      if (result == TRUE) {

         if ((strcmp(ptrtag->songname, "") != 0) &&
             (strcmp(ptrtag->artist, "") != 0)) {
            item->id3 = ptrtag;
         } else { /* nov27/2000 Victor Zandy <zandy@cs.wisc.edu> */
            free(ptrtag);
            item->id3 = NULL;
         }
      } else { /* nov27/2000 Victor Zandy <zandy@cs.wisc.edu> */
         free(ptrtag);
         item->id3 = NULL;
      }

   } else { /* ignore id3 tag and return filename (without .mp3) */
      free(ptrtag);
      item->id3 = NULL;
   }
}

/*
 * finds the total time for the current track based on header info and on
 * file size. this function calls time_per_frame and bits_per_frame.
 */
int get_time(AudioInfo *inf) {

   return ((int) (curSong->size / (inf->bitrate / 8)));

}

/*
 * checks if a given filename is an mp3 based on its extension. case
 * insensitive.
 */
int ismp3(char *name) {

   int ret = FALSE;
   int len = 0;
   len = strlen(name);

   if (len > 3) {
      if (strcasecmp(name+len-4, ".mp3") == 0)
         ret = TRUE;
   }
   return(ret);
}

/*
 * checks if a given filename is an oog based on its extension. case
 * insensitive. for future use with a backend supporting oog vorbis.
 */
int isoog(char *name) {

   int ret = FALSE;
   int len = 0;
   len = strlen(name);

   if (len > 3) {
      if (strcasecmp(name+len-4, ".oog") == 0)
         ret = TRUE;
   }
   return(ret);
}

/*
 * checks if a given filename is an m3u based on its extension. case
 * insensitive.
 */
int ism3u(char *name) {

   int ret = FALSE;
   int len = 0;
   len = strlen(name);

   if (len > 3) {
      if (strcasecmp(name+len-4, ".m3u") == 0)
         ret = TRUE;
   }
   return(ret);
}

/*
 * returns a string representing the input string padded out to the
 * supplied length. if the string is too long the string is truncated and
 * the last three chars of the truncated string are "...".
 */
char *strpad(ITEM *itm, int len) {
   char *ret = NULL;
   char *str = NULL;
   int i = 0;
   int slen = 0;

   ret = (char *)malloc(sizeof(char)*(len + 1));
   if (ret == NULL) die("strpad: malloc failure\n");

   str = (char *)malloc(sizeof(char)*MAX_STRLEN);
   if (str == NULL) die("strpad: malloc failure\n");

   if (itm->id3 != NULL) {
      sprintf(str, "%s - %s", itm->id3->artist, itm->id3->songname);
   } else {
      strcpy(str, itm->name);
   }

   slen = strlen(str);
   if (slen <= len) { /* string is shorter than len, so pad with blanks */
      if (str != NULL) {
         while (*(str + i) != '\0') {
            *(ret + i) = *(str + i);
            i++;
         }
      }

      while (i < len) {
         *(ret + i) = ' ';
         i++;
      }
      *(ret + len) = '\0';

   } else { /* string is longer than len, so truncate and add "..." */
      strncpy(ret, str, len - 3);
      ret[len-3] = '.';
      ret[len-2] = '.';
      ret[len-1] = '.';
      ret[len] = '\0';
   }

   free(str);
   return(ret);
}

/*
 * trims trailing ' ', '\n', or '\t' from the end of a string by replacing
 * these characters by '\0'. used to trim id3 tags.
 */
void strtrim(char *str, int len) {

   int i;

   if (str != NULL) {
      i = len - 1;

      /* if current char is ' ', '\n', or '\t' then replace it with '\0' */
      while (((*(str + i) == ' ') || (*(str + i) == '\n') ||
             (*(str + i) == '\t')) && (i >= 0)) {
         *(str + i) = '\0';
         i--;
      }
   }
}

/*
 * trims leading ' ', '\n', or '\t' from a string by shifting the string
 * over. should always result in a string of <= length to the original.
 */
void strleadtrim(char *str) {

   int i, j, len;

   len = strlen(str);
   i = 0;

   /* if current char is ' ', '\n', or '\t' then replace it with '\0' */
   while (((*(str + i) == ' ') || (*(str + i) == '\n') ||
          (*(str + i) == '\t')) && (i < len)) {
      i++;
   }

   if (i > 0) {
      for (j = 0; j < len - i; j++) {
         *(str + j) = *(str + j + i);
      }
      *(str + j) = '\0';
   }
}
