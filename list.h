#ifndef LIST_H
#define LIST_H

#include "gamp.h"

extern AudioInfo *copyInfo(AudioInfo *);
extern ITEM *copyItem(ITEM *);
extern ITEM *newItem(char *, char *);
extern void addItem(ITEM *, ITEMLIST **);
extern void addOrdered(ITEM *, ITEMLIST **);
extern void addRecursive(ITEMLIST *, ITEMLIST **);
extern void removeItem(ITEM *, ITEMLIST **);
extern void deleteItem(ITEM *, ITEMLIST **);
extern void swapItems(ITEMLIST *, ITEM *, ITEM *);
extern int compItems(ITEM *, ITEM *);
extern int isdir(ITEM *);
extern int isfile(ITEM *);
extern void sortList(ITEMLIST *);
extern void moveItem(ITEM *, ITEMLIST **, ITEMLIST **);
extern ITEM *seekItem(int, ITEMLIST *);
extern ITEMLIST *randomizeList(ITEMLIST **);
extern void freeList(ITEMLIST **);
extern void initList(ITEMLIST **);
extern void freeItem(ITEM *);

#endif /* LIST_H */
