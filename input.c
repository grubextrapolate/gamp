#include "input.h"
#include <ctype.h>
#include <string.h>

#define ESC 27
#define ENT 13

/*
 * pops up a window to ask for a load/save filename
 */
char *getFilename(char *prompt) {
   int x_offset = 0, y_offset = 0;
   WINDOW *win = NULL;
   char *ret = NULL;
   int key = 0;
   int width = 0, height = 0, i = 0;
   int input_x = 0, scroll = 0;
   int box_x = 0, box_y = 0, box_width = 0;

   width = 60;
   height = 5;
   x_offset = (COLS - width) / 2;
   y_offset = (LINES - height) / 2;

   if (x_offset < 0) x_offset = 0;
   if (y_offset < 0) y_offset = 0;

   win = newwin(height, width, y_offset, x_offset);
   if (win == NULL) die("getFilename: newwin failure\n");
   box(win, 0, 0);

   box_x = 2, box_y = 2;
   mvwaddstr(win, box_y, box_x, prompt);

   wnoutrefresh(win);  
   doupdate();

   ret = (char *)malloc(sizeof(char)*MAX_STRLEN);
   if (ret == NULL) die("getFilename: malloc failure\n");

   box_x += strlen(prompt) + 1;
   box_width = width - box_x - 2;
   wmove(win, box_y, box_x + input_x);

   while ((key != ESC) && (key != ENT)) {

      key = wgetch (win);

      debug("getFilename: key = '%c' (ascii %d)\n", key, key);
      switch(key) {

         case KEY_BACKSPACE:
         case 127:
            if (input_x || scroll) {
               if (!input_x) {
                  scroll = scroll < box_width - 1 ?
                           0 : scroll - (box_width - 1);
                  wmove(win, box_y, box_x);  
                  for (i = 0; i < box_width; i++) {
                     waddch(win, ret[scroll + input_x + i] ?
                            ret[scroll + input_x + i] : ' ');
                  }
                  input_x = strlen(ret) - scroll;
               } else {
                   input_x--;
               }
               ret[scroll + input_x] = '\0';
               mvwaddch(win, box_y, input_x + box_x, ' ');
               wmove(win, box_y, input_x + box_x);
               wrefresh(win);
            }
            continue;
         default:
            if (key < 0x100 && isprint (key)) {
               if (scroll + input_x < MAX_STRLEN) {
                  ret[scroll + input_x] = key;
                  ret[scroll + input_x + 1] = '\0';
                  if (input_x == box_width - 1) {
                     scroll++;
                     wmove(win, box_y, box_x);
                     for(i = 0; i < box_width - 1; i++)
                        waddch(win, ret[scroll + i]);
                  } else {
                     wmove(win, box_y, input_x++ + box_x);
                     waddch(win, key);
                  }
                  wrefresh (win);
               } else {
                  flash ();       /* Alarm user about overflow */
               }
            }
            continue;
      }
   }

   if (key == ESC)
      ret = NULL;

   strtrim(ret, MAX_STRLEN);

   debug("getFilename: filename = \"%s\"\n", ret);

   delwin(win);

   touchwin(dirwin->win);
   touchwin(listwin->win);
   if (helpwin != NULL) touchwin(helpwin);
   refresh();
   wnoutrefresh(dirwin->win);
   wnoutrefresh(listwin->win);
   if (helpwin != NULL) wnoutrefresh(helpwin);

   doupdate();

   return(ret);
}

int confirm(char *prompt, int def) {

   int ret = def;
   WINDOW *win = NULL;
   int key = 0, width, height;
   int x_offset = 0, y_offset = 0;

   width = 60;
   height = 5;
   x_offset = (COLS - width) / 2;
   y_offset = (LINES - height) / 2;

   if (x_offset < 0) x_offset = 0;
   if (y_offset < 0) y_offset = 0;

   win = newwin(height, width, y_offset, x_offset);
   if (win == NULL) die("getFilename: newwin failure\n");
   box(win, 0, 0);

   mvwaddstr(win, 2, 2, prompt);

   wnoutrefresh(win);  
   doupdate();

   while ((key != ESC) && (key != '\n') &&
          (tolower(key) != 'y') && (tolower(key) != 'n')) {
      key = wgetch (win);
   }

   if (key == '\n')
      ret = def;
   else if ((key == ESC) ||(tolower(key) == 'n'))
      ret = FALSE;
   else /* (tolower(key) == 'y') */
      ret = TRUE;

   debug("confirm: key = %d, ret = %d\n", key, ret);

   delwin(win);

   if (func == PLAYLIST) {
      touchwin(dirwin->win);
      touchwin(listwin->win);
      if (helpwin != NULL) touchwin(helpwin);
      refresh();
      wnoutrefresh(dirwin->win);
      wnoutrefresh(listwin->win);
      if (helpwin != NULL) wnoutrefresh(helpwin);
   } else if (func == PLAYER) {
      if (helpwin != NULL) touchwin(helpwin);
      if (infowin->win != NULL) touchwin(infowin->win);
      refresh();
      if (helpwin != NULL) wnoutrefresh(helpwin);
      if (infowin->win != NULL) wnoutrefresh(infowin->win);
   }

   doupdate();

   return(ret);

}
