/* gamp.c v0.2.3
   by grub <grub@extrapolation.net> and borys <borys@bill3.ncats.net>
   based on amp by Tomislav Uzelac <tuzelac@rasip.fer.hr>

   ncurses based command line interface to amp. has a directory browser
   and other fun stuff like those other x based clients, but for the
   hardcore command line enthusiast.

*/

/* uncomment this to ignore id3 tags */
/* #define IGNORE_ID3TAGS */

#include "gamp.h"
#include "gamp-util.h"
#include "spectrum.h"

/* constants from args.c (which we dont call)
int SHOW_HEADER=FALSE;
int SHOW_HEADER_DETAIL=FALSE;
int SHOW_SIDE_INFO=FALSE;
int SHOW_SIDE_INFO_DETAIL=FALSE;
int SHOW_MDB=FALSE;
int SHOW_MDB_DETAIL=FALSE;
int SHOW_HUFFMAN_ERRORS=FALSE;
int SHOW_HUFFBITS=FALSE;
int SHOW_SCFSI=FALSE;
int SHOW_BLOCK_TYPE=FALSE;
int SHOW_TABLES=FALSE;
int SHOW_BITRATE=FALSE;
int AUDIO_BUFFER_SIZE = 100*1024;

int A_DUMP_BINARY=FALSE;
int A_QUIET=FALSE;
int A_FORMAT_WAVE=FALSE;
int A_SHOW_CNT=FALSE;
int A_WRITE_TO_FILE=FALSE;
int A_MSG_STDOUT=FALSE;
int A_SHOW_TIME = TRUE;
int A_AUDIO_PLAY = TRUE;
*/
/* program constants. used to indicate play status and for breaking out of
   loops and changing function from player to playlist and back. */

int bar_heights[32];

int display_spectrum;

STRLIST dirlist  = { 0, 0, 0, 0 };
STRLIST playlist = { 0, 0, 0, 0 };
char cwd[256];
char tmpstr[150];

#define LOGO_X 50
#define LOGO_Y 20
char logo[LOGO_Y][LOGO_X];
int logo_width;
int logo_height;

pthread_t player_thread; /* the player thread */
pthread_mutex_t mut, readMut;
pthread_cond_t cond;

int stop;   /* stopping condition for the program */
int play;
int current;

int time_mode;
int length;

int ch;
int currentVolume;
char filename[150];
char tmpstring[150];
char tmpname[150];
char start_dir[150];

int first_loop;
int last_loop;
struct AUDIO_HEADER header;

int cnt;

/* our windows */
WINDOW *dirwin, *playwin, *helpwin, *titlewin, *mainwin;

int file_status;
ID3_tag *ptrtag;

/*
 * opens the next mp3 for playback. calls show_filename to display the
 * name of the song in the titlewin.
 */
int openAndSetup() {

    if(stop == PLAYER) {
        clear_filename(titlewin);
        show_filename(titlewin, &playlist, current);
        refresh();
        wnoutrefresh(titlewin);
        wnoutrefresh(mainwin);
        wnoutrefresh(helpwin);
        doupdate();
    }

    strcpy(filename, playlist.dirs[current]);
    strcat(filename, "/");
    strcat(filename, playlist.files[current]);

    /* lock so we're only doing one of open/close/read at a time */
    pthread_mutex_lock(&readMut);

    if (file_status == OPENED) {
        fclose(in_file);

        if (AUDIO_BUFFER_SIZE != 0)
            audioBufferClose();
        else
            audioClose();

        file_status = CLOSED;
    }

    if ((in_file=fopen(filename,"r"))==NULL) {
        die("Could not open file: %s\n", filename);
        return(-1);
    }

    file_status = OPENED;

    pthread_mutex_unlock(&readMut);

    /* initialize globals */
    append=data=nch=0;
    first_loop = FALSE;
    cnt = 0;

    return(0);
}

/*
 * closes the currently playing mp3 and gets ready to move onto the next
 * if one is available.
 */
