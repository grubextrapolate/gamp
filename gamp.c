/*
 * gamp.c v1.0.2
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
WINDOW *dirwin = NULL; /* direcory list window (in editor) */
WINDOW *listwin = NULL; /* playlist window (in editor) */

WINDOW *titlewin = NULL; /* song time/title window (in player) */
WINDOW *mainwin = NULL; /* main window (in player) */
WINDOW *progwin = NULL; /* progress window (in player) */

WINDOW *infowin = NULL; /* popup info window */
WINDOW *helpwin = NULL; /* popup help window */
WINDOW *miniwin = NULL; /* popup mini-list window in player*/

/* playlist and current directory list */
ITEMLIST *playlist = NULL;
ITEMLIST *dirlist = NULL;

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

/* where the program currently is. one of playlist, player, quit */
int func = PLAYLIST;

/* directory to start in (NULL unless set on command line with '-d') */
char *sdir = NULL;

/* player configuration */
CONFIGURATION configuration;

/*
 * initialize the given list with the contents of the specified directory.
 * this is called upon entering a new directory and upon entering the
 * playlist browser.
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
 * cleanup when exiting player. destroys windows and sets the pointers
 * to NULL.
 */
void finishPlayer() {

   if (titlewin != NULL) {
      delwin(titlewin);
      titlewin = NULL;
   }
   if (mainwin != NULL) {
      delwin(mainwin);
      mainwin = NULL;
   }
   if (progwin != NULL) {
      delwin(progwin);
      progwin = NULL;
   }
}

/*
 * initialization routine for the player. sets up the windows, displays
 * information if the track is already playing.
 */
void initPlayer() {

   /* create windows */
   titlewin = newwin(3, 0, 0, 0);
   if (titlewin == NULL) die("initPlayer: newwin failure\n");
   mainwin = newwin(LINES - 6, 0, 3, 0);
   if (mainwin == NULL) die("initPlayer: newwin failure\n");
   progwin = newwin(3, 0, LINES - 3, 0);
   if (progwin == NULL) die("initPlayer: newwin failure\n");

   /* draw boxes in windows and fill them with something */
   box(titlewin, 0, 0);
   box(mainwin, 0, 0);
   box(progwin, 0, 0);

   if (curState & playState)
      showFilename(titlewin, curSong);

   if (configuration.repeatMode == repeatOne)
      mvwaddstr(titlewin, 1, 1, "r");
   else if (configuration.repeatMode == repeatAll)
      mvwaddstr(titlewin, 1, 1, "R");

   if (configuration.displaySpectrum == FALSE)
      showLogo(mainwin, LINES, COLS);

   /* refresh screen */
   refresh();
   wnoutrefresh(titlewin);
   wnoutrefresh(mainwin);
   wnoutrefresh(progwin);
   doupdate();

}

/*
 * the player function. this controls all the keyboard input while in
 * player mode. returns info about what part of the program to go to next.
 */
int playPlaylist() {

   int ch = ';';

   initPlayer();

   if (helpwin != NULL) {
      toggleHelpWin();
      toggleHelpWin();
   }
   if (infowin != NULL) {
      toggleInfoWin();
      toggleInfoWin();
   }
   if (miniwin != NULL) {
      toggleMiniWin();
      toggleMiniWin();
   }

   /* if we're in 'autoplay' mode via the '-p' command line switch */
   if (func == GOPLAY) {
      func = PLAYER;
      if (playlist != NULL) curSong = playlist->head;
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
                  if (playlist != NULL) curSong = playlist->head;
                  playerPlay(curSong);
               }
            }
            break;

         case 'h': /* show/remove help window */
            toggleHelpWin();
            break;

         case 'i': /* show/remove mp3 info */
            toggleInfoWin();
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

         case 'u': /* show/remove mini-list */
         case 'm':
            toggleMiniWin();
            break;

         case 'a': /* pause */
            playerPause();
            break;

         case '+': /* increase volume (disabled) */
            break;

         case '-': /* decrease volume (disabled) */
            break;

         case 'd': /* toggle visuals on/off (disabled) */
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
               mvwaddstr(titlewin, 1, 1, "r");
            } else if (configuration.repeatMode == repeatOne) {
               configuration.repeatMode = repeatAll;
               mvwaddstr(titlewin, 1, 1, "R");
            } else if (configuration.repeatMode == repeatAll) {
               configuration.repeatMode = repeatNone;
               mvwaddstr(titlewin, 1, 1, " ");
            }
            wnoutrefresh(titlewin);
            doupdate();
            if (configuration.dirty == 0) configuration.dirty = 1;
            break;

         case 's': /* stop */
            playerStop();
            updateStopped();
            break;

         case 'l': /* back to playlist */
         case '>':
         case '<':
            func = PLAYLIST;
            break;

         case 12: /* ctrl-l (refresh) */
            finishPlayer();
            initPlayer();
            if (helpwin != NULL) {
               toggleHelpWin();
               toggleHelpWin();
            }
            if (infowin != NULL) {
               toggleInfoWin();
               toggleInfoWin();
            }
            if (miniwin != NULL) {
               toggleMiniWin();
               toggleMiniWin();
            }
            break;

         default:
            break;

      }

   }

   finishPlayer();

   return(func);
}

