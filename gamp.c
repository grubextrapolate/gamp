/*
 * gamp.c v1.1.0
 * by grub <grub@extrapolation.net>
 *
 * ncurses based command line mp3 player. has a directory browser
 * and other fun stuff like those other x based clients, but for the
 * hardcore command line enthusiast.
 *
 */

#include "gamp.h"
#include "genre.h"
#include <sys/stat.h>

#include <unistd.h>
#include <sys/types.h>

#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <string.h>
#include <dirent.h>

/* our windows */
LISTWIN *dirwin = NULL; /* direcory list window (in editor) */
LISTWIN *listwin = NULL; /* listwin->list window (in editor) */

VOLWIN *volwin = NULL;
PROGWIN *progwin = NULL; /* progress/time/song window */

INFOWIN *infowin = NULL; /* info window */
HELPWIN *helpwin = NULL; /* popup help window */

/*
 * list of directory tree traversal so <-- can act as 'back' button in
 * playlist editor and work for multiple directory levels. browsing with
 * --> adds to tail of this list, browsing <-- will remove tail item.
 * this just means that the last item selected when browsing forward
 * will be selected when we go back. great help for large directories.
 */
ITEMLIST *treelist = NULL;

/* currenly playing song, or null if stopped */
ITEM *curSong = NULL;
ITEM *lastSong = NULL;

/* where the program currently is. one of listwin->list, player, quit */
int func = PLAYLIST;

/* directory to start in (NULL unless set on command line with '-d') */
char *sdir = NULL;

/* player configuration */
CONFIGURATION configuration;

int volleft;
int volright;

/*
 * initialize the given list with the contents of the specified directory.
 * this is called upon entering a new directory and upon entering the
 * listwin->list browser.
 */
void initDirlist(char *pwd, ITEMLIST **list) {
   DIR *dir;
   struct dirent *entry;
   struct stat dstat;
   char cwd[MAX_STRLEN];
   char tmpstr[MAX_STRLEN];

   if (pwd[0] == '\0') {
      getcwd(cwd, sizeof(cwd));
   } else {
      chdir(pwd);
      getcwd(cwd, sizeof(cwd));
   }
   freeList(list);
   initList(list);

   if ((dir = opendir(cwd)) == NULL)
      die("initDirlist: cant open dir \"%s\"\n", cwd);

   while((entry = readdir(dir))) {

      if (strcmp(".", entry->d_name) == 0)  continue;
      if (stat(entry->d_name, &dstat) != 0) continue;

      if (S_ISDIR(dstat.st_mode)) {
         sprintf(tmpstr, "%s/%s", cwd, entry->d_name);
         addItem(newItem(tmpstr, NULL), list);

      } else if(S_ISREG(dstat.st_mode)) {
         if (ismp3(entry->d_name))
            addItem(newItem(cwd, entry->d_name), list);

      } else {
         continue;
      }

   }

}

/*
 * initialization routine for the player. sets up the windows, displays
 * information if the track is already playing.
 */
void updateProgWin() {
   int i = 0, mymin = 0, mysec = 0;
   char tmpstr[MAX_STRLEN];

   if ((progwin != NULL) && (progwin->win != NULL)) {
      wmove(progwin->win, 0, 0);
      wclrtobot(progwin->win);
      box(progwin->win, 0, 0);

      if (! configuration.expert) {
         mvwaddstr(progwin->win, 0, 2, progwin->wname);
      }

      if (configuration.repeatMode == repeatOne)
         mvwaddstr(progwin->win, 1, 1, "r");
      else if (configuration.repeatMode == repeatAll)
         mvwaddstr(progwin->win, 1, 1, "R");

      if (curState & playState) {
         mvwaddstr(progwin->win, 1, 11, progwin->name);

      for (i = 0; i < (progwin->win->_maxx - 1); i++) {
         if (i < progwin->ipos)
            tmpstr[i] = '=';
         else if (i == progwin->ipos)
            tmpstr[i] = '|';
         else /* (i > progwin->ipos) */
            tmpstr[i] = '-';
      }
      tmpstr[progwin->win->_maxx - 1] = '\0';
      mvwaddstr(progwin->win, 2, 1, tmpstr);

      mymin = progwin->min;
      mysec = progwin->sec;
      if ((configuration.timeMode == ELAPSED) || ((curSong != NULL) && (curSong->info == NULL))) {
         if (mymin > 99) {
            mymin = 99;
            mysec = 59;
         }
         sprintf(tmpstr, "[%3d:%02d] ", mymin, mysec);
      } else {
         if (abs(mymin) > 99) {
            mymin = -99;
            mysec = 59;
         }
         if (mymin == 0) {
            sprintf(tmpstr, "[ -0:%02d] ", mysec);
         } else {
            sprintf(tmpstr, "[%3d:%02d] ", mymin, mysec);
         }
      }
      mvwaddstr(progwin->win, 1, 2, tmpstr);
      }

      wnoutrefresh(progwin->win);
      if (helpwin->active) {
         touchwin(helpwin->win);
         wnoutrefresh(helpwin->win);
      }
      doupdate();
   }
}

/*
 * the player function. this controls all the keyboard input while in
 * player mode. returns info about what part of the program to go to next.
 */
int playPlaylist() {

   int ch = ';';

   /* refresh screen */
   refresh();
   updateInfoWin();
   updateVolWin();
   updateProgWin();
   updateListWin(listwin);

   if (helpwin->active)
      updateHelpWin();

   doupdate();

   /* if we're in 'autoplay' mode via the '-p' command line switch */
   if (func == GOPLAY) {
      func = PLAYER;
      if (listwin->list != NULL) curSong = listwin->list->head;
      playerPlay(curSong);
   }

   while(func == PLAYER) {

      ch = getch();
      switch(ch) {
         case 'q': /* quit program */
            func = QUIT;
            break;

         case 'p': /* play */
         case ' ':
            if (!(curState & playState)) {
               if (curSong != NULL) playerPlay(curSong);
               else {
                  if (listwin->list != NULL) curSong = listwin->list->head;
                  playerPlay(curSong);
               }
            }
            break;

         case 'h': /* show/remove help window */
            toggleHelpWin();
            break;

         case 'f': /* ffwd */
            playerForward();
            timeout(configuration.stepTimeout);
            ch = '0';
            while ((ch != 'f') && (ch != 'p')) {
               ch = getch();
               playerStep();
            }
            timeout(-1);
            playerForward();
            break;

         case 'r': /* rew */
            playerRewind();
            timeout(configuration.stepTimeout);
            ch = '0';
            while ((ch != 'r') && (ch != 'p')) {
               ch = getch();
               playerStep();
            }
            timeout(-1);
            playerRewind();
            break;

         case 'n': /* next */
         case KEY_RIGHT:
            playerNext();
            break;

         case 'v': /* prev */
         case KEY_LEFT:
            playerPlay(getPrevSong());
            break;

         case 'a': /* pause */
            playerPause();
            break;

         case '+': /* increase volume (disabled) */
         case '=':
            volUp(&configuration);
            break;

         case '-': /* decrease volume (disabled) */
         case '_':
            volDown(&configuration);
            break;

         case 't': /* toggle time mode between elapsed/remaining */
            if (configuration.timeMode == ELAPSED)
               configuration.timeMode = REMAINING;
            else
               configuration.timeMode = ELAPSED;
            if (configuration.dirty == 0) configuration.dirty = 1;
            break;

         case 'R': /* cycle repeat mode through none/one/all */
            if (configuration.repeatMode == repeatNone) {
               configuration.repeatMode = repeatOne;
               mvwaddstr(progwin->win, 1, 1, "r");
            } else if (configuration.repeatMode == repeatOne) {
               configuration.repeatMode = repeatAll;
               mvwaddstr(progwin->win, 1, 1, "R");
            } else if (configuration.repeatMode == repeatAll) {
               configuration.repeatMode = repeatNone;
               mvwaddstr(progwin->win, 1, 1, " ");
            }
            wnoutrefresh(progwin->win);
            doupdate();
            if (configuration.dirty == 0) configuration.dirty = 1;
            break;

         case 's': /* stop */
            playerStop();
            updateProgWin();
            break;

         case 'l': /* back to listwin->list */
            func = PLAYLIST;
            break;

         case 12: /* ctrl-l (refresh) */
            wresize(listwin->win, listwin->height, COLS);
//            finishPlayer();
//            initPlayer();
            if (helpwin->active)
               updateHelpWin();
            break;

         default:
            break;

      }
      if (helpwin->active) {
         touchwin(helpwin->win);
         wnoutrefresh(helpwin->win);
      }
      doupdate();

   }

   return(func);
}