int closeAndEnd() {

    /* lock so we're only doing one of open/close/read at a time */
    pthread_mutex_lock(&readMut);

    if (file_status == OPENED) {
        fclose(in_file);
        if (AUDIO_BUFFER_SIZE != 0)
            audioBufferClose();
        else
            audioClose();
    }

    file_status = CLOSED;

    pthread_mutex_unlock(&readMut);

    current++;

    clear_filename(titlewin);

    cnt = 0;
    last_loop = FALSE;
    first_loop = TRUE;

    if (current >= playlist.cur) {
        current = -1;
        wclear(mainwin);
        box(mainwin, 0, 0);
        refresh();
        wnoutrefresh(titlewin);
        wnoutrefresh(mainwin);
        wnoutrefresh(helpwin);
        doupdate();

        return(-1);
    }

    return(0);

}

/*
 * plays a single mp3 frame. calls processHeader, statusDisplay, and on
 * its first run through it will get the length of the track (in seconds)
 * via the get_time function.
 */
int playFrame() {

    int retval = 0;

    /* lock so we're only doing one of open/close/read at a time */
    pthread_mutex_lock(&readMut);

    if (processHeader(&header,cnt))
        last_loop = TRUE;

    else {
        statusDisplay(titlewin, mainwin, LINES - 8, COLS - 2, &header, cnt);

        /* setup the audio when we have the frame info */
        if (!cnt) {

            length = get_time(&header);

            if (AUDIO_BUFFER_SIZE == 0)
                audioOpen(t_sampling_frequency[header.ID][header.sampling_frequency],
                          (header.mode!=3),currentVolume);
            else
                audioBufferOpen(t_sampling_frequency[header.ID][header.sampling_frequency],
                                (header.mode!=3),currentVolume);
        }

        if (layer3_frame(&header,cnt)) {
            warn(" error. blip.\n");
            retval = 1;
            last_loop = TRUE;
        } else
            cnt++;

    }

    pthread_mutex_unlock(&readMut);

    return(retval);

}

/*
 * this function is run as a separate thread and it plays the songs. if
 * the player is not set to 'play' it will wait until signaled to
 * continue.
 */
void *play_song(void *arg) {

    int frame_size = 0;

    /* _the_ loop */
    for(;;){
        if (play == PLAY) {
            if (first_loop == TRUE) {
                openAndSetup();
            }

            playFrame();

            if (last_loop == TRUE)
                if (closeAndEnd() < 0) {
                    pthread_mutex_lock(&mut);
                    play = STOP;
                    pthread_mutex_unlock(&mut);
                }
        } else if ((play == NEXT) || (play == PREV)) {

            play = PLAY;
            cnt = 0;
            first_loop = TRUE;
            last_loop = FALSE;

        } else if ((play == FFWD) && (file_status == OPENED)) {

    if (processHeader(&header,cnt))
        last_loop = TRUE;
            frame_size = bits_per_frame(&header);
            sackbits(frame_size);
    if (processHeader(&header,cnt))
        last_loop = TRUE;
            frame_size = bits_per_frame(&header);
            sackbits(frame_size);
    if (processHeader(&header,cnt))
        last_loop = TRUE;
            frame_size = bits_per_frame(&header);
            sackbits(frame_size);
    if (processHeader(&header,cnt))
        last_loop = TRUE;
            frame_size = bits_per_frame(&header);
            sackbits(frame_size);
    if (processHeader(&header,cnt))
        last_loop = TRUE;
            frame_size = bits_per_frame(&header);
            sackbits(frame_size);

            cnt += 5;
/*            fseek(in_file, frame_size, SEEK_CUR);
*/            play = PLAY;

        } else if ((play == REW) && (file_status == OPENED)) {

/*            frame_size = bits_per_frame(&header);
            cnt -= 8;
            fseek(in_file, -frame_size, SEEK_CUR);
*/            play = PLAY;

        } else {

            pthread_mutex_lock(&mut);
            pthread_cond_wait(&cond, &mut);
            pthread_mutex_unlock(&mut);

        }
    }

    return NULL;
}

