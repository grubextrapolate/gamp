#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <sys/stat.h>
#include <unistd.h>

/*
 * reads config info from file into config. assumes config arg non-null
 */
int readPrefs(char *filename, CONFIGURATION *config) {

   int ret = 0; /* return value */
   FILE *infile; /* input file descriptor */
   char ptr[MAX_STRLEN]; /* input buffer */
   char *env_ptr = NULL;
   char *conf_file = NULL;

   if (filename == NULL) { /* no config file specified. try defaults */

      if (config->configFile == NULL) { /* none listed, try ~/gamp/gamprc */

         env_ptr = getenv("HOME");
 
         if (env_ptr != NULL) {
            conf_file = (char *)malloc(sizeof(char)*(strlen(env_ptr)+14));
            if (conf_file == NULL) die("readPrefs: malloc failure\n");
            strcpy(conf_file, env_ptr);
            strcat(conf_file, "/.gamp/gamprc");
            config->configFile = conf_file;
            debug("readPrefs: no config specified. trying \"%s\"\n", conf_file);
         } else {
            die("readPrefs: HOME undefined? somethings wrong...\n");
         }

      } else { /* use config specified in configuration structure */
         conf_file = config->configFile;
         debug("readPrefs: trying config->configFile \"%s\"\n", conf_file);
      }

   } else { /* try specified config file */
      conf_file = filename;
      config->configFile = strdup(conf_file);
      debug("readPrefs: trying specified file \"%s\"\n", conf_file);
   }

   if (!(infile = fopen(conf_file, "r"))) {
      /* no prefs. go with defaults */
      debug("readPrefs: cant open config file \"%s\"\n", conf_file);
      config->dirty = TRUE;

   } else {
      debug("readPrefs: found config file \"%s\"\n", conf_file);

      while(!feof(infile)) {

         fgets(ptr, sizeof(char)*MAX_STRLEN, infile);
         strtrim(ptr, strlen(ptr));
         strleadtrim(ptr);

         if ((*ptr != '\0') && (*ptr != '#')) {
            if (strncmp(ptr, "player=", 7) == 0) {
               if (*(ptr+7) != '\0') {
                  config->player.player = strdup(ptr + 7);
               } else {
                  if (config->player.player != NULL)
                     free(config->player.player);
                  config->player.player = NULL;
               }
            } else if (strncmp(ptr, "playerArg1=", 11) == 0) {
               if (*(ptr+11) != '\0') {
                  config->player.arg1 = strdup(ptr + 11);
               } else {
                  if (config->player.arg1 != NULL)
                     free(config->player.arg1);
                  config->player.arg1 = NULL;
               }
            } else if (strncmp(ptr, "playerArg2=", 11) == 0) {
               if (*(ptr+11) != '\0') {
                  config->player.arg2 = strdup(ptr + 11);
               } else {
                  if (config->player.arg2 != NULL)
                     free(config->player.arg2);
                  config->player.arg2 = NULL;
               }
            } else if (strncmp(ptr, "playerArg3=", 11) == 0) {
               if (*(ptr+11) != '\0') {
                  config->player.arg3 = strdup(ptr + 11);
               } else {
                  if (config->player.arg3 != NULL)
                     free(config->player.arg3);
                  config->player.arg3 = NULL;
               }
            } else if (strncmp(ptr, "playerArg4=", 11) == 0) {
               if (*(ptr+11) != '\0') {
                  config->player.arg4 = strdup(ptr + 11);
               } else {
                  if (config->player.arg4 != NULL)
                     free(config->player.arg4);
                  config->player.arg4 = NULL;
               }
            } else if (strncmp(ptr, "playerArg5=", 11) == 0) {
               if (*(ptr+11) != '\0') {
                  config->player.arg5 = strdup(ptr + 11);
               } else {
                  if (config->player.arg5 != NULL)
                     free(config->player.arg5);
                  config->player.arg5 = NULL;
               }
            } else if (strncmp(ptr, "displaySpectrum=", 16) == 0) {
               if (strcmp(ptr + 16, "TRUE") == 0)
                  config->displaySpectrum = TRUE;
               else if (strcmp(ptr + 16, "FALSE") == 0)
                  config->displaySpectrum = FALSE;

            } else if (strncmp(ptr, "timeMode=", 9) == 0) {
               if (strcmp(ptr + 9, "REMAINING") == 0)
                  config->timeMode = REMAINING;
               else if (strcmp(ptr + 9, "ELAPSED") == 0)
                  config->timeMode = ELAPSED;

            } else if (strncmp(ptr, "startWith=", 10) == 0) {
               if (strcmp(ptr + 10, "PLAYER") == 0)
                  config->startWith = PLAYER;
               else if (strcmp(ptr + 10, "PLAYLIST") == 0)
                  config->startWith = PLAYLIST;

            } else if (strncmp(ptr, "ignoreID3=", 10) == 0) {
               if (strcmp(ptr + 10, "TRUE") == 0)
                  config->ignoreID3 = TRUE;
               else if (strcmp(ptr + 10, "FALSE") == 0)
                  config->ignoreID3 = FALSE;

            } else if (strncmp(ptr, "expertMode=", 11) == 0) {
               if (strcmp(ptr + 11, "TRUE") == 0)
                  config->expert = TRUE;
               else if (strcmp(ptr + 11, "FALSE") == 0)
                  config->expert = FALSE;

            } else if (strncmp(ptr, "useColor=", 9) == 0) {
               if (strcmp(ptr + 11, "TRUE") == 0)
                  config->color = TRUE;
               else if (strcmp(ptr + 11, "FALSE") == 0)
                  config->color = FALSE;

            } else if (strncmp(ptr, "ignoreCase=", 11) == 0) {
               if (strcmp(ptr + 11, "TRUE") == 0)
                  config->ignoreCase = TRUE;
               else if (strcmp(ptr + 11, "FALSE") == 0)
                  config->ignoreCase = FALSE;

            } else if (strncmp(ptr, "repeatMode=", 11) == 0) {
               if (strcmp(ptr + 11, "repeatNone") == 0)
                  config->repeatMode = repeatNone;
               else if (strcmp(ptr + 11, "repeatOne") == 0)
                  config->repeatMode = repeatOne;
               else if (strcmp(ptr + 11, "repeatAll") == 0)
                  config->repeatMode = repeatAll;

            } else if (strncmp(ptr, "volUpCmd=", 9) == 0) {
               if (*(ptr+9) != '\0') {
                  config->volup = strdup(ptr + 9);
               } else {
                  if (config->volup != NULL)
                     free(config->volup);
                  config->volup = NULL;
               }

            } else if (strncmp(ptr, "volDownCmd=", 11) == 0) {
               if (*(ptr+9) != '\0') {
                  config->voldown = strdup(ptr + 11);
               } else {
                  if (config->voldown != NULL)
                     free(config->voldown);
                  config->voldown = NULL;
               }

            } else if (strncmp(ptr, "stepTimeout=", 12) == 0) {
               if (atoi(ptr + 12) > 0)
                  config->stepTimeout = atoi(ptr + 12);

            }
         }
      }
      fclose(infile);
   }

   return(ret);
}

