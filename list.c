#include "list.h"
#include <sys/stat.h>
#include <unistd.h>

#include <string.h>
#include <dirent.h>

AudioInfo *copyInfo(AudioInfo *orig) {
   AudioInfo *ret = NULL;

   if (orig != NULL) {
      ret = (AudioInfo *)malloc(sizeof(AudioInfo));
      if (ret == NULL) die("copyInfo: malloc error\n");

      memcpy(ret, orig, sizeof(AudioInfo));

      if (orig->ident != NULL) ret->ident = strdup(orig->ident);
      else ret->ident = NULL;
   }
   return(ret);

}

ITEM *copyItem(ITEM *orig) {

   ITEM *new = NULL;

   if (orig != NULL) {
      new = (ITEM *)malloc(sizeof(ITEM));
      if (new == NULL) die("copyItem: malloc error\n");
      new->path = strdup(orig->path);
      new->name = strdup(orig->name);
      new->next = NULL;
      new->prev = NULL;
      new->info = copyInfo(orig->info);
      new->size = orig->size;
      new->length = orig->length;
      new->isfile = orig->isfile;
      new->marked = orig->marked;

      if (orig->id3 != NULL) {
         new->id3 = (ID3_tag *)malloc(sizeof(ID3_tag));
         if (new->id3 == NULL) die("copyItem: malloc failure\n");
         memcpy((void *)new->id3, (void *)orig->id3, sizeof(ID3_tag));
      } else {
         new->id3 = NULL;
      }
   }
   return(new);
}

ITEM *newItem(char *path, char *name) {

   ITEM *itm = NULL;

   if ((itm = (ITEM *)malloc(sizeof(ITEM))) == NULL) die("malloc failure\n");

   itm->info = NULL;
   itm->next = NULL;
   itm->prev = NULL;
   itm->size = -1;
   itm->length = -1;
   itm->id3 = NULL;
   itm->marked = 0;

   if (name == NULL) { /* directory */
      itm->isfile = FALSE;

      itm->path = strdup(path);

      itm->name = rindex(itm->path, '/');
      if (itm->name != NULL) itm->name++;
      else die("newItem: oops, no / in full path? something's wrong\n");

   } else {
      itm->isfile = TRUE;

      itm->path = (char *)malloc(sizeof(char)*(strlen(path) + strlen(name) + 2));
      strcpy(itm->path, path);
      strcat(itm->path, "/");
      strcat(itm->path, name);

      itm->name = rindex(itm->path, '/');
      if (itm->name != NULL) itm->name++;
      else die("newItem: oops, no / in full path? something's wrong\n");

      getID3(itm);
   }

   return(itm);

}

/*
 * adds an item to the end of the list
 */
void addItem(ITEM *item, ITEMLIST **list) {

   if (*list == NULL) initList(list);

   if ((*list)->head == NULL) { /* no items on list yet */
      (*list)->head = item;
      (*list)->tail = item;
      (*list)->num++;
   } else { /* at least one item on list */
      item->prev = (*list)->tail;
      (*list)->tail->next = item;
      (*list)->tail = item;
      (*list)->num++;
   }
}

/*
 * adds items to the list in an ordered manner. resulting list will have
 * directories first (alphabetical) followed by files (alphabetical).
 */
void addOrdered(ITEM *item, ITEMLIST **list) {

   ITEM *cur = NULL;

   if (*list == NULL) initList(list);

   if ((*list)->head == NULL) { /* no items on list yet */
      (*list)->head = item;
      (*list)->tail = item;
      (*list)->num++;
   } else { /* at least one item on list */

      cur = (*list)->head;
      while ((cur != NULL) && (cur->name < item->name))
         cur = cur->next;

      if (cur == NULL) { /* adding to end of list */
         item->prev = (*list)->tail;
         (*list)->tail->next = item;
         (*list)->tail = item;

      } else { /* add before current item */
         item->next = cur;
         item->prev = cur->prev;
         cur->prev->next = item;
         cur->prev = item;
      }
      (*list)->num++;
   }
}

