#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct {
    char **files;
    char **dirs;
    int cur;  /* how many strings are in use */
    int max;  /* how many strings we've allocated space for */
} STRLIST;

void grow_list(STRLIST *list) {
    char **tmpfiles = (char **)malloc(sizeof(char **) * list->max * 2);
    char **tmpdirs = (char **)malloc(sizeof(char **) * list->max * 2);
    memcpy(tmpfiles, list->files, list->cur*sizeof(char *));
    memcpy(tmpdirs, list->dirs, list->cur*sizeof(char *));
    free(list->files);
    free(list->dirs);
    list->files = tmpfiles;
    list->dirs = tmpdirs;
    list->max = list->max * 2;
}

void add(STRLIST *list, char* dir, char *file) {
    list->files[list->cur] = strdup(file);
    list->dirs[list->cur++] = strdup(dir);
    if(list->cur == list->max) grow_list(list);
}

void delete(STRLIST *list, int index) {
    int i;
    free(list->files[index]);
    free(list->dirs[index]);
    for (i = index; i < list->cur - 1; i++) {
        list->files[i] = list->files[i+1];
        list->dirs[i] = list->dirs[i+1];
    }
    list->cur--;
}

void swap(STRLIST *list, int first, int second) {
    char *tmpPtr;
    tmpPtr = list->dirs[first];
    list->dirs[first] = list->dirs[second];
    list->dirs[second] = tmpPtr;
    tmpPtr = list->files[first];
    list->files[first] = list->files[second];
    list->files[second] = tmpPtr;
}

int isdir(STRLIST *list, int entry) {
    if (strcmp(list->files[entry], "") == 0) return 1;
    else return 0;
}

int isfile(STRLIST *list, int entry) {
    if (strcmp(list->dirs[entry], "") == 0) return 1;
    else return 0;
}

void sort_list(STRLIST *list) {

    int i, j;
    /* do a bubble sort by both type (dir < file) and by alphabet. */

    for (j = 1; j <= list->cur - 1; j++) {
        for (i = 0; i <= list->cur - 2; i++) {

            /* dir and dir */
            if (isdir(list,i) && isdir(list,i+1)) {
                if (strcmp(list->dirs[i], list->dirs[i+1]) > 0)
                    swap(list, i, i+1);

            /* file and file */
            } else if (isfile(list, i) && isfile(list,i+1)) {
                if (strcmp(list->files[i], list->files[i+1]) > 0)
                    swap(list, i, i+1);

            /* file and dir so swap regardless*/
            } else if (isfile(list, i) && isdir(list, i+1))
                    swap(list, i, i+1);
        }
    }
}

void randomize_list(STRLIST *list) {

    int i, num_items, j, k;

    char **tmpfiles = (char **)malloc(sizeof(char **) * list->max);
    char **tmpdirs = (char **)malloc(sizeof(char **) * list->max);

    num_items = list->cur;
    i = 0;

    while (num_items > 0) {

        /* number from 0 to num_items-1 (according to man rand) */
        j = (int) (num_items*rand());

        tmpfiles[i] = list->files[j];  /* move random item to */
        tmpdirs[i] = list->dirs[j];    /* temp list.          */

        for (k = j; k < num_items - 1; k++) {  /* move rest of items up */
            list->dirs[k] = list->dirs[k+1];   /* in list to "delete"   */
            list->files[k] = list->files[k+1]; /* the random item.      */
        }

        i++; /* increment the counter of our temp list */
        num_items--; /* decrement the number of items in the old list */
    }

    free(list->files);
    free(list->dirs);
    list->files = tmpfiles;
    list->dirs = tmpdirs;

}

void free_list(STRLIST *list) {
    if ((list && list->files) || (list && list->dirs)) {
        while (list->cur--) {
           free(list->files[list->cur]);
           free(list->dirs[list->cur]);
        }
        free(list->files);
        free(list->dirs);
        list->max = 0;
    }
}

void init_list(STRLIST *list, int num) {
    if(list) {
        list->files = (char **)malloc(sizeof(char **) * num);
        list->dirs = (char **)malloc(sizeof(char **) * num);
        list->cur = 0;
        list->max = num;
    }
}