/*
 * writes config info from config into file. assumes both args non-null
 */
int writePrefs(char *filename, CONFIGURATION *config) {
   int ret = 0;
   FILE *outfile; /* output file descriptor */
   char *fptr = NULL;

   debug("writePrefs called\n");

   if (config != NULL) {

      if (filename == NULL)
         fptr = config->configFile;
      else
         fptr = filename;

      if (!(outfile = fopen(fptr, "w"))) {
         debug("writePrefs: cant write config file \"%s\"\n", fptr);
         ret = -1;

      } else {
         debug("writePrefs: writing to config file \"%s\"\n", fptr);

         fprintf(outfile, "# name of the player\n");
         fprintf(outfile, "player=");
         if ((config->player.player != NULL) &&
             (config->player.player[0] != '\0'))
            fprintf(outfile, "%s\n", config->player.player);
         else
            fprintf(outfile, "\n");

         fprintf(outfile, "# first argument for the player\n");
         fprintf(outfile, "playerArg1=");
         if ((config->player.arg1 != NULL) &&
             (config->player.arg1[0] != '\0'))
            fprintf(outfile, "%s\n", config->player.arg1);
         else
            fprintf(outfile, "\n");

         fprintf(outfile, "# second argument for the player\n");
         fprintf(outfile, "playerArg2=");
         if ((config->player.arg2 != NULL) &&
             (config->player.arg2[0] != '\0'))
            fprintf(outfile, "%s\n", config->player.arg2);
         else
            fprintf(outfile, "\n");

         fprintf(outfile, "# third argument for the player\n");
         fprintf(outfile, "playerArg3=");
         if ((config->player.arg3 != NULL) &&
             (config->player.arg3[0] != '\0'))
            fprintf(outfile, "%s\n", config->player.arg3);
         else
            fprintf(outfile, "\n");

         fprintf(outfile, "# fourth argument for the player\n");
         fprintf(outfile, "playerArg4=");
         if ((config->player.arg4 != NULL) &&
             (config->player.arg4[0] != '\0'))
            fprintf(outfile, "%s\n", config->player.arg4);
         else
            fprintf(outfile, "\n");

         fprintf(outfile, "# fifth argument for the player\n");
         fprintf(outfile, "playerArg5=");
         if ((config->player.arg5 != NULL) &&
             (config->player.arg5[0] != '\0'))
            fprintf(outfile, "%s\n", config->player.arg5);
         else
            fprintf(outfile, "\n");

         fprintf(outfile, "# display spectrum? TRUE or FALSE\n");
         fprintf(outfile, "displaySpectrum=");
         if (config->displaySpectrum == TRUE)
            fprintf(outfile, "TRUE\n");
         else
            fprintf(outfile, "FALSE\n");

         fprintf(outfile, "# time mode REMAINING or ELAPSED\n");
         fprintf(outfile, "timeMode=");
         if (config->timeMode == REMAINING)
            fprintf(outfile, "REMAINING\n");
         else
            fprintf(outfile, "ELAPSED\n");

         fprintf(outfile, "# startup in PLAYER or PLAYLIST\n");
         fprintf(outfile, "startWith=");
         if (config->startWith == PLAYER)
            fprintf(outfile, "PLAYER\n");
         else
            fprintf(outfile, "PLAYLIST\n");

         fprintf(outfile, "# ignore ID3 tags? TRUE or FALSE\n");
         fprintf(outfile, "ignoreID3=");
         if (config->ignoreID3 == TRUE)
            fprintf(outfile, "TRUE\n");
         else
            fprintf(outfile, "FALSE\n");

         fprintf(outfile, "# expert mode? TRUE or FALSE\n");
         fprintf(outfile, "expertMode=");
         if (config->expert == TRUE)
            fprintf(outfile, "TRUE\n");
         else
            fprintf(outfile, "FALSE\n");

         fprintf(outfile, "# use color? TRUE or FALSE. ignored if term does not support color\n");
         fprintf(outfile, "useColor=");
         if (config->color == TRUE)
            fprintf(outfile, "TRUE\n");
         else
            fprintf(outfile, "FALSE\n");

         fprintf(outfile, "# ignore case when sorting? TRUE or FALSE\n");
         fprintf(outfile, "ignoreCase=");
         if (config->ignoreCase == TRUE)
            fprintf(outfile, "TRUE\n");
         else
            fprintf(outfile, "FALSE\n");

         fprintf(outfile, "# repeat? repeatNone, repeatOne, or repeatAll\n");
         fprintf(outfile, "repeatMode=");
         if (config->repeatMode == repeatNone)
            fprintf(outfile, "repeatNone\n");
         else if (config->repeatMode == repeatOne)
            fprintf(outfile, "repeatOne\n");
         else if (config->repeatMode == repeatAll)
            fprintf(outfile, "repeatAll\n");

         fprintf(outfile, "# command to execute to increase volume\n");
         fprintf(outfile, "volUpCmd=");
         if ((config->volup != NULL) &&
             (config->volup[0] != '\0'))
            fprintf(outfile, "%s\n", config->volup);
         else
            fprintf(outfile, "\n");

         fprintf(outfile, "# command to execute to decrease volume\n");
         fprintf(outfile, "volDownCmd=");
         if ((config->voldown != NULL) &&
             (config->voldown[0] != '\0'))
            fprintf(outfile, "%s\n", config->voldown);
         else
            fprintf(outfile, "\n");

         fprintf(outfile, "# delay (in milliseconds) between ffwd/rew steps\n");
         fprintf(outfile, "stepTimeout=%d\n", config->stepTimeout);

         fclose(outfile);

      }
   } else {
      debug("writePrefs: config NULL\n");
   }

   return(ret);
}