/*
 * recursive add function
 */
void addRecursive(ITEMLIST *src, ITEMLIST **dest) {

   ITEM *itm = NULL;
   ITEMLIST *tmp = NULL;

   if (src != NULL) {

      if (*dest == NULL) {
         initList(dest);
      }
      itm = src->head;
      while (itm != NULL) {
         if (itm->isfile) {
            addItem(copyItem(itm), dest);
         } else {
            if (strcmp(itm->name, "..") != 0) { /* want to ignore .. */
               initDirlist(itm->path, &tmp);
               sortList(tmp);
               addRecursive(tmp, dest);
            }
         }
         itm = itm->next;
      }
   }
   freeList(&tmp);
}

/*
 * removes the specified item from the given list. first verifies that the
 * item is actually on the list before removing it.
 */
void removeItem(ITEM *item, ITEMLIST **list) {

   ITEM *cur = NULL;

   if ((*list != NULL) && (item != NULL)) {

      cur = (*list)->head; /* check to make sure item is on this list */
      while ((cur != item) && (cur != NULL))
         cur = cur->next;

      if (cur != NULL) { /* item was found on list */

         /* check and adjust list pointers if necessary */
         if ((*list)->head == item) {
            (*list)->head = item->next;
         }
         if ((*list)->tail == item) {
            (*list)->tail = item->prev;
         }

         /* adjust pointers within list */
         if (item->prev != NULL) {
            cur = item->prev;
            cur->next = item->next;
         }
         if (item->next != NULL) {
            cur = item->next;
            cur->prev = item->prev;
         }

         item->prev = NULL;
         item->next = NULL;

         (*list)->num--; /* adjust list item count */

      } else {
         debug("removeItem: could not find item to remove\n");
      }

   }
}

/*
 * deletes the specified item from the given list. first verifies that the
 * item is actually on the list before deleting it.
 */
void deleteItem(ITEM *item, ITEMLIST **list) {

   removeItem(item, list);

   freeItem(item); /* free list item */

}

/*
 * popItem will delete the tail item from a list.
 */
void popItem(ITEMLIST **list) {

   ITEM *itm = NULL;
   if (*list != NULL) {
      itm = (*list)->tail;
      deleteItem(itm, list);
   }
}

/*
 * swaps two items on a given list. assumes that the items are both on the
 * list.
 */
void swapItems(ITEMLIST *list, ITEM *first, ITEM *second) {

   ITEM *f_p = NULL;
   ITEM *f_n = NULL;
   ITEM *s_p = NULL;
   ITEM *s_n = NULL;

   if ((first != NULL) && (second != NULL) && (list != NULL)) {

      /* adjust list head and tail if necessary */
      if (list->head == first) {
         list->head = second;
      } else if (list->head == second) {
         list->head = first;
      }
      if (list->tail == first) {
         list->tail = second;
      } else if (list->tail == second) {
         list->tail = first;
      }

      f_p = first->prev;
      f_n = first->next;
      s_p = second->prev;
      s_n = second->next;

      if (first->next == second) {

         if (f_p != NULL) f_p->next = second;
         first->next = second->next;
         second->next = first;

         if (s_n != NULL) s_n->prev = first;
         first->prev = second;
         second->prev = f_p;

      } else {

         /* adjust next pointers of first and second */
         first->next = s_n;
         second->next = f_n;

         /* adjust prev pointers of first and second */
         first->prev = s_p;
         second->prev = f_p;

         /* if there are items before or after first adjust their pointers */
         if (f_p != NULL) f_p->next = second;
         if (f_n != NULL) f_n->prev = second;

         /* if there are items before or after second adjust their pointers */
         if (s_p != NULL) s_p->next = first;
         if (s_n != NULL) s_n->prev = first;

      }
   }
}

/*
 * compares two items. will result in ordering alphabetically starting
 * with directories, then alphabetically by filename.
 */
