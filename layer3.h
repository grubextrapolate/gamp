/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/

/* layer3.h
 *
 * Created by: tomislav uzelac  Mar  1 97
 * Last modified by:
 */

#ifndef LAYER3_H
#define LAYER3_H

extern int layer3_frame(struct AUDIO_HEADER *header,int cnt);

#ifdef LAYER3

int layer3_frame(struct AUDIO_HEADER *header,int cnt);

#endif /* LAYER3 */

#endif /* LAYER3_H */
