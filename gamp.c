/* gamp.c v0.1.3
   by grub <grub@toast.net> and borys <borys@bill3.ncats.net>

   ncurses based command line interface to amp. has a directory browser
   and other fun stuff like those other x based clients, but for the
   hardcore command line enthusiast.

   WORKING:

   -playlist seems to work ok for adding and removing.
   -directory browsing works.
   -directory list is sorted.
   -file filtering for *.mp3 (case insensitive).
   -passing start directory on command line seems to be working.

   FIX ME:

   -playlist randomizer segfaults.

   TODO (the big list):

   -fix items in FIX ME section :)
   -make it play! only playlist presently functional.
   -prev/next/ffwd/rew/stop/pause for player
   -bottom section to indicate key functions.
   -documentation (in and out of source).
   -load/save playlists (possibly via -p <filename> switch)
   -text-based frequency analyzer?
   -redo structure for directory list to save memory
   -webpage?
   -any other cool items we can think of

   CHANGES:

   v0.1.0:
   -working on the player. big restructuring. incorperating parts of
    audio.c from amp to make it play now. almost there!
   -moved the functions associated with the STRLIST structure to gamp.h to
    make the main source more readable.

   v0.0.7:
   -minor cleanup. still confused whats up with the browser segfaulting
    after you add to the playlist.
   -some big changes to program control. made way to change between the
    player and the playlist editor.
   -started working on the player.

   v0.0.6:
   -further cleanup. fixed some memory leaks.

   v0.0.5:
   -bunch of restructuring to clean things up. added new add, delete,
    isdir, isfile, swap functions.
   -fixed filename filter.
   -command line directory seems to be working now.

*/

/* amp include stuff (from audio.c in amp) */
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

#include "gamp.h"
#include <curses.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>


int AUDIO_BUFFER_SIZE = 300*1024;
int A_SET_VOLUME = -1;
int A_AUDIO_PLAY = TRUE;

int CONT=0;
int QUIT=1;
int GOTO_EDITOR=2;
int GOTO_PLAYER=3;

int STOP=4;
int NEXT=5;
int PREV=6;
int FFWD=7;
int REW=8;
int PAUSE=9;
int PLAY=10;

int inline processHeader(struct AUDIO_HEADER *header, int cnt) {
    int g;

    /* getbits funcs expect first four bytes after BUFFER_SIZE have same contents
     * as the first four bytes of the buffer. fillbfr() normally takes care of
     * this when it hits the end of the buffer, but not this first time
     */
    if (cnt==1) memcpy(&buffer[BUFFER_SIZE],buffer,4);	/* XXX WHAT!!! */

    if ((g=gethdr(header))!=0) {
        if (g != GETHDR_ERR) {
            warn(" this is a file in MPEG 2.5 format, which is not defined\n");
            warn(" by ISO/MPEG. It is \"a special Fraunhofer format\".\n");
            warn(" amp does not support this format. sorry.\n");
        } else {
            /* here we should check whether it was an EOF or a sync err. later. */
            if (A_FORMAT_WAVE) wav_end(header);

            /* this is the message we'll print once we check for sync err.
             * warn(" amp encountered a synchronization error\n");
             * warn(" sorry.\n");
             */
        }
        return(1);
    }

    /* crc is read just to get out of the way.
     */
    if (header->protection_bit==0) getcrc();

    return(0);
}

void statusDisplay(struct AUDIO_HEADER *header, int frameNo) {
    int minutes,seconds;

    if ((A_SHOW_CNT || A_SHOW_TIME) && !(frameNo%10))
	msg("\r");
    if (A_SHOW_CNT && !(frameNo%10) ) {
	msg("	 {%d} ",frameNo);
    }
    if (A_SHOW_TIME && !(frameNo%10)) {
	seconds=frameNo*1152/t_sampling_frequency[header->ID][header->sampling_frequency];
	minutes=seconds/60;
	seconds=seconds % 60;
	msg("	 %2d:%02d ",minutes,seconds);
    }
    if (A_SHOW_CNT || A_SHOW_TIME)
	fflush(stderr);
}