void volUp(CONFIGURATION *config) {

   if ((config->volup != NULL) && (config->volup[0] != '\0')) {
      system(config->volup);
      volwin->vol += volwin->incr;
      if (volwin->vol > volwin->max) volwin->vol = volwin->max;
      updateVolWin();
   }
}

void volDown(CONFIGURATION *config) {

   if ((config->voldown != NULL) && (config->voldown[0] != '\0')) {
      system(config->voldown);
      volwin->vol -= volwin->incr;
      if (volwin->vol < 0) volwin->vol = 0;
      updateVolWin();
   }
}

void initVolume(VOLWIN *volwin) {

//   char buf[MAX_STRLEN];
//   int mrec[3];
// volwin->vol = system("aumix -w q | awk '{print $2}' | awk -F ',' '{print $1}'";

   volwin->vol = 50;

//   if ((fork_pid = fork()) == 0) {
//      dup2(mrec[0], STDOUT_FILENO);
//      close(mrec[0]);
//      close(mrec[1]);
//      system("aumix -w q | awk '{print $2}' | awk -F ',' '{print $1}'");

//      read(mrec[0], buf, sizeof(buf));
//      debug("initVolume: buf=\"%s\"\n", buf);
//   }
}

void updateVolWin() {

   int i, maxy, maxx, height;
   char tmpstr[MAX_STRLEN];

   if (volwin->win != NULL) {
      maxy = volwin->height - 2;
      maxx = volwin->width - 2;

      for (i = 0; i < maxx; i++)
         tmpstr[i] = '*';
      tmpstr[maxx] = '\0';

      height = (maxy*volwin->vol)/volwin->max;

      wmove(volwin->win, 0, 0);
      wclrtobot(volwin->win);
      box(volwin->win, 0, 0);

      if (! configuration.expert) {
         mvwaddstr(volwin->win, 0, 2, volwin->wname);
      }

      if (height > 0) {
         for (i = 0; i < height; i++)
            mvwaddstr(volwin->win, maxy - i, 1, tmpstr);
      }
      wnoutrefresh(volwin->win);
      doupdate();
   }
}

/*
 * writes num entries from list begining with first into win. will show
 * sel item with a "-" at each end and in reverse if the current window 
 * is 'active'. will put a ">" at the left hand end of the song
 * indicated by play.
 */
void fillwin(LISTWIN *lw, ITEM *first, int num, 
             ITEM *sel, ITEM *play, int active) {

   ITEM *cur = NULL;
   int i = 0;
   char *tmpstr = NULL;
   int h = 0;
   double hd = 0;
   double frac = 0;
   int n = 0;
   int off = 0;

   wmove(lw->win, 0, 0);
   wclrtobot(lw->win);
   box(lw->win, 0, 0);
   if (! configuration.expert) {
      mvwaddstr(lw->win, 0, 2, lw->wname);
   }
   cur = first;
   if ((lw->list != NULL) && (lw->list->num != 0)) {
      while ((i < num) && (cur != NULL)) {

         if (cur == sel) {
            mvwaddstr(lw->win, i+1, 1, "-");
            if (active) wattrset(lw->win, A_REVERSE);
         }
         if (cur->marked) mvwaddstr(lw->win, i+1, 1, "*");
         if (cur == play) mvwaddstr(lw->win, i+1, 1, ">");
         tmpstr = strpad(cur, lw->win->_maxx - 4);
         mvwaddstr(lw->win, i+1, 2, tmpstr);
         free(tmpstr);
         if (cur == sel) {
            if (active) wattrset(lw->win, A_NORMAL);
            mvwaddstr(lw->win, i+1, lw->win->_maxx - 2, "-");
         }
         cur = cur->next;
         i++;
      }

      frac = ((double)num)/lw->list->num;
      if (frac > 1) frac = 1;
      hd = frac * num;
      h = (int) hd;
      if (hd < 1) h = 1;

      n = 0;
      cur = first;
      while (cur != NULL) {
         n = n + 1;
         cur = cur->prev;
      }

      hd = frac * (n);
      off = (int) (hd+0.5);
      if (n <= 1) off = 0;
      if (n + num > lw->list->num) off = num - h;

      for (i = 0; i < num; i++) {
         wattrset(lw->win, A_REVERSE);
         if (i < off)
            mvwaddstr(lw->win, i+1, lw->win->_maxx - 1, " ");
         else if (i < (off+h))
            mvwaddstr(lw->win, i+1, lw->win->_maxx - 1, "#");
         else
            mvwaddstr(lw->win, i+1, lw->win->_maxx - 1, " ");
         wattrset(lw->win, A_NORMAL);
      }
   }

   wnoutrefresh(lw->win);
   doupdate();
}

/*
 * routine to update a window and fill it with relevant info
 */
void updateListWin(LISTWIN *win) {
   int pos = (win->win->_maxy - 2)/2;
   if ((win != NULL) && (win->win != NULL)) {
      if (func == PLAYLIST) {
         fillwin(win, win->first, win->win->_maxy - 1, win->cur,
                 curSong, win->active);
      } else {
         fillwin(win, seekBackItem(curSong, &pos), win->win->_maxy - 1,
                 curSong, NULL, TRUE);
      }
      if (helpwin->active) {
         touchwin(helpwin->win);
         wnoutrefresh(helpwin->win);
      }
      doupdate();
   }
}

/*
 * the listwin->list editor. sets up the windows and allows you to create/alter
 * your listwin->list.
 */