/*
 * reads in the mp3 header and checks it. also reads the crc bit to get it
 * out of the way.
 */
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
            warn(" gamp does not support this format. sorry.\n");
        } else {
            /* here we should check whether it was an EOF or a sync err. later. */
            if (A_FORMAT_WAVE) wav_end(header);

            /* this is the message we'll print once we check for sync err.
             * warn(" gamp encountered a synchronization error\n");
             * warn(" sorry.\n");
             */
        }
        return(1);
    }

    /* crc is read just to get out of the way.
     */
    if (header->protection_bit == 0) getcrc();

    return(0);
}

/*
 * displays status (well, more than status). updates the elapsed/remaining
 * time once every 10 frames if the spectrum is off. if spectrum is on it
 * updates the spectrum and time and redisplays every other fram.
 */
void statusDisplay(WINDOW *twin, WINDOW *mwin, int height, int width,
                   struct AUDIO_HEADER *header, int frameNo) {

    int minutes,seconds;
    if (stop == PLAYER) {

    if (!(frameNo%10)) {
	seconds=frameNo*1152/t_sampling_frequency[header->ID][header->sampling_frequency];

        if (time_mode == ELAPSED) {
            minutes = seconds / 60;
            seconds = seconds % 60;

            sprintf(tmpstr, " [%2d:%02d]", minutes, seconds);
            mvwaddstr(twin, 1, 1, tmpstr);
            wnoutrefresh(twin);
        } else {
            seconds = length - seconds;

            if (seconds < 0) seconds = 0;

            minutes = seconds / 60;
            seconds = seconds % 60;

            sprintf(tmpstr, " [-%d:%02d]", minutes, seconds);
            mvwaddstr(twin, 1, 1, tmpstr);
            wnoutrefresh(twin);
        }
    }

    if (display_spectrum == SHOW_SPECTRUM) {
        sanalyzer_render_freq(mwin, height, width);
        if (!(frameNo%2)) {
            refresh();
            wnoutrefresh(mwin);
            doupdate();
        }
    } else {
        if (!(frameNo%10)) {
            refresh();
            wnoutrefresh(mwin);
            doupdate();
        }
    }

    fflush(stderr);
    }
}

/*
 * initializes the playlist to be empty with an initial size of 20 entries.
 */
void init_playlist(void) {
    free_list(&playlist);
    init_list(&playlist, 20);
}

/*
 * initialize the directory list. this is called upon entering a new
 * directory or upon entering the playlist browser.
 */
void init_dirlist(char *pwd) {
    DIR *dir;
    struct dirent *entry;
    struct stat dstat;
    char blank[5]="";

    if (pwd[0] == '\0') {
        getcwd(cwd, sizeof(cwd));
    } else {
        chdir(pwd);
        getcwd(cwd, sizeof(cwd));
    }

    free_list(&dirlist);
    init_list(&dirlist, 30);

    if((dir = opendir(cwd)) == NULL) die("cant open dir");

    while((entry = readdir(dir))) {

        if(strcmp(".", entry->d_name) == 0)  continue;
        if(stat(entry->d_name, &dstat) != 0) continue;

        if(S_ISDIR(dstat.st_mode))
            add(&dirlist, entry->d_name, blank, entry->d_name);

        else if(S_ISREG(dstat.st_mode)) {
            if (ismp3(entry->d_name))
                add(&dirlist, blank, entry->d_name, getName(entry->d_name));

        } else continue;

    }

    sort_list(&dirlist);
}

/*
 * fills a given window with contents of the list, starting with item
 * 'first' from the list. only displays enough items to fill the window.
 * will truncate names so that they fit across the window.
 */
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

/*
 * shows a filename in the given window, offset by 9 characters to allow
 * room for the time to be displayed.
 */
void show_filename(WINDOW *win, STRLIST *list, int index) {
    char *dirfilecat;

    dirfilecat = malloc(sizeof(char) * (strlen(list->names[index]) + 9));

    strcpy(dirfilecat, "         ");
    strcat(dirfilecat, list->names[index]);
    strtrunc(dirfilecat, COLS-2);

    mvwaddstr(win, 1, 1, tmpstr);
}

/*
 * displays the gamp logo in the center of the window! :)
 */
