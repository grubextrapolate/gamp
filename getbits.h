/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* getbits.h
 *
 * Created by: tomislav uzelac  Apr 1996
 */

#ifndef GETBITS_H
#define GETBITS_H

/* gethdr() error codes: NS == not supported
*/
#define GETHDR_ERR 1
#define GETHDR_NS  2

/* buffer for the 'bit reservoir'
*/
#define BUFFER_SIZE     4096
#define BUFFER_AUX      2048
extern unsigned char buffer[];
extern int append;
extern int data;
  
/* exports
*/
extern void getinfo(struct AUDIO_HEADER *header,struct SIDE_INFO *info);
extern int gethdr(struct AUDIO_HEADER *header);
extern void getcrc();
extern void fillbfr(int advance);
extern unsigned int viewbits(int n);
extern void sackbits(int n);
extern unsigned int getbits(int n);

#ifdef GETBITS

/* buffer, AUX is used in case of input buffer "overflow", and its contents
 * are copied to the beginning of the buffer
*/
unsigned char buffer[BUFFER_SIZE+BUFFER_AUX];

/* buffer pointers 
 * append counts in bytes, data in bits
 */
  int append;
  int data;

/* internal buffer, _bptr holds the position in _bits_
*/
static unsigned char _buffer[32];
static int _bptr;

inline unsigned int _getbits(int n);
static inline void _fillbfr(int size);
void getinfo(struct AUDIO_HEADER *header,struct SIDE_INFO *info);
int gethdr(struct AUDIO_HEADER *header);
void getcrc();
void fillbfr(int advance);
unsigned int viewbits(int n);
void sackbits(int n);
unsigned int getbits(int n);
  
#endif /* GETBITS */

#endif /* GETBITS_H */