/*
 * dumps prefs to debug (so only seen if debugging is enabled)
 */
int dumpPrefs(CONFIGURATION *config) {

   debug("dumpPrefs: configFile=\"%s\"\n", config->configFile);
   debug("dumpPrefs: player=\"%s\"\n", config->player.player);
   debug("dumpPrefs: playerArg1=\"%s\"\n", config->player.arg1);
   debug("dumpPrefs: playerArg2=\"%s\"\n", config->player.arg2);
   debug("dumpPrefs: playerArg3=\"%s\"\n", config->player.arg3);
   debug("dumpPrefs: playerArg4=\"%s\"\n", config->player.arg4);
   debug("dumpPrefs: playerArg5=\"%s\"\n", config->player.arg5);
   debug("dumpPrefs: displaySpectrum=\"%d\"\n", config->displaySpectrum);
   debug("dumpPrefs: timeMode=\"%d\"\n", config->timeMode);
   debug("dumpPrefs: startWith=\"%d\"\n", config->startWith);
   debug("dumpPrefs: ignoreID3=\"%d\"\n", config->ignoreID3);
   debug("dumpPrefs: expert=\"%d\"\n", config->expert);
   debug("dumpPrefs: color=\"%d\"\n", config->color);
   debug("dumpPrefs: ignoreCase=\"%d\"\n", config->ignoreCase);
   debug("dumpPrefs: repeatMode=\"%d\"\n", config->repeatMode);
   debug("dumpPrefs: volup=\"%s\"\n", config->volup);
   debug("dumpPrefs: voldown=\"%s\"\n", config->voldown);
   debug("dumpPrefs: stepTimeout=%d\n", config->stepTimeout);

   return(0);

}

/*
 * initializes preferences to defaults.
 */
void initPrefs(CONFIGURATION *config) {

   /* start with default prefs */
   config->player.player = NULL;
   config->player.arg1 = NULL;
   config->player.arg2 = NULL;
   config->player.arg3 = NULL;
   config->player.arg4 = NULL;
   config->player.arg5 = NULL;

   config->player.player = strdup("sajberplay");
   config->player.arg1 = strdup("-q");
   config->player.buffer = 2048;
   config->player.playahead = 0;
   config->player.priority = PRIORITY_NORMAL;
   config->player.realtime = FALSE;  
   config->player.release = FALSE;
   config->player.forcetomono = FALSE;
   config->player.mixer = FALSE;

   config->displaySpectrum = FALSE;
   config->timeMode = REMAINING;
   config->startWith = PLAYER;
   config->ignoreID3 = FALSE;
   config->expert = FALSE;
   config->color = TRUE;
   config->ignoreCase = TRUE;
   config->repeatMode = repeatNone;

   config->volup = strdup("aumix -w +10");
   config->voldown = strdup("aumix -w -10");

   config->configFile = NULL;

   config->stepTimeout = 10;

}