int compItems(ITEM *p1, ITEM *p2) {

   int ret = 0;

   /* dir and dir, so return strcmp */
   if (!(p1->isfile) && !(p2->isfile)) {
      if (configuration.ignoreCase)
         ret = strcasecmp(p1->name, p2->name);
      else
         ret = strcmp(p1->name, p2->name);

   /* file and file, so return strcmp */
   } else if (p1->isfile && p2->isfile) {
      if (configuration.ignoreCase)
         ret = strcasecmp(p1->name, p2->name);
      else
         ret = strcmp(p1->name, p2->name);

   /* file and dir so file is always greater than dir, return > 0 */
   } else if (p1->isfile && !(p2->isfile)) {
      ret = 1;

   /* dir and file so file is always greater than dir, return < 0 */
   } else {
      ret = -1;
   }

   return(ret);

}

/*
 * checks if the given item is a directory. if it is, the function returns
 * TRUE, otherwise it returns FALSE.
 */
int isdir(ITEM *item) {
   return(!(item->isfile));
}

/*
 * checks if the given item is a file. if it is, the function returns
 * TRUE, otherwise it returns FALSE.
 */
int isfile(ITEM *item) {
   return(item->isfile);
}

/*
 * sorts a given list so that directories are first (alphabetically)
 * followed by files (alphabetically).
 */
void sortList(ITEMLIST *list) {

   ITEM *tmp = NULL;
   ITEM *p1 = NULL;
   ITEM *p2 = NULL;

   if (list == NULL) { /* insure there is a list to sort */
      debug("sortList: null list\n");
   } else if (list->head == list->tail) { /* only one item, no sorting */
      debug("sortList: one item list\n");
   } else {
      /* do a bubble sort by both type (dir < file) and by alphabet. */
      for (p1 = list->head; p1->next != NULL; p1 = p1->next) {
         for (p2 = p1->next; p2 != NULL; p2 = p2->next) {

            if (compItems(p1, p2) > 0) {
               swapItems(list, p1, p2);
               tmp = p1;
               p1 = p2;
               p2 = tmp;
            }

         }
      }
   }
}

/*
 * move item from one list to another
 */
void moveItem(ITEM *item, ITEMLIST **list1, ITEMLIST **list2) {
   removeItem(item, list1);

   addItem(item, list2);
}

/*
 * seek (and return) the ith item in a list
 */
ITEM *seekItem(int i, ITEMLIST *list) {

   ITEM *ret = NULL;
   int j = 0;

   if (list != NULL) {
      ret = list->head;
      for(j = 0; ((j < i) && (ret != NULL)); j++)
         ret = ret->next;
   }

   return(ret);
}

/*
 * puts a list in random order.
 */
ITEMLIST *randomizeList(ITEMLIST **list) {

   int j = 0;
   ITEM *item = NULL;
   ITEMLIST *newlist = NULL;

   if ((*list != NULL) && ((*list)->num > 1)) {
      initList(&newlist);
      while ((*list)->num > 0) {

         /* number from 0 to (*list)->num - 1 */
         srand(clock());
         j = (int) (rand() % (*list)->num);
         item = seekItem(j, *list);
         moveItem(item, list, &newlist);

      }

      freeList(list);

   } else {
      newlist = *list;
   }

   return(newlist);
}

/*
 * free the memory allocated to a list and all items on it.
 */
void freeList(ITEMLIST **list) {

   ITEM *ptr1 = NULL;

   if (*list != NULL) {

      ptr1 = (*list)->head;
      while (ptr1 != NULL) {

         deleteItem(ptr1, list);
         ptr1 = (*list)->head;

      }
      free(*list);
      *list = NULL;
   }
}

/*
 * allocate and initialize a list
 */
void initList(ITEMLIST **list) {

   if (*list == NULL) {
      (*list) = (ITEMLIST *)malloc(sizeof(ITEMLIST));
      (*list)->head = NULL;
      (*list)->tail = NULL;
      (*list)->num = 0;
   }
}