int editPlaylist() {

   int ch = ';';
   char cwd[MAX_STRLEN];

   int i = 0;        /* loop counter */
   ITEM *itm = NULL; /* loop pointer */
   char *filename = NULL; /* load/save filename */
   char *cptr = NULL;

   if (dirwin->list == NULL) {
      initDirlist("", &dirwin->list);
      sortList(dirwin->list);

      if (dirwin->list != NULL) {
         if (treelist != NULL) {
            dirwin->cur = seekItemByPath(treelist->tail, dirwin->list);
            if (dirwin->cur != NULL) {
               dirwin->first = seekItemByPath(treelist->tail->prev, dirwin->list);
               dirwin->pos = treelist->tail->prev->length;
            } else {
               dirwin->first = dirwin->list->head;
               dirwin->cur = dirwin->list->head;
            }
         } else {
            dirwin->first = dirwin->list->head;
            dirwin->cur = dirwin->list->head;
         }
      }
   }

   /* draw boxes in windows and fill them with something */
   refresh();
   updateProgWin();
   updateListWin(dirwin);
   updateListWin(listwin);

   if (helpwin->active)
      updateHelpWin();

   doupdate();

   while (func == PLAYLIST) {

      move(0, 0);

      ch = getch();
      switch(ch) {

         case 'P': /* start playing! */
            if (!(curState & playState)) {
               if (curSong != NULL) playerPlay(curSong);
               else {
                  if (listwin->list != NULL) curSong = listwin->list->head;
                  playerPlay(curSong);
               }
            }
            break;

         case 'p': /* switch to player */
            func = PLAYER;
            break;

         case 'q': /* quit program */
            func = QUIT;
            break;

         case 's': /* save listwin->list */
            filename = getFilename("save filename (ESC aborts):");
            if (filename != NULL) {
               if (exists(filename)) {
                  if (confirm("file exists. overwrite ([y]/n)?", TRUE))
                     writeM3u(filename);
               } else {
                  writeM3u(filename);
               }
            }
            break;

         case 'l': /* load listwin->list */
            filename = getFilename("load filename (ESC aborts):");
            if (filename != NULL) {
               if (readM3u(filename) == 0) {

                  if ((listwin->first == NULL) && (listwin->list != NULL))
                     listwin->first = listwin->list->head;
                  if ((listwin->cur == NULL) && (listwin->list != NULL))
                     listwin->cur = listwin->list->head;

                 updateListWin(listwin); 
               }
            }
            break;

         case 'm': /* mark/unmark current item */
            if (dirwin->active) {
               if (strcmp(dirwin->cur->name, "..") != 0) {
                  dirwin->cur->marked = ! dirwin->cur->marked;
                  updateListWin(dirwin);
               }
            } else { /* listwin->active */
               listwin->cur->marked = ! listwin->cur->marked;
               updateListWin(listwin);
            }
            break;

         case 'h': /* toggle help window */
            toggleHelpWin();
            break;

         case 'c': /* clear the listwin->list */
            if (curState & playState) {
               playerStop();
            }

            lastSong = NULL;
            curSong = NULL;

            freeList(&listwin->list);
            listwin->list = NULL;

            listwin->pos = 0;
            listwin->cur = NULL;
            listwin->first = NULL;
            wmove(listwin->win, 0, 0);
            wclrtobot(listwin->win);
            box(listwin->win, 0, 0);
            break;

         case 'a': /* add all in current directory */
            if (dirwin->list != NULL) {
               itm = dirwin->list->head;
               while (itm != NULL) {
                  if (itm->isfile) addItem(copyItem(itm), &listwin->list);
                  itm = itm->next;
               }
               if (listwin->list != NULL) {
                  if (listwin->first == NULL) listwin->first = listwin->list->head;
                  if (listwin->cur == NULL) listwin->cur = listwin->list->head;
               }
               updateListWin(listwin);
            }
            break;

         case 'A': /* add all recursively */
            if (dirwin->list != NULL) {
               getcwd(cwd, sizeof(cwd));
               addRecursive(dirwin->list, &listwin->list);
               if (listwin->list != NULL) {
                  if (listwin->first == NULL) listwin->first = listwin->list->head;
                  if (listwin->cur == NULL) listwin->cur = listwin->list->head;
               }
               chdir(cwd);
               updateListWin(listwin);
            }
            break;

         case ' ': /* scroll down by one page (if possible) */
            if (dirwin->active) {
               if ((dirwin->list != NULL) && (dirwin->cur != dirwin->list->tail)) {
                  i = 0;
                  itm = dirwin->first;
                  while ((i < dirwin->win->_maxy - 1) && (itm != NULL)) {
                     i++;
                     itm = itm->next;
                  }

                  if (itm != NULL) {
                     dirwin->first = itm;
                     dirwin->cur = itm;
                     dirwin->pos = 0;
                  } else {
                     dirwin->pos = i - 1;
                     dirwin->cur = dirwin->list->tail;
                  }

                  updateListWin(dirwin);
               }
            } else { /* listwin->active */
               if ((listwin->list != NULL) && (listwin->cur != listwin->list->tail)) {
                  i = 0;
                  itm = listwin->first;
                  while ((i < listwin->win->_maxy - 1) && (itm != NULL)) {
                     i++;
                     itm = itm->next;
                  }

                  if (itm != NULL) {
                     listwin->first = itm;
                     listwin->cur = itm;
                     listwin->pos = 0;
                  } else {
                     listwin->pos = i - 1;
                     listwin->cur = listwin->list->tail;
                  }

                  updateListWin(listwin);
               }
            }
            break;

         case '-': /* scroll up by one page (if possible) */
            if (dirwin->active) {
               if ((dirwin->list != NULL) && (dirwin->cur != dirwin->list->head)) {
                  i = 0;
                  itm = dirwin->first;
                  while ((i < dirwin->win->_maxy - 1) && (itm != NULL)) {
                     i++;
                     itm = itm->prev;
                  }

                  if (itm != NULL) {
                     dirwin->first = itm;
                     dirwin->cur = itm;
                     dirwin->pos = 0;
                  } else {
                     dirwin->pos = 0;
                     dirwin->first = dirwin->list->head;
                     dirwin->cur = dirwin->list->head;
                  }

                  updateListWin(dirwin);
               }
            } else { /* listwin->active */
               if ((listwin->list != NULL) && (listwin->cur != listwin->list->head)) {
                  i = 0;
                  itm = listwin->first;
                  while ((i < listwin->win->_maxy - 1) && (itm != NULL)) {
                     i++;
                     itm = itm->prev;
                  }

                  if (itm != NULL) {
                     listwin->first = itm;
                     listwin->cur = itm;
                     listwin->pos = 0;
                  } else {
                     listwin->pos = 0;
                     listwin->first = listwin->list->head;
                     listwin->cur = listwin->list->head;
                  }

                  updateListWin(listwin);
               }
            }
            break;

         case KEY_UP: /* move up in window, scroll if needed */
            if (dirwin->active) {
               if ((dirwin->cur != NULL) && (dirwin->cur->prev != NULL)) {
                  if (dirwin->pos > 0) {
                     dirwin->pos--;
                     dirwin->cur = dirwin->cur->prev;
                  } else {
                     if (dirwin->first != NULL) dirwin->first = dirwin->first->prev;
                     dirwin->cur = dirwin->cur->prev;
                  }
                  updateListWin(dirwin);
               }
            } else { /* listwin->active */
               if ((listwin->cur != NULL) && (listwin->cur->prev != NULL)) {
                  if (listwin->pos > 0) {
                     listwin->pos--;
                     listwin->cur = listwin->cur->prev;
                  } else {
                     if (listwin->first != NULL) listwin->first = listwin->first->prev;
                     listwin->cur = listwin->cur->prev;
                  }
                  updateListWin(listwin);
               }
            }
            break;

         case KEY_DOWN: /* move down in window, scroll if needed */
            if (dirwin->active) {
               if ((dirwin->cur != NULL) && (dirwin->cur->next != NULL)) {
                  if (dirwin->pos < (dirwin->win->_maxy - 2)) { /* no scroll */
                     dirwin->pos++;
                     dirwin->cur = dirwin->cur->next;
                  } else { /* scroll */
                     if (dirwin->first != NULL) dirwin->first = dirwin->first->next;
                     dirwin->cur = dirwin->cur->next;
                  }
                  updateListWin(dirwin);
               }
            } else { /* listwin->active */
               if ((listwin->cur != NULL) && (listwin->cur->next != NULL)) {
                  if (listwin->pos < (listwin->win->_maxy - 2)) { /* no scroll */
                     listwin->pos++;
                     listwin->cur = listwin->cur->next;
                  } else { /* scroll */
                     if (listwin->first != NULL) listwin->first = listwin->first->next;
                     listwin->cur = listwin->cur->next;
                  }
                  updateListWin(listwin);
               }
            }
            break;

         case KEY_LEFT: /* remove item (only if in listwin->list win) */
            if (dirwin->active) {
               if ((treelist != NULL) && (treelist->tail != NULL)) {
                  strcpy(cwd, treelist->tail->path);
                  cptr = rindex(cwd, '/');
                  *(cptr+1) = '\0';
                  initDirlist(cwd, &dirwin->list);
               } else {
                  initDirlist("..", &dirwin->list);
               }
               sortList(dirwin->list);
               dirwin->pos = 0;
               if (dirwin->list != NULL) {
                  if (treelist != NULL) {
                     dirwin->cur = seekItemByPath(treelist->tail, dirwin->list);
                     if (dirwin->cur != NULL) {
                        popItem(&treelist);
                        dirwin->first = seekItemByPath(treelist->tail,
                                                  dirwin->list);
                        dirwin->pos = treelist->tail->length;
                        popItem(&treelist);
                     } else {
                        dirwin->first = dirwin->list->head;
                        dirwin->cur = dirwin->list->head;
                     }
                  } else {
                     dirwin->first = dirwin->list->head;
                     dirwin->cur = dirwin->list->head;
                  }
               }
               updateListWin(dirwin);

            } else if ((listwin->active) && (listwin->cur != NULL)) {
               /* if we are currently playing then we need to adjust
                  the pointer to the current playing song */
               if (curSong == listwin->cur) { 
                  if (listwin->cur->next != NULL) playerNext();
                  else if (listwin->cur->prev != NULL) playerPrev();
                  else { playerStop(); curSong = NULL; lastSong = NULL; }
               }
               itm = listwin->cur;

               if (listwin->cur->next != NULL) listwin->cur = listwin->cur->next;
               else if (listwin->cur->prev != NULL) listwin->cur = listwin->cur->prev;
               else listwin->cur = NULL;

               if (listwin->pos == 0) {
                  if (listwin->first->next != NULL) listwin->first = listwin->first->next;
                  else if (listwin->first->prev != NULL) listwin->first = listwin->first->prev;
                  else listwin->first = NULL;
               }

               if ((itm->next == NULL) && (listwin->pos != 0)) listwin->pos--;

               deleteItem(itm, &listwin->list);
               updateListWin(listwin);
            }
            break;

         case KEY_RIGHT: /* browse to new dir or add to listwin->list*/
            if (dirwin->active) {
               if (isfile(dirwin->cur)) {
                  addItem(copyItem(dirwin->cur), &listwin->list);
                  if (listwin->first == NULL) listwin->first = listwin->list->head;
                  if (listwin->cur == NULL) listwin->cur = listwin->list->head;
                  updateListWin(listwin);
               } else {
                  addItem(newItem(dirwin->first->path, NULL), &treelist);
                  treelist->tail->length = dirwin->pos;
                  addItem(newItem(dirwin->cur->path, NULL), &treelist);
                  initDirlist(dirwin->cur->name, &dirwin->list);
                  sortList(dirwin->list);
                  dirwin->pos = 0;
                  if (dirwin->list != NULL) {
                     dirwin->first = dirwin->list->head;
                     dirwin->cur = dirwin->list->head;
                  }
                  updateListWin(dirwin);
               }
            } else { /* listwin->active */
               if (listwin->cur != NULL) {
                  playerPlay(listwin->cur);
                  updateListWin(listwin);
               }
            }
            break;

         case '\t': /* use tab to move between windows */
            if (listwin->active) {
               listwin->active = FALSE;
               dirwin->active = TRUE;
               updateListWin(dirwin);
               updateListWin(listwin);
            } else { /* dirwin->active */
               if (listwin->first != NULL) {
                  dirwin->active = FALSE;
                  listwin->active = TRUE;
                  updateListWin(dirwin);
                  updateListWin(listwin);
               }
            }
            break;

         case 'r': /* r to randomize the listwin->list */
            listwin->list = randomizeList(&listwin->list);
            if (listwin->list != NULL) {
               listwin->first = seekBackItem(listwin->cur, &listwin->pos);
            }
            updateListWin(listwin);
            break;

         case 'o': /* sort (order) the listwin->list */
            sortList(listwin->list);
            if (listwin->list != NULL) {
               listwin->first = seekBackItem(listwin->cur, &listwin->pos);
            }
            updateListWin(listwin);
            break;

         case 'u': /* move song up in listwin->list */
            if (listwin->active) {
               if ((listwin->list != NULL) && (listwin->cur != NULL) &&
                   (listwin->cur->prev != NULL)) {
                  swapItems(listwin->list, listwin->cur->prev, listwin->cur);

                  if (listwin->pos > 0) listwin->pos--;
                  if (listwin->pos == 0) listwin->first = listwin->cur;

                  updateListWin(listwin);
               }
            }
            break;

         case 'd': /* move song down in listwin->list */
            if (listwin->active) {
               if ((listwin->list != NULL) && (listwin->cur != NULL) &&
                   (listwin->cur->next != NULL)) {
                  swapItems(listwin->list, listwin->cur, listwin->cur->next);

                  if (listwin->pos == 0) listwin->first = listwin->cur->prev;
                  if (listwin->pos < (listwin->win->_maxy - 2)) listwin->pos++;
                  else listwin->first = listwin->first->next;

                  updateListWin(listwin);
               }
            }
            break;

         case 'C': /* crop to marked */
            if (listwin->cur->marked) {
               cropList(&listwin->list);
               listwin->first = seekBackItem(listwin->cur, &listwin->pos);
            } else {
               cropList(&listwin->list);
               listwin->pos = 0;
               listwin->first = listwin->list->head;
               listwin->cur = listwin->first;
            }
            updateListWin(listwin);
            break;

         case 'M': /* mark all */
            if (dirwin->active) {
               markAll(dirwin->list);
               updateListWin(dirwin);
            } else { /* listwin->active */
               markAll(listwin->list);
               updateListWin(listwin);
            }
            break;

         case 'U': /* unmark all */
            if (dirwin->active) {
               unmarkAll(dirwin->list);
               updateListWin(dirwin);
            } else { /* listwin->active */
               unmarkAll(listwin->list);
               updateListWin(listwin);
            }
            break;

         case 'D': /* delete marked */
            if (listwin->cur->marked) {
               deleteMarked(&listwin->list);
               listwin->first = seekBackItem(listwin->cur, &listwin->pos);
            } else {
               deleteMarked(&listwin->list);
               listwin->pos = 0;
               listwin->first = listwin->list->head;
               listwin->cur = listwin->first;
            }
            updateListWin(listwin);
            break;

         case 'z': /* add marked */
            addMarked(dirwin->list, &listwin->list);
            if ((listwin->cur == NULL) && (listwin->list != NULL)) {
               listwin->cur = listwin->list->head;
               listwin->first = listwin->cur;
            }
            updateListWin(listwin);
            break;

         case 'Z': /* add marked recursive */
            addMarkedRecursive(dirwin->list, &listwin->list);
            if ((listwin->cur == NULL) && (listwin->list != NULL)) {
               listwin->cur = listwin->list->head;
               listwin->first = listwin->cur;
            }
            updateListWin(listwin);
            break;

         case 'e': /* replace */
            if ((listwin->cur != NULL) && (isfile(dirwin->cur))) {
               itm = listwin->cur->prev;
               replaceItem(listwin->cur, &listwin->list, dirwin->cur);
               listwin->cur = itm->next;
               updateListWin(listwin);
            }
            break;

         case 'i': /* insert after current */
            if (dirwin->active) {
              if ((dirwin->cur != NULL) && isfile(dirwin->cur)) {
                 itm = copyItem(dirwin->cur);
                 insertAfterItem(itm, listwin->cur, &(listwin->list));
                 updateListWin(listwin);
               }
            }
            break;

         case 'I': /* insert marked after current */
            if (dirwin->active) {
              if ((dirwin->cur != NULL) && isfile(dirwin->cur)) {
                 insertMarkedAfterItem(dirwin->list, listwin->cur,
                                       &(listwin->list));
                 updateListWin(listwin);
               }
            }
            break;

         case 'V': /* invert selection */
            if (dirwin->active) {
               markInvert(dirwin->list);
               updateListWin(dirwin);
            } else { /* listwin->active */
               markInvert(listwin->list);
               updateListWin(listwin);
            }
            break;

         case 'v': /* reverse list */
            reverseList(listwin->list);
            listwin->first = seekBackItem(listwin->cur, &listwin->pos);
            updateListWin(listwin);
            break;

         case 't': /* move song to top of listwin->list */
            if ((listwin->active) && (listwin->list != NULL)) {
               moveItemToHead(listwin->cur, &listwin->list);
               listwin->first = seekBackItem(listwin->cur, &listwin->pos);
               updateListWin(listwin);
            }
            break;

         case 'b': /* move song to bottom of listwin->list */
            if ((listwin->active) && (listwin->list != NULL)) {
               moveItemToTail(listwin->cur, &listwin->list);
               listwin->first = seekBackItem(listwin->cur, &listwin->pos);
               updateListWin(listwin);
            }
            break;

         case 'T': /* move marked songs to top of listwin->list */
            if (listwin->active) {
               moveMarkedToHead(&listwin->list);
               listwin->first = seekBackItem(listwin->cur, &listwin->pos);
               updateListWin(listwin);
            }
            break;

         case 'B': /* move marked songs to bottom of listwin->list */
            if (listwin->active) {
               moveMarkedToTail(&listwin->list);
               listwin->first = seekBackItem(listwin->cur, &listwin->pos);
               updateListWin(listwin);
            }
            break;

         case 12: /* ctrl-l (refresh) */
            updateListWin(dirwin);
            updateListWin(listwin);
            if (helpwin->active)
               updateHelpWin();
            break;

         default:
            break;
      }
      wnoutrefresh(dirwin->win);
      wnoutrefresh(listwin->win);
      if (helpwin->active) {
         touchwin(helpwin->win);
         wnoutrefresh(helpwin->win);
      }
      doupdate();

   }

   return(func);

}