void show_logo(WINDOW *win, int win_height, int win_width) {

    int i = 0, x_offset = 0, y_offset = 0;

    x_offset = (win_width - logo_width) / 2;
    y_offset = (win_height - logo_height) / 2;

    if (x_offset < 0) x_offset = 0;
    if (y_offset < 0) y_offset = 0;

    for (i = 0; i < logo_height; i++)
        mvwaddstr(win, i+y_offset, x_offset, logo[i]);
}

/*
 * clears the filename from the given window. this is sort of ugly, so i
 * should probably fix it one of these days.
 */
void clear_filename(WINDOW *win) {
    char dirfilecat[150];

    strcpy(dirfilecat, "    ");
    strtrunc(dirfilecat, COLS-2);

    mvwaddstr(win, 1, 1, tmpstr);
}

/*
 * initialization routine for the player. sets up the windows, displays
 * information if the track is already playing, shows the help menu at the
 * bottom.
 */
void init_player() {

    delwin(titlewin);
    delwin(mainwin);

    /* create windows */
    titlewin = newwin(3, 0, 0, 0);
    mainwin = newwin(LINES - 6, 0, 3, 0);
    helpwin = newwin(3, 0, LINES - 3, 0);

    /* draw boxes in windows and fill them with something */
    box(titlewin, 0, 0);
    box(mainwin, 0, 0);
    box(helpwin, 0, 0);

    if (play == PLAY)
        show_filename(titlewin, &playlist, current);

    if (display_spectrum == NO_SPECTRUM)
        show_logo(mainwin, LINES - 8, COLS - 2);

    strcpy(tmpstring, " Play Stop pLaylist Quit Ffwd Rew Next preV");
    strcat(tmpstring, " pAuse Display Time");
    mvwaddstr(helpwin, 1, 1, tmpstring);

    /* move cursor to its start position */
    move(1,1);

    /* refresh screen */
    refresh();
    wnoutrefresh(titlewin);
    wnoutrefresh(mainwin);
    wnoutrefresh(helpwin);
    doupdate();

}

/*
 * the player function. this controls all the keyboard input while in
 * player mode. returns info about what part of the program to go to next.
 */
int play_playlist(int argc, char *argv[]) {

    if (play != PLAY) {
        first_loop = TRUE;
        last_loop = FALSE;

        cnt = 0;
    }

    init_player();

    while(stop == PLAYER) {

        ch = wgetch(titlewin);
        switch(ch) {
            case 'q': /* quit program */
                pthread_mutex_lock(&mut);
                play = STOP;
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mut);
                stop = QUIT;
                break;

            case 'p': case ' ': /* play */
                if (playlist.cur > 0) {
                    if ((current >= playlist.cur) || (current < 0))
                        current = 0;
                    pthread_mutex_lock(&mut);
                    play = PLAY;
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mut);
                }
                break;

            case 'f': /* ffwd */
/*                if ((play == PLAY) || (play == PAUSE)) {
                    pthread_mutex_lock(&mut);
                    play = FFWD;
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mut);
                }
*/                break;

            case 'r': /* rew */
/*                if ((play == PLAY) || (play == PAUSE)) {
                    pthread_mutex_lock(&mut);
                    play = REW;
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mut);
                }
*/                break;

            case 'n': /* next */
            case KEY_RIGHT:
                if (current < playlist.cur - 1) {
                    current++;
                    pthread_mutex_lock(&mut);
                    play = NEXT;
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mut);
                }
                break;

            case 'v': /* prev */
            case KEY_LEFT:
                if (current > 0) {
                    current--;
                    pthread_mutex_lock(&mut);
                    play = PREV;
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mut);
                }
                break;

            case 'a': /* pause */
                if (play == PLAY) { /* pause if playing */
                    pthread_mutex_lock(&mut);
                    play = PAUSE;
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mut);
                }
                else if (play == PAUSE) { /* unpause if already paused */
                    pthread_mutex_lock(&mut);
                    play = PLAY;
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mut);
                }
                break;

            case '+': case '=': /* increase volume */
                if (currentVolume < 100) {
                    currentVolume += 5;
                    if (currentVolume > 100) currentVolume = 100;
                    audioSetVolume(currentVolume);
                }
                break;

            case '-': case '_': /* decrease volume */
                if (currentVolume > 0) {
                    currentVolume -= 5;
                    if (currentVolume < 0) currentVolume = 0;
                    audioSetVolume(currentVolume);
                }
                break;

            case 'd': /* toggle visuals on/off */
                wclear(mainwin);
                box(mainwin, 0, 0);
                if (display_spectrum == NO_SPECTRUM)
                    display_spectrum = SHOW_SPECTRUM;
                else
                    display_spectrum = NO_SPECTRUM;

                init_player();
                break;

            case 't': /* toggle time mode between elapsed/remaining */
                if (time_mode == ELAPSED)
                    time_mode = REMAINING;
                else
                    time_mode = ELAPSED;
                break;

            case 's': /* stop */
                if ((play == PLAY) || (play == PAUSE)) {
                    pthread_mutex_lock(&mut);
                    play = STOP;
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mut);

                    init_player();
                }
                else if (play == STOP) {
                    if (playlist.cur > 0)
                        current = -1;
                }

                break;

            case 'l': /* back to playlist */
                stop = PLAYLIST;
                break;

        }

    }

    return stop;
}