/*
 * free memory allocated to an item
 */
void freeItem(ITEM *item) {

   if (item != NULL) {
      free(item->path); /* free path. dont need to free name as it should
                           be a pointer into the path */
      if (item->id3 != NULL) free(item->id3); /* free id3 (if any) */
      if (item->info != NULL) free(item->info); /* free info (if any) */
   }
}

/*
 * seek (and return) the item by path
 */
ITEM *seekItemByPath(ITEM *itm, ITEMLIST *list) {

   ITEM *ret = NULL;

   if ((list != NULL) && (itm != NULL)) {
      ret = list->head;
      while((ret != NULL) && (strcmp(ret->path, itm->path) != 0))
         ret = ret->next;
   }
      
   return(ret);
}

/*
 * deletes all unmarked items from the list
 */
void cropList(ITEMLIST **list) {

   ITEM *cur = NULL;
   ITEM *ptr = NULL;

   if (*list != NULL) {
      cur = (*list)->head;
      while (cur != NULL) {
         ptr = cur->next;
         if (! cur->marked) deleteItem(cur, list);
         cur = ptr;
      }
   }
}

/*
 * marks all items on the list
 */
void markAll(ITEMLIST *list) {

   ITEM *cur = NULL;

   if (list != NULL) {
      cur = list->head;
      while (cur != NULL) {
         if (strcmp(cur->name, "..") != 0) {
            cur->marked = TRUE;
         }
         cur = cur->next;
      }
   }
}

/*
 * unmarks all items on the list
 */
void unmarkAll(ITEMLIST *list) {

   ITEM *cur = NULL;

   if (list != NULL) {
      cur = list->head;
      while (cur != NULL) {
         cur->marked = FALSE;
         cur = cur->next;
      }
   }
}

/*
 * inverts the mark on each item in the list
 */
void markInvert(ITEMLIST *list) {

   ITEM *cur = NULL;

   if (list != NULL) {
      cur = list->head;
      while (cur != NULL) {
         if (strcmp(cur->name, "..") != 0) {
            cur->marked = ! cur->marked;
         }
         cur = cur->next;
      }
   }
}

/*
 * deletes all marked items from the list. as this is simply the
 * opposite of a crop we invert the selections, crop the list to the
 * previously unselected items, then invert them back to being unmarked.
 */
void deleteMarked(ITEMLIST **list) {

   if (*list != NULL) {
      markInvert(*list);
      cropList(list);
      markInvert(*list);
   }
}

/*
 * reverses the order of the list
 */
void reverseList(ITEMLIST *list) {

   ITEM *top = NULL;
   ITEM *bot = NULL;
   ITEM *tmp = NULL;

   if (list != NULL) {
      top = list->head;
      bot = list->tail;
      while (top != NULL) {
         if (top == bot) { /* center of odd-sized list, so done. */
            top = NULL;
         } else if (top->next == bot) { /* center of even-sized list. */
            swapItems(list, top, bot);  /* swap and then we're done.  */
            top = NULL;
         } else {
            swapItems(list, top, bot); /* else swap top and bottom */
            tmp = bot->next;
            bot = top->prev;
            top = tmp;
         }
      }
   }
}

/*
 * moves the specified item to the head of the list
 */
void moveItemToHead(ITEM *item, ITEMLIST **list) {

   if ((item != NULL) && (*list != NULL)) {
      if (((*list)->head != (*list)->tail) &&
          ((*list)->head != NULL)) { /* more than one item */
         removeItem(item, list);
         item->next = (*list)->head;
         (*list)->head->prev = item;
         (*list)->head = item;
      }
   }
}

/*
 * moves the specified item to the tail of the list
 */
void moveItemToTail(ITEM *item, ITEMLIST **list) {

   if ((item != NULL) && (*list != NULL)) {
      if (((*list)->head != (*list)->tail) &&
          ((*list)->head != NULL)) { /* more than one item */
         removeItem(item, list);
         item->prev = (*list)->tail;
         (*list)->tail->next = item;
         (*list)->tail = item;
      }
   }
}

