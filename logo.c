
#include "logo.h"
#include <string.h>
#include <ctype.h>

/*
 * function to read in the gamp logo.
 */
int readLogo(char *file, CONFIGURATION *config) {
   FILE *infile;
   int i = 0;
   int ret = 0;
   char buf[MAX_STRLEN];
   char *env_ptr = NULL;
   char *logo_file = NULL;

   config->logoHeight = 0;
   config->logoWidth = 0;

   if (file == NULL) { /* none specified. try one from config, then default */

      if (config->logoFile == NULL) {
         env_ptr = getenv("HOME");

         if (env_ptr != NULL) {
            logo_file = (char *)malloc(sizeof(char)*(strlen(env_ptr)+17));
            if (logo_file == NULL) die("readLogo: malloc failure\n");
            strcpy(logo_file, env_ptr);
            strcat(logo_file, "/.gamp/gamp.logo");
            config->logoFile = logo_file;
            debug("readLogo: no config specified. trying \"%s\"\n", logo_file);
         }

      } else {
         logo_file = config->logoFile;
      }
   } else {
      logo_file = file;
      config->logoFile = strdup(logo_file);
   }

   /* opens input file. dies if there is an error */
   if (!(infile = fopen(logo_file, "r"))) {
      debug("readLogo: error opening file %s\n", logo_file);

      initLogo(config);

      writeLogo(logo_file, config);

      ret = -1;
   } else { 

      debug("readLogo: found logo file \"%s\"\n", logo_file);
      config->logoFile = strdup(logo_file);

      fgets(buf, sizeof(buf), infile);
      sscanf(buf, "%d %d", &(config->logoHeight), &(config->logoWidth));

      config->logo = (char **)malloc(sizeof(char *) * config->logoHeight);
      if (config->logo == NULL) die("readLogo: malloc failure\n");

      for (i = 0; i < config->logoHeight; i++) {
         config->logo[i] = (char*)malloc(sizeof(char) *
                                         (config->logoWidth + 2));
         if (config->logo[i] == NULL) die("readLogo: malloc failure\n");
      }

      for (i = 0; i < config->logoHeight; i++) {

         fgets(config->logo[i], sizeof(char)*(config->logoWidth + 2), infile);
         strtrim(config->logo[i], strlen(config->logo[i]));

      }

      fclose(infile);
   }

   return(ret);
}

/*
 * writes the logo
 */
int writeLogo(char *file, CONFIGURATION *config) {

   int ret = 0;
   int i = 0;
   char *fptr = NULL;
   FILE *outfile;

   debug("writeLogo called\n");

   if (config != NULL) {

      if (file == NULL)
         fptr = config->logoFile;
      else
         fptr = file;

      if (!(outfile = fopen(fptr, "w"))) {
         debug("writeLogo: cant write logo file \"%s\"\n", fptr);
         ret = -1;

      } else {
         debug("writeLogo: writing to logo file \"%s\"\n", fptr);

         fprintf(outfile, "%d %d\n", config->logoHeight, config->logoWidth);

         for (i = 0; i < config->logoHeight; i++)
            fprintf(outfile, "%s\n", config->logo[i]);

         fclose(outfile);
      }     

   } else {
      debug("writeLogo: config NULL\n"); 
   }

   return(ret);

}

/*
 * initializes the logo to the default
 */
int initLogo(CONFIGURATION *config) {

   int i = 0;

   debug("initLogo called\n");

   if (config != NULL) {
      config->logoHeight = 7;
      config->logoWidth = 49;

      config->logo = (char **)malloc(sizeof(char *) * config->logoHeight);
      if (config->logo == NULL) die("initLogo: malloc failure\n");

      for (i = 0; i < config->logoHeight; i++) {

         config->logo[i] = (char *)malloc(sizeof(char)*config->logoWidth);
         if (config->logo[i] == NULL) die("initLogo: malloc failure\n");

      }

      strcpy(config->logo[0],
             "  ******     ***** ** *** *** ***   *** *****");
      strcpy(config->logo[1],
             "***    *** ***    *** ************* ***     ***");
      strcpy(config->logo[2],
             "***    *** ***    *** ***  ***  *** ***     ***");
      strcpy(config->logo[3],
             " *********   ***** ** ***  ***  *** *********");
      strcpy(config->logo[4],
             "       ***                          ***");
      strcpy(config->logo[5],
             "***    ***                          ***");
      strcpy(config->logo[6],
              "  ******                            ***");

   }

   return(0);
}