/*
 * writes num entries from list begining with first into win. will show
 * sel item in reverse text.
 */
void fillwin(WINDOW *win, ITEM *first, ITEMLIST *list, int num, ITEM *sel) {

   ITEM *cur = NULL;
   int i = 0;
   char *tmpstr = NULL;

   wclear(win);
   box(win, 0, 0);
   cur = first;
   while ((i <= num) && (cur != NULL)) {

      if (cur == sel) wattrset(win, A_REVERSE);
      tmpstr = strpad(cur, win->_maxx - 1);
      mvwaddstr(win, i+1, 1, tmpstr);
      free(tmpstr);
      if (cur == sel) wattrset(win, A_NORMAL);
      cur = cur->next;
      i++;
   }

   wnoutrefresh(win);
   doupdate();
}

/*
 * playlist editor cleanup routine. destroys windows and sets pointers to
 * NULL.
 */
void finishEditor() {

   if (dirwin != NULL) {
      delwin(dirwin);
      dirwin = NULL;
   }
   if (listwin != NULL) {
      delwin(listwin);
      listwin = NULL;
   }
}

/*
 * initialization routine for the editor. sets up the windows, displays
 * information if the track is already playing, shows the help menu at the
 * bottom.
 */
void initEditor() {

   int dirwin_height, listwin_height;  /* height of our windows */

   dirwin_height = LINES/2;  /* window sizes */
   listwin_height = LINES - dirwin_height;

   /* create windows */
   dirwin = newwin(dirwin_height, 0, 0, 0);
   if (dirwin == NULL) die("initEditor: newwin failure\n");
   listwin = newwin(listwin_height, 0, dirwin_height, 0);
   if (listwin == NULL) die("initEditor: newwin failure\n");

   /* draw boxes in windows and fill them with something */
   box(dirwin, 0, 0);
   box(listwin, 0, 0);

   /* refresh screen */
   refresh();
   wnoutrefresh(dirwin);
   wnoutrefresh(listwin);
   doupdate();

}

/*
 * the playlist editor. sets up the windows and allows you to create/alter
 * your playlist.
 */