int decodeMPEG(char *inFileStr, char *outFileStr) {
    struct AUDIO_HEADER header;
    int cnt,err=0;

    if ((in_file=fopen(inFileStr,"r"))==NULL) {
        die("Could not open file: %s\n");
        return(1);
    }

    /* initialize globals */
    append=data=nch=0;

    /* _the_ loop */
    for (cnt=0;;cnt++) {
        if (processHeader(&header,cnt))
            break;

/*        statusDisplay(&header,cnt); */

        /* setup the audio when we have the frame info */
        if (!cnt) {

            /* audioOpen(frequency, stereo, volume) */
            audioBufferOpen(t_sampling_frequency[header.ID][header.sampling_frequency],
                            (header.mode!=3),A_SET_VOLUME);
        }

        if (layer3_frame(&header,cnt)) {
            warn(" error. blip.\n");
            err=1;
            break;
        } 
    }
    fclose(in_file);

    audioBufferClose();

    msg("\n");
    return(err);
}


STRLIST dirlist  = { 0, 0, 0, 0 };
STRLIST playlist = { 0, 0, 0, 0 };
char cwd[256];
char tmpstr[150];

/*
void die(void) {
    move(LINES, 0);
    endwin();

    perror("");
}
*/

int ismp3(char *name) {

    int len = 0;
    len = strlen(name);

    if (len > 3) {
        if ((strcmp(name+len-4, ".mp3") == 0) ||
            (strcmp(name+len-4, ".Mp3") == 0) ||
            (strcmp(name+len-4, ".mP3") == 0) ||
            (strcmp(name+len-4, ".MP3") == 0))
            return 1;
    }
    return 0;
}

void init_playlist(void) {
    free_list(&playlist);
    init_list(&playlist, 20);
}

void init_dirlist(char *pwd) {
    DIR *dir;
    struct dirent *entry;
    struct stat dstat;
    char blank[5]="";

    if (pwd[0] == '/') {       /* if we have an absolute path */
        strcpy(cwd, pwd);
        chdir(cwd);
        getcwd(cwd, sizeof(cwd));
    }
    else {
        if (cwd[0] == '\0')
           getcwd(cwd, sizeof(cwd));

        strcat(cwd, "/");
        strcat(cwd, pwd);
        chdir(cwd);
        getcwd(cwd, sizeof(cwd));
    }

    free_list(&dirlist);
    init_list(&dirlist, 30);

    if((dir = opendir(cwd)) == NULL) die("cant open dir");

    while((entry = readdir(dir))) {

        if(strcmp(".", entry->d_name) == 0)  continue;
        if(stat(entry->d_name, &dstat) != 0) continue;

        if(S_ISDIR(dstat.st_mode))
            add(&dirlist, entry->d_name, blank);
        else if(S_ISREG(dstat.st_mode)) {
            if (ismp3(entry->d_name))
                add(&dirlist, blank, entry->d_name);
        } else continue;

    }

    sort_list(&dirlist);
}

void strtrunc(char *str, int length) {
    char format[6];
    int len = strlen(str);
    int i;
  
    sprintf(format, "%%-%ds", length);
    sprintf(tmpstr, format, str);

    /* convert long files like:
     * long-filename.ext -> long-fi...ext
     * subject to the length requirements as passed
     */
    if(len > length) {
        tmpstr[length] = 0;
        for(i = 0; i < 3; i++) tmpstr[--length] = tmpstr[--len];
        for(i = 0; i < 3; i++) tmpstr[--length] = '.';
    }
}

void fillwin(WINDOW *win, STRLIST *list, int first, int width, int height) {

    int i;
    char *dirfilecat;

    for(i = 0; i < list->cur && i < height - 2; i++) {
        /* if its a file, print only the file */
        if (isfile(list,i+first))
            strtrunc(list->files[i+first],width-2);

        /* if its a dir then print only the dir */
        else if (isdir(list,i+first))
            strtrunc(list->dirs[i+first],width-2);

        /* if it is neither then it is a dir+file combo so
           concatentate dir to file and print */
        else {
            dirfilecat = malloc(sizeof(char) *
                         (strlen(list->dirs[i+first]) +
                          strlen(list->files[i+first]) + 2));
            strcpy(dirfilecat, list->dirs[i+first]);
            strcat(dirfilecat, "/");
            strcat(dirfilecat, list->files[i+first]);
            strtrunc(dirfilecat, width-2);
            free(dirfilecat);
        }
        mvwaddstr(win, i+1, 1, tmpstr);
    }

}

