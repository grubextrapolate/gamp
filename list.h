#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct {
    char **files;
    char **dirs;
    char **names;
    int cur;  /* how many strings are in use */
    int max;  /* how many strings we've allocated space for */
} STRLIST;

extern void grow_list(STRLIST *);
extern void add(STRLIST *, char*, char *, char *);
extern void delete(STRLIST *, int);
extern void swap(STRLIST *, int, int);
extern int isdir(STRLIST *, int);
extern int isfile(STRLIST *, int);
extern void sort_list(STRLIST *);
extern void randomize_list(STRLIST *);
extern void free_list(STRLIST *);
extern void init_list(STRLIST *, int);

#endif /* LIST_H */