/*
 * the playlist editor. sets up the windows and allows you to create/alter
 * your playlist.
 */
int edit_playlist(int argc, char *argv[]) {

    int y = 1;       /* the y position (within a given window) */
    int win = 0;     /* window number. 0=dirwin, 1=playwin */
    int i = 0;       /* loop counter */

    int dirwin_width, playwin_width;    /* width of our windows */
    int dirwin_height, playwin_height;  /* height of our windows */
    int dirwin_first = 0, playwin_first = 0; /* first item in window */
    int dirwin_start, playwin_start; /* starting position of windows */

    dirwin_height = (LINES - 3)/2;  /* window sizes */
    playwin_height = LINES - dirwin_height - 3;
    dirwin_width = COLS;
    playwin_width = COLS;

    dirwin_start = 0;
    playwin_start = dirwin_height;

    /* create windows */
    dirwin =  newwin(dirwin_height, 0, 0, 0);
    playwin = newwin(playwin_height, 0, dirwin_height, 0);
    helpwin = newwin(3, 0, dirwin_height + playwin_height, 0);

    /* draw boxes in windows and fill them with something */
    box(dirwin, 0, 0);
    fillwin(dirwin, &dirlist, dirwin_first,
            dirwin_width, dirwin_height);
    box(playwin, 0, 0);
    fillwin(playwin, &playlist, playwin_first,
            playwin_width, playwin_height);
    box(helpwin, 0, 0);

    strcpy(tmpstr, " Quit Clear All (tab)switch (left)remove ");
    strcat(tmpstr, "(right)add/browse Player Random");
    mvwaddstr(helpwin, 1, 1, tmpstr);

    /* refresh screen */
    refresh();
    wnoutrefresh(dirwin);
    wnoutrefresh(playwin);
    wnoutrefresh(helpwin);
    doupdate();

    /* move cursor to its start position */
    move(1,1);

    while(stop == PLAYLIST) {

        int ch = getch();

        switch(ch) {
            case 'p': /* start playing! */
                move(0, 0);
                stop = PLAYER;
                break;

            case 'q': /* quit program */
                move(0, 0);
                stop = QUIT;
                break;

            case 'c': /* clear the playlist */
                if (play == PLAY) {
                    pthread_mutex_lock(&mut);
                    play = STOP;
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mut);
                }
                current = -1;
                init_playlist();
                wclear(playwin);
                box(playwin, 0, 0);
                break;

            case 'a': /* add all in current directory */
                for (i = 0; i < dirlist.cur; i++) {
                    if (isfile(&dirlist, i))
                        add(&playlist, cwd, dirlist.files[i], dirlist.names[i]);
                }
                fillwin(playwin,&playlist,playwin_first,
                        playwin_width, playwin_height);
                break;

            case ' ': /* scroll down by one page (if possible) */
                if (win == 0) {
                    if (dirwin_first+dirwin_height-2 < dirlist.cur) {
                        dirwin_first += dirwin_height - 2;
                        if (dirwin_first + dirwin_height - 2 > dirlist.cur)
                            dirwin_first = dirlist.cur - dirwin_height + 2;
                    } else {
                        y = dirwin_height - 2;
                        if (y > dirlist.cur) y = dirlist.cur;
                    }

                    fillwin(dirwin, &dirlist, dirwin_first,
                            dirwin_width, dirwin_height);
                }
                else if (win == 1) {
                    if (playwin_first+playwin_height-2 < playlist.cur) {
                        playwin_first += playwin_height - 2;
                        if (playwin_first + playwin_height - 2 > playlist.cur)
                            playwin_first = playlist.cur - playwin_height + 2;
                    } else {
                        y = playwin_height -2;
                        if (y > playlist.cur) y = playlist.cur;
                    }

                    fillwin(playwin, &playlist, playwin_first,
                            playwin_width, playwin_height);
                }
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

                    /* if we are currently playing then we need to adjust
                       the pointer to the current playing song */
                    if (play == PLAY) {
                        if (y+playwin_first-1 < current)
                            current--;
                        else if (y+playwin_first-1 == current) {
                            pthread_mutex_lock(&mut);
                            play = NEXT;
                            pthread_cond_signal(&cond);
                            pthread_mutex_unlock(&mut);
                        }
                    }
                }
                break;

            case KEY_RIGHT: /* browse to new dir or add to playlist*/
                if (win == 0) {
                    if (isfile(&dirlist, y+dirwin_first-1)) {
                        add(&playlist, cwd,
                            dirlist.files[y+dirwin_first-1],
                            dirlist.names[y+dirwin_first-1]);
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
                randomize_list(&playlist, &current);
                playwin_first = 0;
                fillwin(playwin,&playlist,playwin_first,
                        playwin_width, playwin_height);
                if (win == 1) y = 1;
                break;

        }
        move(y + win*dirwin_height, 1);
        wnoutrefresh(dirwin);
        wnoutrefresh(playwin);
        wnoutrefresh(helpwin);
        doupdate();

    }

    delwin(playwin);
    delwin(dirwin);
    delwin(helpwin);

    return stop;

}