/*
 * adds the given mp3 filename onto the listwin->list (if file exists).
 */
int addMp3(char *mp3) {
   char cwd[MAX_STRLEN];
   ITEM *item = NULL;
   char *ptr = NULL;
   struct stat dstat;
   int ret = 0;

   if (strlen(mp3) > 3) {

      ptr = (char *)malloc(sizeof(char)*MAX_STRLEN);
      if (ptr == NULL) die("addMp3: malloc failure\n");

      if (mp3[0] != '/') {
         getcwd(cwd, MAX_STRLEN);
         sprintf(ptr, "%s/%s", cwd, mp3);
      } else {
         sprintf(ptr, "%s", mp3);
      }

      item = (ITEM *)malloc(sizeof(ITEM));
      if (item == NULL) die("addMp3: malloc failure\n");

      item->path = ptr;
      item->name = rindex(ptr, '/');
      item->info = NULL;
      item->next = NULL;
      item->prev = NULL;
      item->isfile = TRUE;

      if (item->name != NULL) item->name++;
      else die("addMp3: invalid filename \"%s\"\n", ptr);

      if(stat(item->path, &dstat) == 0) {
         getID3(item);
         addItem(item, &listwin->list);
         debug("addMp3: added %s\n", item->name);
      } else {
         free(ptr);
         free(item);
      }
   }

   return(ret);
}