void show_filename(WINDOW *win, STRLIST *list, int index) {
    char *dirfilecat;

    dirfilecat = strdup(list->dirs[index]);
    strcat(dirfilecat, "/");
    strcat(dirfilecat, list->files[index]);
    strtrunc(dirfilecat, COLS-2);

    mvwaddstr(win, 1, 1, tmpstr);
}

void clear_filename(WINDOW *win) {
    char dirfilecat[255] = "";

    strtrunc(dirfilecat, COLS-2);

    mvwaddstr(win, 1, 1, tmpstr);
}

int play_playlist(int argc, char *argv[]) {
    WINDOW *titlewin, *helpwin, *mainwin;  /* our window */
    int stop = CONT;   /* stopping condition for the program */
    int play = STOP;
    int current = 0;
    int ch;
    char filename[255];

    /* premultiply dewindowing tables.  Should go away */
    premultiply();

    /* init imdct tables */
    imdct_init();

    /* create windows */
    titlewin = newwin(3, 0, 0, 0);
    mainwin = newwin(LINES - 6, 0, 3, 0);
    helpwin = newwin(3, 0, LINES - 3, 0);

    /* draw boxes in windows and fill them with something */
    box(titlewin, 0, 0);
    box(mainwin, 0, 0);
    box(helpwin, 0, 0);

    strcpy(filename, "(p)lay   (s)top  p(l)aylist (q)uit");
    mvwaddstr(helpwin, LINES-1, 1, filename);

    /* refresh screen */
    refresh();
    wnoutrefresh(titlewin);
    wnoutrefresh(mainwin);
    wnoutrefresh(helpwin);
    doupdate();

    /* move cursor to its start position */
    move(1,1);

    while(stop == CONT) {

        if (play == PLAY && current < playlist.cur) {
            show_filename(titlewin, &playlist, current);
            refresh();
            wnoutrefresh(titlewin);
            wnoutrefresh(mainwin);
            wnoutrefresh(helpwin);
            doupdate();

            strcpy(filename, playlist.dirs[current]);
            strcat(filename, "/");
            strcat(filename, playlist.files[current]);
            decodeMPEG(filename,0);

            clear_filename(titlewin);
            refresh();
            wnoutrefresh(titlewin);
            wnoutrefresh(mainwin);
            wnoutrefresh(helpwin);
            doupdate();

            current++;
            if (current == playlist.cur) {
                play = STOP;
                current = 0;
            }
        }

        ch = getch();

        switch(ch) {
            case 'q': /* quit program */
                move(1, 1);
                stop = QUIT;
                break;

            case 'p': /* play */
                move(1, 1);
                play = PLAY;
                break;

            case 'f': /* ffwd */
                move(1, 1);
                play = FFWD;
                break;

            case 'r': /* rew */
                move(1, 1);
                play = REW;
                break;

            case 'n': /* next */
                move(1, 1);
                play = NEXT;
                break;

            case 'v': /* prev */
                move(1, 1);
                play = PREV;
                break;

            case 'a': /* pause */
                move(1, 1);
                play = PAUSE;
                break;

            case 's': /* stop */
                move(1, 1);
                play = STOP;
                break;

            case 'l': /* back to playlist */
                move(1, 1);
                stop = GOTO_EDITOR;
                break;

        }
        move(1, 1);
        wnoutrefresh(titlewin);
        wnoutrefresh(mainwin);
        wnoutrefresh(helpwin);
        doupdate();

    }

    delwin(titlewin);
    delwin(mainwin);
    delwin(helpwin);

    return stop;
}