/*
 * this is the  main function. it does some initial setup and then passes
 * things over to the playlist editor. also responsible for switching
 * between modes within the program.
 */
int main(int argc, char **argv) {

    int argPos;
    int i;
    char *env_ptr;

    pthread_mutex_init(&mut, NULL);
    pthread_mutex_init(&readMut, NULL);
    pthread_cond_init(&cond, NULL);

    stop = PLAYLIST;   /* stopping condition for the program */
    play = STOP;
    current = -1;
    filename[0] = '\0';
    display_spectrum = NO_SPECTRUM;

    time_mode = REMAINING;

    first_loop = TRUE;
    last_loop = FALSE;

    file_status = CLOSED;

    length = 0;
    cnt = 0;

    start_dir[0] = '\0';
 
    for (i = 0; i <= NUM_BANDS; i++)
        bar_heights[i] = 0;

    argPos = args(argc,argv); /* process command line arguments */

    /* premultiply dewindowing tables.  Should go away */
    premultiply();

    /* init imdct tables */
    imdct_init();

    if (argPos <= argc) {

        env_ptr = getenv("GAMPDIR");

        if (env_ptr != NULL)
            strcpy(start_dir, env_ptr);

        if (argPos == argc - 1)
            strcpy(start_dir, argv[argPos]);

        init_dirlist(start_dir); /* initialize our two lists */
        init_playlist();

        logo_width = 0;
        logo_height = 0;
        read_logo();

        initscr(); /* ncurses init stuff */
        keypad(stdscr, TRUE);
        cbreak();
        noecho();

        pthread_create(&player_thread, NULL, play_song, NULL);
        pthread_detach(player_thread);

        while(stop != QUIT) {

            if (stop == PLAYER)
                stop = play_playlist(argc, argv);

            else if (stop == PLAYLIST)
                stop = edit_playlist(argc, argv);

        }

        endwin();

    } else
        displayUsage();

    return 0;

}