/*
 * reads in a .m3u listwin->list file and adds entries into the internal
 * listwin->list.
 */
int readM3u(char *filename) {

   int ret = 0;
   FILE *infile = NULL;
   char *ptr = NULL;
   char *err = NULL;
   char *tmp = NULL;
   ITEM *item = NULL;
   struct stat dstat;

   if(!(infile = fopen(filename, "r"))) {
      debug("readM3u: cant open listwin->list \"%s\"\n", filename);
      ret = -1;
   } else {
      debug("readM3u: reading \"%s\"\n", filename);
      while(!feof(infile)) {

         ptr = (char *)malloc(sizeof(char)*MAX_STRLEN);
         if (ptr == NULL) die("readM3u: malloc error\n");

         err = fgets(ptr, sizeof(char)*MAX_STRLEN, infile);

         if (err != NULL) {

            tmp = index(ptr, '\n');
            if (tmp != NULL) *tmp = '\0';

            if (*ptr != '#') {
               debug("readM3u: reading line \"%s\"\n", ptr);

               item = (ITEM *)malloc(sizeof(ITEM));
               if (item == NULL) die("readM3u: malloc failure\n");

               item->path = ptr;
               item->name = rindex(ptr, '/');
               item->info = NULL;
               item->next = NULL;
               item->prev = NULL;
               item->isfile = TRUE;

               if (item->name != NULL) item->name++;
               else item->name = item->path;

               if (strncmp(item->path, "http://", 7) == 0) {
                  debug("readM3u: looks like url, not stating \"%s\"\n", item->path);
                  item->id3 = NULL;
                  addItem(item, &listwin->list);
                  debug("readM3u: added %s\n", item->path);
               } else if (stat(item->path, &dstat) == 0) {
                  getID3(item);
                  addItem(item, &listwin->list);
                  debug("readM3u: added %s\n", item->name);
               } else {
                  debug("readM3u: cant stat \"%s\"\n", item->path);

                  free(ptr);
                  free(item);
               }
            } else {
               debug("readM3u: skipping \"%s\"\n", ptr);
            }
         }
      }

      fclose(infile);
      ret = 0;
   }

   return(ret);

}

/*
 * writes a .m3u listwin->list file from the internal listwin->list.
 */
int writeM3u(char *filename) {

   int ret = 0;
   FILE *outfile = NULL;
   ITEM *item = NULL;

   if(!(outfile = fopen(filename, "w"))) {
      debug("writeM3u: cant open listwin->list \"%s\" for writing\n", filename);
      ret = -1;
   } else {

      if (listwin->list != NULL) {
         item = listwin->list->head;

         while(item != NULL) {
            fprintf(outfile, "%s\n", item->path);
            item = item->next;
         }

      }

      fclose(outfile);
      ret = 0;
   }

   return(ret);

}

/*
 * updates parts of the current songs AudioInfo struct
 */
void updateInfo(AudioInfo *data, int info) {
   debug("updateInfo called\n");
   if ((curSong != NULL) && (curSong->info != NULL)) {
      if (info == INFO_BITRATE)
         curSong->info->bitrate = data->bitrate;
      else if (info == INFO_FREQUENCY)
         curSong->info->frequency = data->frequency;
      else if (info == INFO_MODE)
         curSong->info->mode = data->mode;
      else if (info == INFO_CRC)
         curSong->info->crc = data->crc;
      else if (info == INFO_COPYRIGHT)
         curSong->info->copyright = data->copyright;
      else if (info == INFO_ORIGINAL)
         curSong->info->original = data->original;
      else if (info == INFO_IDENT)
         if (curSong->info->ident != NULL) free(curSong->info->ident);
         curSong->info->ident = strdup(data->ident);
   }
}

/*
 * sets the audio info for the current song and calls updateInfoWin to
 * update the info window if it is being displayed.
 */