int editPlaylist() {

   int win = 0;     /* window number. 0=dirwin, 1=listwin */
   int dw_pos = 0;  /* cursor position in dirwin */
   int lw_pos = 0;  /* cursor position in listwin */
   int ch = ';';
   int *pos = &dw_pos;
   char cwd[MAX_STRLEN];

   ITEM *dw_first = NULL; /* first item showing in window */
   ITEM *lw_first = NULL;
   ITEM *dw_cur = NULL;   /* currently selected item in window */
   ITEM *lw_cur = NULL;

   int i = 0;        /* loop counter */
   ITEM *itm = NULL; /* loop pointer */
   char *filename = NULL; /* load/save filename */
   char *cptr = NULL;

   initEditor();

   initDirlist("", &dirlist);
   sortList(dirlist);

   if (playlist != NULL) {
      if (curState & playState) {
         lw_first = curSong;
         lw_cur = curSong;
      } else {
         lw_first = playlist->head;
         lw_cur = playlist->head;
      }
   }
   if (dirlist != NULL) {
      if (treelist != NULL) {
         dw_cur = seekItemByPath(treelist->tail, dirlist);
         if (dw_cur != NULL) {
            dw_first = seekItemByPath(treelist->tail->prev, dirlist);
            dw_pos = treelist->tail->prev->length;
         } else {
            dw_first = dirlist->head;
            dw_cur = dirlist->head;
         }
      } else {
         dw_first = dirlist->head;
         dw_cur = dirlist->head;
      }
   }

   /* draw boxes in windows and fill them with something */
   fillwin(dirwin, dw_first, dirlist, dirwin->_maxy - 2, dw_cur);
   fillwin(listwin, lw_first, playlist, listwin->_maxy - 2, NULL);

   if (helpwin != NULL) {
      toggleHelpWin();
      toggleHelpWin();
   }

   while (func == PLAYLIST) {

      move(0, 0);

      ch = getch();
      switch(ch) {

         case 'p': /* start playing! */
         case '>':
         case '<':
            func = PLAYER;
            break;

         case 'q': /* quit program */
            func = QUIT;
            break;

         case 's': /* save playlist */
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

         case 'l': /* load playlist */
            filename = getFilename("load filename (ESC aborts):");
            if (filename != NULL) {
               if (readM3u(filename) == 0) {

                  if ((lw_first == NULL) && (playlist != NULL))
                     lw_first = playlist->head;
                  if ((lw_cur == NULL) && (playlist != NULL))
                     lw_cur = playlist->head;

                  if (win == 0) {
                     fillwin(listwin, lw_first, playlist,
                             listwin->_maxy - 2, NULL);
                  } else {
                     fillwin(listwin, lw_first, playlist,
                             listwin->_maxy - 2, lw_cur);
                  }
               }
            }
            break;

         case 'h': /* toggle help window */
            toggleHelpWin();
            break;

         case 'c': /* clear the playlist */
            if (curState & playState) {
               playerStop();
            }

            lastSong = NULL;
            curSong = NULL;

            freeList(&playlist);
            playlist = NULL;

            lw_pos = 0;
            lw_cur = NULL;
            lw_first = NULL;
            wclear(listwin);
            box(listwin, 0, 0);
            break;

         case 'a': /* add all in current directory */
            if ((dirlist != NULL) && (win == 0)) {
               itm = dirlist->head;
               while (itm != NULL) {
                  if (itm->isfile) addItem(copyItem(itm), &playlist);
                  itm = itm->next;
               }
               if (playlist != NULL) {
                  if (lw_first == NULL) lw_first = playlist->head;
                  if (lw_cur == NULL) lw_cur = playlist->head;
               }
               fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                       NULL);

            }
            break;

         case 'A': /* add all recursively */
            if ((dirlist != NULL) && (win == 0)) {
               getcwd(cwd, sizeof(cwd));
               addRecursive(dirlist, &playlist);
               if (playlist != NULL) {
                  if (lw_first == NULL) lw_first = playlist->head;
                  if (lw_cur == NULL) lw_cur = playlist->head;
               }
               chdir(cwd);
               fillwin(listwin, lw_first, playlist, listwin->_maxy - 2, 
                       NULL);
            }
            break;

         case ' ': /* scroll down by one page (if possible) */
            if (win == 0) {
               if ((dirlist != NULL) && (dw_cur != dirlist->tail)) {
                  i = 0;
                  itm = dw_first;
                  while ((i < dirwin->_maxy - 1) && (itm != NULL)) {
                     i++;
                     itm = itm->next;
                  }

                  if (itm != NULL) {
                     dw_first = itm;
                     dw_cur = itm;
                     dw_pos = 0;
                  } else {
                     dw_pos = i - 1;
                     dw_cur = dirlist->tail;
                  }

                  fillwin(dirwin, dw_first, dirlist, dirwin->_maxy - 2,
                          dw_cur);
               }
            } else {
               if ((playlist != NULL) && (lw_cur != playlist->tail)) {
                  i = 0;
                  itm = lw_first;
                  while ((i < listwin->_maxy - 1) && (itm != NULL)) {
                     i++;
                     itm = itm->next;
                  }

                  if (itm != NULL) {
                     lw_first = itm;
                     lw_cur = itm;
                     lw_pos = 0;
                  } else {
                     lw_pos = i - 1;
                     lw_cur = playlist->tail;
                  }

                  fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                          lw_cur);
               }
            }
            break;

         case '-': /* scroll up by one page (if possible) */
            if (win == 0) {
               if ((dirlist != NULL) && (dw_cur != dirlist->head)) {
                  i = 0;
                  itm = dw_first;
                  while ((i < dirwin->_maxy - 1) && (itm != NULL)) {
                     i++;
                     itm = itm->prev;
                  }

                  if (itm != NULL) {
                     dw_first = itm;
                     dw_cur = itm;
                     dw_pos = 0;
                  } else {
                     dw_pos = 0;
                     dw_first = dirlist->head;
                     dw_cur = dirlist->head;
                  }

                  fillwin(dirwin, dw_first, dirlist, dirwin->_maxy - 2,
                          dw_cur);
               }
            } else {
               if ((playlist != NULL) && (lw_cur != playlist->head)) {
                  i = 0;
                  itm = lw_first;
                  while ((i < listwin->_maxy - 1) && (itm != NULL)) {
                     i++;
                     itm = itm->prev;
                  }

                  if (itm != NULL) {
                     lw_first = itm;
                     lw_cur = itm;
                     lw_pos = 0;
                  } else {
                     lw_pos = 0;
                     lw_first = playlist->head;
                     lw_cur = playlist->head;
                  }

                  fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                          lw_cur);
               }
            }
            break;

         case KEY_UP: /* move up in window, scroll if needed */
            if (win == 0) {
               if ((dw_cur != NULL) && (dw_cur->prev != NULL)) {
                  if (dw_pos > 0) {
                     dw_pos--;
                     dw_cur = dw_cur->prev;
                  } else {
                     if (dw_first != NULL) dw_first = dw_first->prev;
                     dw_cur = dw_cur->prev;
                  }
                  fillwin(dirwin, dw_first, dirlist, dirwin->_maxy - 2,
                          dw_cur);
               }
            } else {
               if ((lw_cur != NULL) && (lw_cur->prev != NULL)) {
                  if (lw_pos > 0) {
                     lw_pos--;
                     lw_cur = lw_cur->prev;
                  } else {
                     if (lw_first != NULL) lw_first = lw_first->prev;
                     lw_cur = lw_cur->prev;
                  }
                  fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                          lw_cur);
               }
            }
            break;

         case KEY_DOWN: /* move down in window, scroll if needed */
            if (win == 0) {
               if ((dw_cur != NULL) && (dw_cur->next != NULL)) {
                  if (dw_pos < (dirwin->_maxy - 2)) { /* no scroll */
                     dw_pos++;
                     dw_cur = dw_cur->next;
                  } else { /* scroll */
                     if (dw_first != NULL) dw_first = dw_first->next;
                     dw_cur = dw_cur->next;
                  }
                  fillwin(dirwin, dw_first, dirlist, dirwin->_maxy - 2,
                          dw_cur);
               }
            } else {
               if ((lw_cur != NULL) && (lw_cur->next != NULL)) {
                  if (lw_pos < (listwin->_maxy - 2)) { /* no scroll */
                     lw_pos++;
                     lw_cur = lw_cur->next;
                  } else { /* scroll */
                     if (lw_first != NULL) lw_first = lw_first->next;
                     lw_cur = lw_cur->next;
                  }
                  fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                          lw_cur);
               }
            }
            break;

         case KEY_LEFT: /* remove item (only if in playlist win) */
            if (win == 0) {
               if ((treelist != NULL) && (treelist->tail != NULL)) {
                  strcpy(cwd, treelist->tail->path);
                  cptr = rindex(cwd, '/');
                  *(cptr+1) = '\0';
                  initDirlist(cwd, &dirlist);
               } else {
                  initDirlist("..", &dirlist);
               }
               sortList(dirlist);
               dw_pos = 0;
               if (dirlist != NULL) {
                  if (treelist != NULL) {
                     dw_cur = seekItemByPath(treelist->tail, dirlist);
                     if (dw_cur != NULL) {
                        popItem(&treelist);
                        dw_first = seekItemByPath(treelist->tail,
                                                  dirlist);
                        dw_pos = treelist->tail->length;
                        popItem(&treelist);
                     } else {
                        dw_first = dirlist->head;
                        dw_cur = dirlist->head;
                     }
                  } else {
                     dw_first = dirlist->head;
                     dw_cur = dirlist->head;
                  }
               }
               fillwin(dirwin, dw_first, dirlist, dirwin->_maxy - 2,
                       dw_cur);

            } else if ((win == 1) && (lw_cur != NULL)) {
               /* if we are currently playing then we need to adjust
                  the pointer to the current playing song */
               if (curSong == lw_cur) { 
                  if (lw_cur->next != NULL) playerNext();
                  else if (lw_cur->prev != NULL) playerPrev();
                  else { playerStop(); curSong = NULL; lastSong = NULL; }
               }
               itm = lw_cur;

               if (lw_cur->next != NULL) lw_cur = lw_cur->next;
               else if (lw_cur->prev != NULL) lw_cur = lw_cur->prev;
               else lw_cur = NULL;

               if (lw_pos == 0) {
                  if (lw_first->next != NULL) lw_first = lw_first->next;
                  else if (lw_first->prev != NULL) lw_first = lw_first->prev;
                  else lw_first = NULL;
               }

               if ((itm->next == NULL) && (lw_pos != 0)) lw_pos--;

               deleteItem(itm, &playlist);
               fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                       lw_cur);
            }
            break;

         case KEY_RIGHT: /* browse to new dir or add to playlist*/
            if (win == 0) {
               if (isfile(dw_cur)) {
                  addItem(copyItem(dw_cur), &playlist);
                  if (lw_first == NULL) lw_first = playlist->head;
                  if (lw_cur == NULL) lw_cur = playlist->head;
                  fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                          NULL);
               } else {
                  addItem(newItem(dw_first->path, NULL), &treelist);
                  treelist->tail->length = dw_pos;
                  addItem(newItem(dw_cur->path, NULL), &treelist);
                  initDirlist(dw_cur->name, &dirlist);
                  sortList(dirlist);
                  dw_pos = 0;
                  if (dirlist != NULL) {
                     dw_first = dirlist->head;
                     dw_cur = dirlist->head;
                  }
                  fillwin(dirwin, dw_first, dirlist, dirwin->_maxy - 2,
                          dw_cur);
               }
            } else {
               if (lw_cur != NULL)
                  playerPlay(lw_cur);
            }
            break;

         case '\t': /* use tab to move between windows */
            if (win == 1) {
               win = 0;
               pos = &dw_pos;
               fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                       NULL);
               fillwin(dirwin, dw_first, dirlist, dirwin->_maxy - 2,
                       dw_cur);
            } else {
               if (lw_first != NULL) {
                  win = 1;
                  pos = &lw_pos;
                  fillwin(dirwin, dw_first, dirlist, dirwin->_maxy - 2,
                          NULL);
                  fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                          lw_cur);
               }
            }
            break;

         case 'r': /* r to randomize the playlist */
            playlist = randomizeList(&playlist);
            if (playlist != NULL) {
               lw_first = playlist->head;
               lw_cur = playlist->head;
            }
            lw_pos = 0;
            if (win == 1)
               fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                       lw_cur);
            else
               fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                       NULL);
            break;

         case 'o': /* sort (order) the playlist */
            sortList(playlist);
            if (playlist != NULL) {
               lw_first = playlist->head;
               lw_cur = playlist->head;
            }
            lw_pos = 0;
            if (win == 1)
               fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                       lw_cur);
            else
               fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                       NULL);
            break;

         case 'u': /* move song up in playlist */
            if (win ==1) {
               if ((playlist != NULL) && (lw_cur != NULL) &&
                   (lw_cur->prev != NULL)) {
                  swapItems(playlist, lw_cur->prev, lw_cur);

                  if (lw_pos > 0) lw_pos--;
                  if (lw_pos == 0) lw_first = lw_cur;

                  fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                          lw_cur);
               }
            }
            break;

         case 'd': /* move song down in playlist */
            if (win ==1) {
               if ((playlist != NULL) && (lw_cur != NULL) &&
                   (lw_cur->next != NULL)) {
                  swapItems(playlist, lw_cur, lw_cur->next);

                  if (lw_pos == 0) lw_first = lw_cur->prev;
                  if (lw_pos < (listwin->_maxy - 2)) lw_pos++;
                  else lw_first = lw_first->next;

                  fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                          lw_cur);
               }
            }
            break;

         case 12: /* ctrl-l (refresh) */
            finishEditor();
            initEditor();
            if (win == 0) {
               fillwin(dirwin, dw_first, dirlist, dirwin->_maxy - 2,
                       dw_cur);
               fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                       NULL);
            } else {
               fillwin(listwin, lw_first, playlist, listwin->_maxy - 2,
                       lw_first);
               fillwin(dirwin, dw_first, dirlist, dirwin->_maxy - 2,
                       NULL);
            }
            if (helpwin != NULL) {
               toggleHelpWin();
               toggleHelpWin();
            }
            break;

         default:
            break;
      }
      wnoutrefresh(dirwin);
      wnoutrefresh(listwin);
      if (helpwin != NULL) {
         touchwin(helpwin);
         wnoutrefresh(helpwin);
      }
      doupdate();

   }

   finishEditor();

   return(func);

}