/*
 * moves marked items to the head of the list.
 */
void moveMarkedToHead(ITEMLIST **list) {

   ITEM *cur = NULL;
   ITEM *ptr = NULL;

   if (*list != NULL) {
      cur = (*list)->head;
      while (cur != NULL) {
         ptr = cur->next;
         if (cur->marked) moveItemToHead(cur, list);
         cur = ptr;
      }
   }
}

/*
 * moves marked items to the tail of the list
 */
void moveMarkedToTail(ITEMLIST **list) {

   ITEM *cur = NULL;
   ITEM *ptr = NULL;
   ITEM *fmark = NULL;

   if (*list != NULL) {
      cur = (*list)->head;
      while (cur != NULL) {
         ptr = cur->next;
         if (cur->marked) {
            if (fmark == NULL) { /* set this as first marked item */
               fmark = cur;
               moveItemToTail(cur, list);
            } else if (cur == fmark) { /* if we've hit fmark we're done */
               ptr = NULL;
            } else { /* otherwise move to tail */
               moveItemToTail(cur, list);
            }
         }
         cur = ptr;
      }
   }
}

/*
 * replaces the specified item in list1 with a copy of item2
 */
void replaceItem(ITEM *item1, ITEMLIST **list1, 
                 ITEM *item2) {

   ITEM *prev1 = NULL;
   ITEM *next1 = NULL;
   ITEM *ptr = NULL;

   if ((item1 != NULL) && (item2 != NULL) && (*list1 != NULL)) {

      ptr = copyItem(item2);
      ptr->next = item1->next;
      ptr->prev = item1->prev;

      prev1 = item1->prev;
      next1 = item1->next;

      if ((*list1)->head == item1) {
         (*list1)->head = ptr;
      } else {
         prev1->next = ptr;
      }

      if ((*list1)->tail == item1) {
         (*list1)->tail = ptr;
      } else {
         next1->prev = ptr;
      }

      freeItem(item1);
   }
}

/*
 * adds all marked items from src list to dest list. ignores selected
 * directories.
 */
void addMarked(ITEMLIST *src, ITEMLIST **dest) {

   ITEM *cur = NULL;
   ITEM *ptr = NULL;

   if (src != NULL) {
      cur = src->head;
      while (cur != NULL) {
         if ((isfile(cur)) && (cur->marked)) {
            ptr = copyItem(cur);
            ptr->marked = FALSE;
            addItem(ptr, dest);
         }
         cur = cur->next;
      }
   }
}

/*
 * adds all marked items from src list into dest list recursively. adds
 * songs and recursively adds directories. copies marked items into a
 * new list and then uses the addRecursive function on this list.
 */
void addMarkedRecursive(ITEMLIST *src, ITEMLIST **dest) {

   ITEMLIST *marklist = NULL;
   ITEM *cur = NULL;
   ITEM *ptr = NULL;

   if (src != NULL) {

      cur = src->head;
      while (cur != NULL) {
         if (cur->marked) {
            ptr = copyItem(cur);
            ptr->marked = FALSE;
            addItem(ptr, &marklist);
         }
         cur = cur->next;
      }
      addRecursive(marklist, dest);
      freeList(&marklist);
   }
}

/*
 * given a selected song and its position in the current window this
 * will work back to find out which song should be the first in the
 * window. used to maintain position of a selected song after the list
 * changes. if it hits the top of the list before moving to the position
 * it wanted it will alter pos to indicate where teh selected song
 * really is in the window.
 */
ITEM *seekBackItem(ITEM *cur, int *pos) {

   ITEM *ret = NULL;
   int i = 0;

   if (cur != NULL) {
      ret = cur;
      while ((ret->prev != NULL) && (i < *pos)) {
         ret = ret->prev;
         i++;
      }
      if (i < *pos) { /* didnt get as far up as we wanted */
         *pos = i;
      }
   }
   return(ret);
}