void updateAudioInfo(AudioInfo *inf) {

   /* neither should never be null if this is being called...  */
   if ((curSong != NULL) && (inf != NULL)) {
      curSong->info = (AudioInfo *)malloc(sizeof(AudioInfo));
      memcpy(curSong->info, inf, sizeof(AudioInfo));
      curSong->length = get_time(inf);
      updateInfoWin();
      updateListWin(listwin);
   }
}

/*
 * updates the song time display
 */
void updateSongTime(int cur_frame) {
   int minutes = 0;
   int seconds = 0;

   if ((progwin != NULL) && (curSong != NULL)) {
      seconds = (cur_frame*32)/1225;
 
      /* if the player doesnt send info then we cant do remaining time */
      if ((configuration.timeMode == ELAPSED) || (curSong->info == NULL)) {
         minutes = seconds / 60;
         seconds = seconds % 60;
      } else {
         seconds = curSong->length - seconds;
 
         if (seconds < 0) seconds = 0;

         minutes = -(seconds / 60);
         seconds = seconds % 60;
      }
      progwin->min = minutes;
      progwin->sec = seconds;
      updateProgWin();
   }
}

/*
 * updates the position in the song as a percentage (ie: 0 <= pos <= 1)
 */
void updateSongPosition(double pos) {

   if ((progwin != NULL) && (progwin->win != NULL)) {
     progwin->ipos = (progwin->win->_maxx - 1)*pos;
     updateProgWin();
   }
}

/*
 * opens a given song and returns the file descriptor
 */
int songOpen(ITEM *item) {

   int fd = 0;

   if (item != NULL)
      fd = open(item->path, O_RDONLY);

   return fd;

}

/*
 * calculates song size
 */
int songSize(ITEM *item) {
   struct stat dstat;
   int ret = 0;

   if(stat(item->path, &dstat) == 0)
      ret = dstat.st_size;
   else
      ret = 0;

   return ret;
}

/*
 * resets an AudioInfo structure
 */
void infoReset(AudioInfo *info) {
   info->bitrate = 0;
   info->frequency = 0;
   info->stereo = 0;
   info->type = 0;
   info->sample = 0;
   info->mode = 0;
   info->crc = 0;
   info->copyright = 0;
   info->original = 0;
   info->ident = NULL;
}

/*
 * displays the filename for the indicated song
 */
void updatePlaying(ITEM *sng) {
   char *term_type = NULL;

   term_type = getenv("TERM");
   if ((term_type != NULL) && ((strcmp(term_type, "xterm") == 0) ||
                               (strcmp(term_type, "rxvt") == 0) ||
                               (strcmp(term_type, "aterm") == 0) ||
                               (strcmp(term_type, "eterm") == 0)))
      fprintf(stderr, "\033]0;%s\007", curSong->name);

   debug("updatePlaying: sng->name=%s\n", sng->name);
   if (progwin != NULL) {
      if (progwin->name != NULL) free(progwin->name);
      progwin->name = strpad(sng, progwin->win->_maxx - 11);
      updateProgWin();
   }
}

/*
 * toggles the display of the help window
 */
void toggleHelpWin() {

   if (!helpwin->active) { /* no help showing currently, so display it */

      helpwin->active = TRUE;
      updateHelpWin();

   } else { /* help being displayed, so destroy it */

      helpwin->active = FALSE;

      if (func == PLAYER) {
         refresh();
         touchwin(infowin->win);
         touchwin(progwin->win);
         touchwin(volwin->win);
         touchwin(listwin->win);
         wnoutrefresh(infowin->win);
         wnoutrefresh(progwin->win);
         wnoutrefresh(listwin->win);
         wnoutrefresh(volwin->win);
      } else { /* PLAYLIST */
         refresh();
         touchwin(dirwin->win);
         touchwin(listwin->win);
         touchwin(progwin->win);
         wnoutrefresh(dirwin->win);
         wnoutrefresh(listwin->win);
         wnoutrefresh(progwin->win);
      }
      doupdate();

   }
}

/*
 * updates the display of the help window
 */
void updateHelpWin() {
   char buf[MAX_STRLEN];
   int i = 0, j = 0;

   wmove(helpwin->win, 0, 0);
   wclrtobot(helpwin->win);
   box(helpwin->win, 0, 0);

   if (! configuration.expert) {
      mvwaddstr(helpwin->win, 0, 2, helpwin->wname);
   }

   if (func == PLAYER) {
      i = 6;
      j = 5;
      strcpy(buf, "p/space: play                   s: stop");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "f: start/end ffwd               r: start/end rew");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "n/right: next                   v/left: prev");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "a: pause                        q: quit");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "l: goto playlist                h: toggle help");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "t: toggle time mode             R: change repeat mode");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "+: increase volume              -: decrease volume");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;

   } else { /* PLAYLIST */
      i = 1;
      j = 5;
      strcpy(buf, "a: add all files                A: add recursively");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "left: remove/cd ..              right: add/chdir/play");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "down: move down                 up: move up");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "space: page down                -: page up");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "u: move song up                 d: move song down");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "t: move to top of list          b: move to bottom of list");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "T: move marked to top           B: move marked to bottom");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "z: add marked                   Z: recursively add marked");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "D: delete marked                e: replace current item");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "i: insert after current         I: insert marked after current");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "M: mark all                     U: unmark all");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "r: randomize list               o: sort list");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "c: clear playlist               C: crop playlist to marked");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "v: reverse list                 V: invert marked");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "l: load playlist                s: save playlist");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "q: quit                         p: goto player");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "h: toggle this help             tab: switch windows");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;
      strcpy(buf, "m: toggle mark                  P: start playing");
      mvwaddstr(helpwin->win, i, j, buf);
      i++;

   }
   wnoutrefresh(helpwin->win);
   doupdate();
}

/*
 * updates the contents of the info window. used by toggleInfoWin and by
 * updateupdateAudioInfo.
 */