int edit_playlist(int argc, char *argv[]) {

    WINDOW *dirwin, *playwin;  /* our two windows */
    int stop = CONT; /* stopping condition for the program */
    int y = 1;       /* the y position (within a given window) */
    int win = 0;     /* window number. 0=dirwin, 1=playwin */

    int dirwin_width, playwin_width;    /* width of our windows */
    int dirwin_height, playwin_height;  /* height of our windows */
    int dirwin_first = 0, playwin_first = 0; /* first item in window */
    int dirwin_start, playwin_start; /* starting position of windows */

    char start_dir[150] = "";

    if (argc == 2)
        strcat(start_dir, argv[1]);

    init_dirlist(start_dir); /* initialize our two lists */
    init_playlist();

    dirwin_height = LINES/2;  /* window sizes */
    playwin_height = LINES - dirwin_height;
    dirwin_width = COLS;
    playwin_width = COLS;

    dirwin_start = 0;
    playwin_start = dirwin_height;

    /* create windows */
    dirwin =  newwin(dirwin_height, 0, 0, 0);
    playwin = newwin(playwin_height, 0, dirwin_height, 0);

    /* draw boxes in windows and fill them with something */
    box(dirwin, 0, 0);
    fillwin(dirwin, &dirlist, dirwin_first,
            dirwin_width, dirwin_height);
    box(playwin, 0, 0);
    fillwin(playwin, &playlist, playwin_first,
            playwin_width, playwin_height);

    /* refresh screen */
    refresh();
    wnoutrefresh(dirwin);
    wnoutrefresh(playwin);
    doupdate();

    /* move cursor to its start position */
    move(1,1);

    while(stop == CONT) {

        int ch = getch();

        switch(ch) {
            case 'p': /* start playing! */
                move(0, 0);
                stop = GOTO_PLAYER;
                break;

            case 'q': /* quit program */
                move(0, 0);
                stop = QUIT;
                break;

            case KEY_UP: /* move up in window, scroll if needed */
                if (y > 1)
                    y--;
                else {
                    if (win == 0 && dirwin_first > 0) {
                        dirwin_first--;
                        fillwin(dirwin,&dirlist,dirwin_first,
                                dirwin_width, dirwin_height);
                    }
                    else if (win == 1 && playwin_first > 0) {
                        playwin_first--;
                        fillwin(playwin,&playlist,playwin_first,
                                playwin_width, playwin_height);
                    }
                }
                break;

            case KEY_DOWN: /* move down in window, scroll if needed */
                if (win == 0 && y+dirwin_first < dirlist.cur) {
                    if (y == dirwin_height - 2) {
                        dirwin_first++;
                        fillwin(dirwin,&dirlist,dirwin_first,
                                dirwin_width, dirwin_height);
                    }
                    else y++;
                }
                else if (win == 1 && y+playwin_first < playlist.cur) {
                    if (y == playwin_height - 2) {
                        playwin_first++;
                        fillwin(playwin,&playlist,playwin_first,
                                playwin_width, playwin_height);
                    }
                    else y++;
                }
                break;

            case KEY_LEFT: /* remove item (only if in playlist win) */
                if (win == 1 && playlist.cur > 0) {
                    delete(&playlist, y+playwin_first-1);
                    wclear(playwin);
                    box(playwin, 0, 0);
                    fillwin(playwin,&playlist,playwin_first,
                            playwin_width, playwin_height);
                    if (y+playwin_first > playlist.cur)
                        if (y > 1) y--;

                }
                break;

            case KEY_RIGHT: /* browse to new dir or add to playlist*/
                if (win == 0) {
                    if (isfile(&dirlist, y+dirwin_first-1)) {
                        add(&playlist, cwd, dirlist.files[y+dirwin_first-1]);
                        fillwin(playwin,&playlist,playwin_first,
                                playwin_width, playwin_height);
                    }
                    else {
                        init_dirlist(dirlist.dirs[y+dirwin_first-1]);
                        y = 1;
                        dirwin_first = 0;
                        wclear(dirwin);
                        box(dirwin, 0, 0);
                        fillwin(dirwin, &dirlist, dirwin_first,
                                dirwin_width, dirwin_height);
                    }
                }
                break;

            case '\t': /* use tab to move between windows */
                if (win == 1) win = 0;
                else win = 1;
                y = 1;
                break;

            case 'r': /* r to randomize the playlist */
                randomize_list(&playlist);
                playwin_first = 0;
                fillwin(playwin,&playlist,playwin_first,
                        playwin_width, playwin_height);
                if (win == 1) y = 1;
                break;

        }
        move(y + win*dirwin_height, 1);
        wnoutrefresh(dirwin);
        wnoutrefresh(playwin);
        doupdate();

    }

    delwin(playwin);
    delwin(dirwin);

    return stop;

}

int main(int argc, char *argv[]) {

    int stop = GOTO_EDITOR;   /* stopping condition for the program */

    initscr(); /* ncurses init stuff */
    keypad(stdscr, TRUE);
    cbreak();
    noecho();

    while(stop != QUIT) {

        if (stop == GOTO_PLAYER)
            stop = play_playlist(argc, argv);

        else if (stop == GOTO_EDITOR)
            stop = edit_playlist(argc, argv);


    }

    endwin();
    return 1;

}

