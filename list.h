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
extern void popItem(ITEMLIST **);
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
extern ITEM *seekItemByPath(ITEM *, ITEMLIST *);

extern void cropList(ITEMLIST **);
extern void markAll(ITEMLIST *);
extern void unmarkAll(ITEMLIST *);
extern void markInvert(ITEMLIST *);
extern void deleteMarked(ITEMLIST **);
extern void reverseList(ITEMLIST *);
extern void moveItemToHead(ITEM *, ITEMLIST **);
extern void moveItemToTail(ITEM *, ITEMLIST **);
extern void moveMarkedToHead(ITEMLIST **);
extern void moveMarkedToTail(ITEMLIST **);
extern void replaceItem(ITEM *, ITEMLIST **, ITEM *);
extern void addMarked(ITEMLIST *, ITEMLIST **);
extern void addMarkedRecursive(ITEMLIST *, ITEMLIST **);
extern ITEM *seekBackItem(ITEM *, int *);

#endif /* LIST_H */