void updateInfoWin() {
   char buf[MAX_STRLEN];
   int min = 0, sec = 0;

   /* info showing currently, so update it */
   if ((infowin->win != NULL) && (func == PLAYER)) {

      wmove(infowin->win, 0, 0);
      wclrtobot(infowin->win);
      box(infowin->win, 0, 0);

      if (! configuration.expert) {
         mvwaddstr(infowin->win, 0, 2, infowin->wname);
      }

      strcpy(buf, "length:                size:");
      mvwaddstr(infowin->win, 1, 3, buf);

      strcpy(buf, "bitrate:               frequency:");
      mvwaddstr(infowin->win, 2, 3, buf);
      strcpy(buf, "stereo:                type:");
      mvwaddstr(infowin->win, 3, 3, buf);

      strcpy(buf, "song: ");
      mvwaddstr(infowin->win, 4, 3, buf);
      strcpy(buf, "artist: ");
      mvwaddstr(infowin->win, 5, 3, buf);
      strcpy(buf, "album: ");
      mvwaddstr(infowin->win, 6, 3, buf);

      strcpy(buf, "year:                  genre:");
      mvwaddstr(infowin->win, 7, 3, buf);
      strcpy(buf, "comment: ");
      mvwaddstr(infowin->win, 8, 3, buf);
      strcpy(buf, "track: ");
      mvwaddstr(infowin->win, 9, 3, buf);

      if ((curSong != NULL) && (curState & playState)) {

         if (curSong->info != NULL) {
            sprintf(buf, "%d", curSong->info->bitrate);
            mvwaddstr(infowin->win, 2, 12, buf);

            sprintf(buf, "%d", curSong->info->frequency);
            mvwaddstr(infowin->win, 2, 37, buf);

            if (curSong->info->stereo == 2) strcpy(buf, "yes");
            else strcpy(buf, "no ");
            mvwaddstr(infowin->win, 3, 11, buf);

            if (curSong->info->type == 1) strcpy(buf, "Layer 1");
            else if (curSong->info->type == 2) strcpy(buf, "Layer 2");
            else if (curSong->info->type == 3) strcpy(buf, "Layer 3");
            else if (curSong->info->type == 4) strcpy(buf, "Wav");
            else strcpy(buf, "?");
            mvwaddstr(infowin->win, 3, 32, buf);
         }

         if (curSong->id3 != NULL) {
            strcpy(buf, curSong->id3->songname);
            mvwaddstr(infowin->win, 4, 9, buf);

            strcpy(buf, curSong->id3->artist);
            mvwaddstr(infowin->win, 5, 11, buf);

            strcpy(buf, curSong->id3->album);
            mvwaddstr(infowin->win, 6, 10, buf);

            strcpy(buf, curSong->id3->year);
            mvwaddstr(infowin->win, 7, 9, buf);

            if (curSong->id3->genre < genre_count) {
               strcpy(buf, genre_table[curSong->id3->genre]);
               mvwaddstr(infowin->win, 7, 33, buf);
            }

            strcpy(buf, curSong->id3->comment);
            mvwaddstr(infowin->win, 8, 12, buf);

            if (curSong->id3->track != 0) {
               sprintf(buf, "%d", curSong->id3->track);
               mvwaddstr(infowin->win, 9, 10, buf);
            }

         }

         sec = curSong->length - sec;
         
         if (sec < 0) sec = 0;
      
         min = sec / 60;
         sec = sec % 60;

         sprintf(buf, "%d:%02d", min, sec);
         mvwaddstr(infowin->win, 1, 11, buf);

         sprintf(buf, "%d bytes", curSong->size);
         mvwaddstr(infowin->win, 1, 32, buf);

      }
      wnoutrefresh(infowin->win);
      doupdate();
   }
}

/*
 * returns a pointer to the next song or NULL if there isnt another. takes
 * into account the current repeat mode.
 */
ITEM *getNextSong() {

   ITEM *next = NULL;

   if (curSong != NULL) {
      if (configuration.repeatMode == repeatNone) {
         next = curSong->next;
      } else if (configuration.repeatMode == repeatOne) {
         next = curSong;
      } else if (configuration.repeatMode == repeatAll) {
         if (curSong->next == NULL)
            next = listwin->list->head;
         else
            next = curSong->next;
      }
   }

   return(next);

}

/*
 * returns a pointer to the prev song or NULL if there isnt one. takes
 * into account the current repeat mode.
 */
ITEM *getPrevSong() {

   ITEM *prev = NULL;

   if (curSong != NULL) {
      if (configuration.repeatMode == repeatNone) {
         prev = curSong->prev;
      } else if (configuration.repeatMode == repeatOne) {
         prev = curSong;
      } else if (configuration.repeatMode == repeatAll) {
         if (curSong->prev == NULL)
            prev = listwin->list->tail;
         else
            prev = curSong->prev;
      }
   }
   return(prev);

}

/*
 * initializes thee users gamp configuration. will create the ~/.gamp/
 * directory and write the default configuration there. can also
 * be used to revert to defaults if you break something in the config
 * file.
 */
void newSetup(CONFIGURATION *config) {

   char *env_ptr = NULL;
   char *path = NULL;
   int ans = 0;
   struct stat dstat;

   env_ptr = getenv("HOME");
   if (env_ptr != NULL) {

      path = (char *)malloc(sizeof(char)*(strlen(env_ptr)+7));
      if (path == NULL) die("newSetup: malloc failure\n");
      strcpy(path, env_ptr);
      strcat(path, "/.gamp");

      if (stat(path, &dstat) == 0) {
         printf("your ~/.gamp/ directory already exists.\n");
         printf("should i reset your preferences ([y]/n)? ");
         ans = getchar();
         if ((ans == 'y') || (ans == '\n')) {
            config->configFile = (char *)malloc(sizeof(char) *
                                                (strlen(path) + 8));
            if (config->configFile == NULL)
               die("newSetup: malloc failure\n");
            strcpy(config->configFile, path);
            strcat(config->configFile, "/gamprc");

            writePrefs(NULL, config);
         }
      } else {
         printf("creating your ~/.gamp/ directory.\n");
         if (mkdir(path, 448) == 0) { /* 448 dec = 700 oct = drwx------ */
            printf("directory created. writing preferences.\n");

            config->configFile = (char *)malloc(sizeof(char) *
                                                (strlen(path) + 8));
            if (config->configFile == NULL)
               die("newSetup: malloc failure\n");
            strcpy(config->configFile, path);
            strcat(config->configFile, "/gamprc");

            writePrefs(NULL, config);
         } else {
            die("newSetup: error creating directory \"%s\"\n", path);
         }
      }
   } else {
      die("newSetup: HOME undefined? something's wrong...\n");
   }
}

/*
 * processes the command line arguments.
 */
void processArgs(int argc, char **argv, CONFIGURATION *config) {

   int i = 1;

   while (i < argc) {

      if ((strcmp(argv[i], "-c") == 0) ||
          (strcmp(argv[i], "--config") == 0)) {
         if (i + 1 < argc) {
            i++;
            config->configFile = strdup(argv[i]);
         } else {
            displayUsage();
            exit(-1);
         }

      } else if ((strcmp(argv[i], "-d") == 0) ||
          (strcmp(argv[i], "--dir") == 0)) {
         if (i + 1 < argc) {
            i++;
            sdir = strdup(argv[i]);
         } else {
            displayUsage();
            exit(-1);
         }

      } else if ((strcmp(argv[i], "-h") == 0) ||
                 (strcmp(argv[i], "--help") == 0)) {
         displayUsage();
         exit(0);

      } else if ((strcmp(argv[i], "-x") == 0) ||
                 (strcmp(argv[i], "--expert") == 0)) {
         config->expert = TRUE;

      } else if ((strcmp(argv[i], "-n") == 0) ||
                 (strcmp(argv[i], "--novice") == 0)) {
         config->expert = FALSE;

      } else if ((strcmp(argv[i], "-e") == 0) ||
                 (strcmp(argv[i], "--edit") == 0)) {
         func = PLAYLIST;

      } else if ((strcmp(argv[i], "-p") == 0) ||
                 (strcmp(argv[i], "--play") == 0)) {
         func = GOPLAY;

      } else if ((strcmp(argv[i], "-v") == 0) ||
                 (strcmp(argv[i], "--version") == 0)) {
         displayVersion();
         exit(0);

      } else if ((strcmp(argv[i], "-s") == 0) ||
                 (strcmp(argv[i], "--setup") == 0)) {
         newSetup(config);

      } else if (ism3u(argv[i])) {
         readM3u(argv[i]);

      } else if (ismp3(argv[i])) {
         addMp3(argv[i]);

      } else {
         debug("ignoring unknown command line option \"%s\"\n", argv[i]);
      }
      i++;
   }

}

/*
 * displays program usage
 */
void displayUsage() {

   displayVersion();
   printf("usage: gamp [option(s)] [mp3/m3u file(s)]\n");
   printf("options: -c, --config filename  load/save config file\n");
   printf("         -d, --dir dirname      start in dirname\n");
   printf("         -h, --help             display this help info\n");
   printf("         -v, --version          display version\n");
   printf("         -p, --play             start playing on startup\n");
   printf("         -s, --setup            sets up ~/.gamp/\n");
   printf("         -x, --expert           starts in expert mode\n");
   printf("         -n, --novice           starts in novice mode\n");
   printf("         -e, --edit             starts in editor\n");
   printf("see manpage gamp(1) for further information.\n");

}