/*
 * adds the given mp3 filename onto the playlist (if file exists).
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
         addItem(item, &playlist);
         debug("addMp3: added %s\n", item->name);
      } else {
         free(ptr);
         free(item);
      }
   }

   return(ret);
}

/*
 * reads in a .m3u playlist file and adds entries into the internal
 * playlist.
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
      debug("readM3u: cant open playlist \"%s\"\n", filename);
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

            item = (ITEM *)malloc(sizeof(ITEM));
            if (item == NULL) die("readM3u: malloc failure\n");

            item->path = ptr;
            item->name = rindex(ptr, '/');
            item->info = NULL;
            item->next = NULL;
            item->prev = NULL;
            item->isfile = TRUE;

            if (item->name != NULL) item->name++;
            else die("readM3u: invalid filename \"%s\"\n", ptr);

            if(stat(item->path, &dstat) == 0) {
               getID3(item);
               addItem(item, &playlist);
               debug("readM3u: added %s\n", item->name);
            } else {
               free(ptr);
               free(item);
            }
         }
      }

      fclose(infile);
      ret = 0;
   }

   return(ret);

}

/*
 * writes a .m3u playlist file from the internal playlist.
 */
int writeM3u(char *filename) {

   int ret = 0;
   FILE *outfile = NULL;
   ITEM *item = NULL;

   if(!(outfile = fopen(filename, "w"))) {
      debug("writeM3u: cant open playlist \"%s\" for writing\n", filename);
      ret = -1;
   } else {

      if (playlist != NULL) {
         item = playlist->head;

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
      updateMiniWin(curSong);
   }
}

/*
 * updates the song time display
 */
void updateSongTime(int cur_frame) {
   char tmpstr[20];
   int minutes = 0;
   int seconds = 0;

   if ((titlewin != NULL) && (curSong != NULL)) {
      seconds = (cur_frame*32)/1225;
 
      /* if the player doesnt send info then we cant do remaining time */
      if ((configuration.timeMode == ELAPSED) || (curSong->info == NULL)) {
         minutes = seconds / 60;
         seconds = seconds % 60;

         if (minutes > 99) {
            minutes = 99;
            seconds = 59;
         }

         sprintf(tmpstr, "[%3d:%02d] ", minutes, seconds);
      } else {
         seconds = curSong->length - seconds;
 
         if (seconds < 0) seconds = 0;

         minutes = -(seconds / 60);
         seconds = seconds % 60;

         if (abs(minutes) > 99) {
            minutes = -99;
            seconds = 59;
         }

         if (minutes == 0) {
            sprintf(tmpstr, "[ -0:%02d] ", seconds);
         } else {
            sprintf(tmpstr, "[%3d:%02d] ", minutes, seconds);
         }
      }
      mvwaddstr(titlewin, 1, 2, tmpstr);
      wnoutrefresh(titlewin);
      doupdate();
   }
}

/*
 * updates the position in the song as a percentage (ie: 0 <= pos <= 1)
 */
void updateSongPosition(double pos) {
   int i, j;
   char tmpstr[MAX_STRLEN];

   if (func == PLAYER) {
      j = (progwin->_maxx - 1)*pos;
      for (i = 0; i < (progwin->_maxx - 1); i++) {
         if (i < j)
            tmpstr[i] = '=';
         else if (i == j)
            tmpstr[i] = '|';
         else /* (i > j) */
            tmpstr[i] = '-';
      }
      tmpstr[progwin->_maxx - 1] = '\0';
      mvwaddstr(progwin, 1, 1, tmpstr);
      wnoutrefresh(progwin);
      doupdate();
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
   if ((term_type != NULL) && (strcmp(term_type, "xterm") == 0))
      fprintf(stderr, "\033]0;%s\007", curSong->name);

   debug("updatePlaying:sng->name=%s\n", sng->name);
   showFilename(titlewin, sng);
}

/*
 * clears filename and progress windows
 */
void updateStopped() {
   debug("updateStopped: clearing progwin and titlewin\n");

   if (func == PLAYER) {
      wclear(titlewin);
      box(titlewin, 0, 0);
      wclear(progwin);
      box(progwin, 0, 0);
      wnoutrefresh(titlewin);
      wnoutrefresh(progwin);
      updateInfoWin();
      doupdate();
   }
}

/*
 * writes the filename in the title window
 */
void showFilename(WINDOW *win, ITEM *itm) {
   char *tmp = NULL;
   if ((win != NULL) && (itm != NULL)) {
      tmp = strpad(itm, win->_maxx - 11);
      mvwaddstr(win, 1, 11, tmp);
      wnoutrefresh(win);
      doupdate();
   }
}

/*
 * toggles the display of the help window
 */
void toggleHelpWin() {
   int x_offset = 0, y_offset = 0;
   int helpWidth = 50, helpHeight = 12;
   char buf[MAX_STRLEN];

   if (helpwin == NULL) { /* no help showing currently, so display it */

      x_offset = (COLS - helpWidth) / 2;
      y_offset = (LINES - helpHeight) / 2;

      if (x_offset < 0) x_offset = 0;
      if (y_offset < 0) y_offset = 0;

      helpwin = newwin(helpHeight, helpWidth, y_offset, x_offset);
      if (helpwin == NULL) die("toggleHelp: newwin failure\n");
      box(helpwin, 0, 0);

      if (func == PLAYER) {

         strcpy(buf, "p: play                 s: stop");
         mvwaddstr(helpwin, 2, 3, buf);
         strcpy(buf, "f: start/end ffwd       r: start/end rew");
         mvwaddstr(helpwin, 3, 3, buf);
         strcpy(buf, "n: next                 v: prev");
         mvwaddstr(helpwin, 4, 3, buf);
         strcpy(buf, "a: pause                q: quit");
         mvwaddstr(helpwin, 5, 3, buf);
         strcpy(buf, "l: goto playlist        h: toggle help");
         mvwaddstr(helpwin, 7, 3, buf);
         strcpy(buf, "m: toggle mini-list     t: toggle time");
         mvwaddstr(helpwin, 8, 3, buf);
         strcpy(buf, "i: toggle info          R: cycle repeat mode");
         mvwaddstr(helpwin, 9, 3, buf);

      } else { /* PLAYLIST */

         strcpy(buf, "a: add all files        A: add recursively");
         mvwaddstr(helpwin, 1, 3, buf);
         strcpy(buf, "left: remove/cd ..      right: add/chdir/play");
         mvwaddstr(helpwin, 2, 3, buf);
         strcpy(buf, "space: page down        -: page up");
         mvwaddstr(helpwin, 3, 3, buf);
         strcpy(buf, "down: move down         up: move up");
         mvwaddstr(helpwin, 4, 3, buf);
         strcpy(buf, "u: move song up         d: move song down");
         mvwaddstr(helpwin, 5, 3, buf);
         strcpy(buf, "r: randomize list       o: sort list");
         mvwaddstr(helpwin, 6, 3, buf);
         strcpy(buf, "l: load playlist        s: save playlist");
         mvwaddstr(helpwin, 7, 3, buf);
         strcpy(buf, "c: clear playlist       h: toggle this help");
         mvwaddstr(helpwin, 8, 3, buf);
         strcpy(buf, "q: quit                 p: goto player");
         mvwaddstr(helpwin, 9, 3, buf);
         strcpy(buf, "tab: switch windows");
         mvwaddstr(helpwin, 10, 3, buf);

      }
      wnoutrefresh(helpwin);

   } else { /* help being displayed, so destroy it */

      if (func == PLAYER) {
         delwin(helpwin);
         touchwin(mainwin);
         helpwin = NULL;
         refresh();
         wnoutrefresh(mainwin);
         if (infowin != NULL) {
            touchwin(infowin);
            wnoutrefresh(infowin);
         }
         if (miniwin != NULL) {
            touchwin(miniwin);
            wnoutrefresh(miniwin);
         }
      } else { /* PLAYLIST */
         delwin(helpwin);
         touchwin(dirwin);
         touchwin(listwin);
         helpwin = NULL;
         refresh();
         wnoutrefresh(dirwin);
         wnoutrefresh(listwin);
      }

   }

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
   if ((infowin != NULL) && (func == PLAYER)) {

      wclear(infowin);
      box(infowin, 0, 0);

      strcpy(buf, "length:                size:");
      mvwaddstr(infowin, 1, 3, buf);

      strcpy(buf, "bitrate:               frequency:");
      mvwaddstr(infowin, 2, 3, buf);
      strcpy(buf, "stereo:                type:");
      mvwaddstr(infowin, 3, 3, buf);

      strcpy(buf, "song: ");
      mvwaddstr(infowin, 5, 3, buf);
      strcpy(buf, "artist: ");
      mvwaddstr(infowin, 6, 3, buf);
      strcpy(buf, "album: ");
      mvwaddstr(infowin, 7, 3, buf);

      strcpy(buf, "year:                  genre:");
      mvwaddstr(infowin, 8, 3, buf);
      strcpy(buf, "comment: ");
      mvwaddstr(infowin, 9, 3, buf);
      strcpy(buf, "track: ");
      mvwaddstr(infowin, 10, 3, buf);

      if ((curSong != NULL) && (curState & playState)) {

         if (curSong->info != NULL) {
            sprintf(buf, "%d", curSong->info->bitrate);
            mvwaddstr(infowin, 2, 12, buf);

            sprintf(buf, "%d", curSong->info->frequency);
            mvwaddstr(infowin, 2, 37, buf);

            if (curSong->info->stereo == 2) strcpy(buf, "yes");
            else strcpy(buf, "no ");
            mvwaddstr(infowin, 3, 11, buf);

            if (curSong->info->type == 1) strcpy(buf, "Layer 1");
            else if (curSong->info->type == 2) strcpy(buf, "Layer 2");
            else if (curSong->info->type == 3) strcpy(buf, "Layer 3");
            else if (curSong->info->type == 4) strcpy(buf, "Wav");
            else strcpy(buf, "?");
            mvwaddstr(infowin, 3, 32, buf);
         }

         if (curSong->id3 != NULL) {
            strcpy(buf, curSong->id3->songname);
            mvwaddstr(infowin, 5, 9, buf);

            strcpy(buf, curSong->id3->artist);
            mvwaddstr(infowin, 6, 11, buf);

            strcpy(buf, curSong->id3->album);
            mvwaddstr(infowin, 7, 10, buf);

            strcpy(buf, curSong->id3->year);
            mvwaddstr(infowin, 8, 9, buf);

            if (curSong->id3->genre < genre_count) {
               strcpy(buf, genre_table[curSong->id3->genre]);
               mvwaddstr(infowin, 8, 33, buf);
            }

            strcpy(buf, curSong->id3->comment);
            mvwaddstr(infowin, 9, 12, buf);

            if (curSong->id3->track != 0) {
               sprintf(buf, "%d", curSong->id3->track);
               mvwaddstr(infowin, 10, 10, buf);
            }

         }

         sec = curSong->length - sec;
         
         if (sec < 0) sec = 0;
      
         min = sec / 60;
         sec = sec % 60;

         sprintf(buf, "%d:%02d", min, sec);
         mvwaddstr(infowin, 1, 11, buf);

         sprintf(buf, "%d bytes", curSong->size);
         mvwaddstr(infowin, 1, 32, buf);

      }
      wnoutrefresh(infowin);
      doupdate();
   }
}

/*
 * toggles the track info window open/closed.
 */
void toggleInfoWin() {
   int x_offset = 0, y_offset = 0;
   int infoWidth = 50, infoHeight = 12;

   if (func == PLAYER) {

      if (infowin == NULL) { /* no info showing currently, so display it */

         x_offset = (mainwin->_maxx - infoWidth) / 2;
         y_offset = (mainwin->_maxy - infoHeight) / 2;
         x_offset += mainwin->_begx + 1;
         y_offset += mainwin->_begy + 1;

         if (x_offset < 0) x_offset = 0;
         if (y_offset < 0) y_offset = 0;

         infowin = newwin(infoHeight, infoWidth, y_offset, x_offset);
         if (infowin == NULL) die("toggleInfoWin: newwin failure\n");

         updateInfoWin();

      } else { /* info being displayed, so destroy it */

         delwin(infowin);
         touchwin(mainwin);
         infowin = NULL;

         refresh();
         wnoutrefresh(mainwin);
         if (helpwin != NULL) {
            touchwin(helpwin);
            wnoutrefresh(helpwin);
         }
         if (miniwin != NULL) {
            touchwin(miniwin);
            wnoutrefresh(miniwin);
         }
      }

      doupdate();

   }
}

/*
 * updates the contents of the track mini-list window. used by
 * toggleMiniWin and playPlaylist();
 */
void updateMiniWin(ITEM *mw_first) {

   if (miniwin != NULL) { /* mini showing currently, so update it */

      if (func == PLAYER) {

         wclear(miniwin);
         box(miniwin, 0, 0);

         if (mw_first->prev != NULL) {
            if (mw_first->prev->prev != NULL) {
               fillwin(miniwin, mw_first->prev->prev, playlist,
                       miniwin->_maxy - 2, curSong);
            } else {
               fillwin(miniwin, mw_first->prev, playlist,
                       miniwin->_maxy - 2, curSong);
            }
         } else {
            fillwin(miniwin, mw_first, playlist, miniwin->_maxy - 2,
                    curSong);
         }
         wnoutrefresh(miniwin);
         doupdate();
      }
   }
}

/*
 * toggles the track mini-list window open/closed.
 */
void toggleMiniWin() {
   int x_offset = 0, y_offset = 0;
   int miniWidth = 60, miniHeight = 7;
   ITEM *ret = NULL;

   if (playlist != NULL) {
      if (func == PLAYER) {
         if (miniwin == NULL) { /* no mini-list showing currently, so display it */

            x_offset = (mainwin->_maxx - miniWidth) / 2;
            y_offset = (mainwin->_maxy - miniHeight) / 2;
            x_offset += mainwin->_begx + 1;
            y_offset += mainwin->_begy;

            if (x_offset < 0) x_offset = 0;
            if (y_offset < 0) y_offset = 0;

            miniwin = newwin(miniHeight, miniWidth, y_offset, x_offset);
            if (miniwin == NULL) die("toggleMiniWin: newwin failure\n");

            if (curSong != NULL)
               ret = curSong;
            else
               ret = playlist->head;

            updateMiniWin(ret);

         } else { /* miniwin being displayed, so destroy it */

            delwin(miniwin);
            touchwin(mainwin);
            miniwin = NULL;

            refresh();
            wnoutrefresh(mainwin);
            if (helpwin != NULL) {
               touchwin(helpwin);
               wnoutrefresh(helpwin);
            }
            if (infowin != NULL) {
               touchwin(infowin);
               wnoutrefresh(infowin);
            }
         }
      }
   }

   doupdate();
}

/*
 * displays the gamp logo in the center of the window! :)
 */
void showLogo(WINDOW *win, int height, int width) {
   int i = 0, x_offset = 0, y_offset = 0;

   if (height > 0) {
      x_offset = (width - configuration.logoWidth) / 2;
      y_offset = (height - configuration.logoHeight) / 2;

      x_offset -= win->_begx;
      y_offset -= win->_begy;

      if (x_offset < 0) x_offset = 0;
      if (y_offset < 0) y_offset = 0;

      for (i = 0; i < configuration.logoHeight; i++) {
         mvwaddstr(win, i+y_offset, x_offset, configuration.logo[i]);
      }
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
            next = playlist->head;
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
            prev = playlist->tail;
         else
            prev = curSong->prev;
      }
   }
   return(prev);

}

/*
 * initializes thee users gamp configuration. will create the ~/.gamp/
 * directory and write the default configuration and logo there. can also
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

            config->logoFile = (char *)malloc(sizeof(char) *
                                              (strlen(path) + 11));
            if (config->logoFile == NULL)
               die("newSetup: malloc failure\n");
            strcpy(config->logoFile, path);
            strcat(config->logoFile, "/gamp.logo");

            writePrefs(NULL, config);

            initLogo(config);
            writeLogo(NULL, config);
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

            config->logoFile = (char *)malloc(sizeof(char) *
                                              (strlen(path) + 11));
            if (config->logoFile == NULL)
               die("newSetup: malloc failure\n");
            strcpy(config->logoFile, path);
            strcat(config->logoFile, "/gamp.logo");

            writePrefs(NULL, config);

            initLogo(config);
            writeLogo(NULL, config);
         } else {
            die("newSetup: error creating directory \"%s\"\n", path);
         }
      }
   } else {
      die("newSetup: HOME undefined? somethings wrong...\n");
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

      } else if ((strcmp(argv[i], "-l") == 0) ||
          (strcmp(argv[i], "--logo") == 0)) {
         if (i + 1 < argc) {
            i++;
            config->logoFile = strdup(argv[i]);
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
   printf("         -l, --logo filename    load this ascii logo\n");
   printf("         -d, --dir dirname      start in dirname\n");
   printf("         -h, --help             display this help info\n");
   printf("         -v, --version          display version\n");
   printf("         -p, --play             start playing on startup\n");
   printf("         -s, --setup            sets up ~/.gamp/\n");
   printf("see manpage gamp(1) for further information.\n");

}

/*
 * displays program version
 */
void displayVersion() {

   printf("gamp version %d.%d.%d\n", MAJOR, MINOR, PATCH);

}

/*
 * this is the  main function. it does some initial setup, reads
 * configuration and logo, forks off the backend player and the message
 * watching threat, then switches between player and playlist until the
 * user quits, at which time it calls cleanup() and exits.
 */
int main(int argc, char **argv) {

   pthread_t msg_thread; /* the msg watcher thread */

   initPrefs(&configuration); /* initialize preferences */
   processArgs(argc, argv, &configuration); /* process the command line */
   readPrefs(NULL, &configuration); /* read prefs */
   readLogo(NULL, &configuration); /* read logo */
   dumpPrefs(&configuration); /* dump prefs (for debugging) */

   /*
    * if we got the "-p" command line arg we need to override
    * configuration.startWith with func, which would default to PLAYLIST
    * but get changed to GOPLAY if "-p" or "--play" were given on the
    * command line.
    */
   if (func != GOPLAY)
      func = configuration.startWith;

   if (sdir != NULL)
      chdir(sdir);

   curState = stopState;
   lastSong = NULL;
   curSong = NULL;

   initialize(); /* initialize everything else */
   forkIt(); /* fork the player */

   initscr(); /* ncurses init stuff */
   cbreak();
   noecho();
   nonl();
   intrflush(stdscr, FALSE);
   keypad(stdscr, TRUE);

   signal(SIGINT, cleanup); /* if we get interrupted we want to cleanup */

   /* create the thread that watches (and processes) msgs from player */
   pthread_create(&msg_thread, NULL, check_msg, NULL);
   pthread_detach(msg_thread);

   /* keep going until we quit */
   while(func != QUIT) {

      if ((func == PLAYER) || (func == GOPLAY))
         func = playPlaylist();

      else if (func == PLAYLIST)
         func = editPlaylist();

   }

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
   signal(SIGINT, SIG_DFL);
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

