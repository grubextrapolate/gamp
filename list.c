#include "list.h"

void grow_list(STRLIST *list) {
    char **tmpfiles = (char **)malloc(sizeof(char *) * list->max * 2);
    char **tmpdirs = (char **)malloc(sizeof(char *) * list->max * 2);
    char **tmpnames = (char **)malloc(sizeof(char *) * list->max * 2);
    memcpy(tmpfiles, list->files, list->cur*sizeof(char *));
    memcpy(tmpdirs, list->dirs, list->cur*sizeof(char *));
    memcpy(tmpnames, list->names, list->cur*sizeof(char *));
    free(list->files);
    free(list->dirs);
    free(list->names);
    list->files = tmpfiles;
    list->dirs = tmpdirs;
    list->names = tmpnames;
    list->max = list->max * 2;
}

void add(STRLIST *list, char* dir, char *file, char *name) {
    list->files[list->cur] = strdup(file);
    list->dirs[list->cur] = strdup(dir);
    list->names[list->cur++] = strdup(name);
    if(list->cur == list->max) grow_list(list);
}

void delete(STRLIST *list, int index) {
    int i;
    free(list->files[index]);
    free(list->dirs[index]);
    free(list->names[index]);
    for (i = index; i < list->cur - 1; i++) {
        list->files[i] = list->files[i+1];
        list->dirs[i] = list->dirs[i+1];
        list->names[i] = list->names[i+1];
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
    tmpPtr = list->names[first];
    list->names[first] = list->names[second];
    list->names[second] = tmpPtr;
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

    int i, num_items, j;

    char **tmpfiles = (char **)malloc(sizeof(char *) * list->max);
    char **tmpdirs = (char **)malloc(sizeof(char *) * list->max);
    char **tmpnames = (char **)malloc(sizeof(char *) * list->max);

    num_items = list->cur;
    i = 0;

    for (i = 0; list->cur > 0; i++) {

        /* number from 0 to list->cur - 1 */
        j = (int) (rand() % list->cur);

        tmpfiles[i] = strdup(list->files[j]);  /* move random item to */
        tmpdirs[i] = strdup(list->dirs[j]);    /* temp list.          */
        tmpnames[i] = strdup(list->names[j]);
        delete(list, j);

    }

    free(list->files);
    free(list->dirs);
    free(list->names);
    list->files = tmpfiles;
    list->dirs = tmpdirs;
    list->names = tmpnames;
    list->cur = num_items;
}

void free_list(STRLIST *list) {
    if ((list && list->files) || (list && list->dirs)) {
        while (list->cur--) {
           free(list->files[list->cur]);
           free(list->dirs[list->cur]);
           free(list->names[list->cur]);
        }
        free(list->files);
        free(list->dirs);
        free(list->names);
        list->max = 0;
    }
}

void init_list(STRLIST *list, int num) {
    if(list) {
        list->files = (char **)malloc(sizeof(char *) * num);
        list->dirs = (char **)malloc(sizeof(char *) * num);
        list->names = (char **)malloc(sizeof(char *) * num);
        list->cur = 0;
        list->max = num;
    }
}