/*
 * displays program version
 */
void displayVersion() {

   printf("gamp version %d.%d.%d\n", MAJOR, MINOR, PATCH);

}

void gampInit() {

   progwin = (PROGWIN *)malloc(sizeof(PROGWIN));
   if (progwin == NULL) die("gampInit: malloc error\n");
   progwin->height = 4;
   progwin->width = 0;
   progwin->ypos = 0;
   progwin->xpos = 0;
   progwin->wname = NULL;
//   progwin->wname = strdup("progwin");
   progwin->min = 0;
   progwin->sec = 0;
   progwin->ipos = 0;
   progwin->name = NULL;
   progwin->win = newwin(progwin->height, progwin->width, progwin->ypos, progwin->xpos);
   if (progwin->win == NULL) die("gampInit: newwin failure\n");
   box(progwin->win, 0, 0);

   volwin = (VOLWIN *)malloc(sizeof(VOLWIN));
   if (volwin == NULL) die("gampInit: malloc error\n");
   volwin->height = 11;
   volwin->width = 4;
   volwin->xpos = 0;
   volwin->ypos = progwin->height;
   volwin->wname = NULL;
   volwin->vol = -1;
   volwin->max = 100;
   volwin->incr = 5;
   initVolume(volwin);
   volwin->win = newwin(volwin->height, volwin->width, volwin->ypos, volwin->xpos);
   if (volwin->win == NULL) die("gampInit: newwin failure\n");
   box(volwin->win, 0, 0);

   infowin = (INFOWIN *)malloc(sizeof(INFOWIN));
   if (infowin == NULL) die("gampInit: malloc error\n");
   infowin->height = volwin->height;
   infowin->width = COLS - volwin->width;
   infowin->xpos = volwin->width;
   infowin->ypos = progwin->height;
   infowin->wname = NULL;
//   infowin->wname = strdup("infowin");
   infowin->win = newwin(infowin->height, infowin->width, infowin->ypos, infowin->xpos);
   if (infowin->win == NULL) die("gampInit: newwin failure\n");
   box(infowin->win, 0, 0);

   dirwin = (LISTWIN *)malloc(sizeof(LISTWIN));
   if (dirwin == NULL) die("gampInit: malloc error\n");
   dirwin->active = TRUE;
   dirwin->height = volwin->height;
   dirwin->width = 0;
   dirwin->ypos = progwin->height;
   dirwin->xpos = 0;
   dirwin->wname = NULL;
//   dirwin->wname = strdup("dirwin");
   dirwin->first = NULL;
   dirwin->cur = NULL;
   dirwin->list = NULL;
   dirwin->win = newwin(dirwin->height, dirwin->width, dirwin->ypos, dirwin->xpos);
   if (dirwin->win == NULL) die("gampInit: newwin failure\n");
   box(dirwin->win, 0, 0);

   listwin = (LISTWIN *)malloc(sizeof(LISTWIN));
   if (listwin == NULL) die("gampInit: malloc error\n");
   listwin->active = FALSE;
   listwin->height = LINES - dirwin->height - progwin->height;
   listwin->width = 0;
   listwin->ypos = dirwin->height + progwin->height; 
   listwin->xpos = 0;
   listwin->wname = NULL;
//   listwin->wname = strdup("listwin");
   listwin->first = NULL;
   listwin->cur = NULL;
   listwin->list = NULL;
   listwin->win = newwin(listwin->height, listwin->width, listwin->ypos, listwin->xpos);
   if (listwin->win == NULL) die("gampInit: newwin failure\n");
   box(listwin->win, 0, 0);

   helpwin = (HELPWIN *)malloc(sizeof(HELPWIN));
   if (helpwin == NULL) die("gampInit: malloc error\n");
   helpwin->height = 18;
   helpwin->width = 70;
   helpwin->active = FALSE;
   helpwin->ypos = (LINES - helpwin->height) / 2;
   if (helpwin->ypos < 0) helpwin->ypos = 0;
   helpwin->xpos = (COLS - helpwin->width) / 2;
   if (helpwin->xpos < 0) helpwin->xpos = 0;
   helpwin->win = newwin(helpwin->height, helpwin->width, helpwin->ypos, helpwin->xpos);
   if (helpwin->win == NULL) die("gampInit: newwin failure\n");
   box(helpwin->win, 0, 0);
   helpwin->wname = NULL;
//   helpwin->wname = strdup("helpwin");

}

void gampFinish() {

   if (progwin->win != NULL) {
      delwin(progwin->win);
      progwin->win = NULL;
   }  
   if (dirwin->win != NULL) {
      delwin(dirwin->win);
      dirwin->win = NULL;
   }
   if (listwin->win != NULL) {
      delwin(listwin->win);
      listwin->win = NULL;
   }  
   if (volwin->win != NULL) {
      delwin(volwin->win);
      volwin->win = NULL;
   }
   if (infowin->win != NULL) {
      delwin(infowin->win);
      infowin->win = NULL;
   }

}

/*
 * this is the  main function. it does some initial setup, reads
 * configuration, forks off the backend player and the message
 * watching threat, then switches between player and playlist until the
 * user quits, at which time it calls cleanup() and exits.
 */
int main(int argc, char **argv) {

   pthread_t msg_thread; /* the msg watcher thread */

   func = NONE;

   initPrefs(&configuration); /* initialize preferences */
   processArgs(argc, argv, &configuration); /* process the command line */
   readPrefs(NULL, &configuration); /* read prefs */
   dumpPrefs(&configuration); /* dump prefs (for debugging) */

   /*
    * if we got the "-p" command line arg we need to override
    * configuration.startWith with func, which would default to PLAYLIST
    * but get changed to GOPLAY if "-p" or "--play" were given on the
    * command line.
    */
   if (func == NONE)
      func = configuration.startWith;

   if (sdir != NULL)
      chdir(sdir);

   curState = stopState;
   lastSong = NULL;
   curSong = NULL;

   initialize(); /* initialize everything else */
   forkIt(); /* fork the player */

   initscr(); /* ncurses init stuff */
//   savetty();
   curs_set(0); /* invisible cursor */
   cbreak();
   noecho();
   nonl();
   intrflush(stdscr, FALSE);
   keypad(stdscr, TRUE);

   signal(SIGINT, cleanup); /* if we get interrupted we want to cleanup */

   /* create the thread that watches (and processes) msgs from player */
   pthread_create(&msg_thread, NULL, check_msg, NULL);
   pthread_detach(msg_thread);

   gampInit();

   /* keep going until we quit */
   while(func != QUIT) {

      if ((func == PLAYER) || (func == GOPLAY))
         func = playPlaylist();

      else if (func == PLAYLIST)
         func = editPlaylist();

   }

   gampFinish();

   /* if the config changed, the write out new config */
   if (configuration.dirty) writePrefs(NULL, &configuration);

   /* cleanup... */
   cleanup();

   /* exit */
   return(0);

}

/*
 * does cleanup, ends ncurses and kills the player
 */
void cleanup() {
   curs_set(1); /* set cursor visible */
   signal(SIGINT, SIG_DFL);
   resetty();
   endwin();
   playerKill();
}

/*
 * checks if the specified file exists. returns TRUE if it does and FALSE
 * otherwise.
 */
int exists(char *filename) {

   struct stat dstat;
   int ret = FALSE;  

   if (filename != NULL) {
      if (stat(filename, &dstat) == 0)
         ret = TRUE;
      else
         ret = FALSE;
   }

   return(ret);

}
