/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* formats.h
 *
 * Created by: tomislav uzelac Dec 22 1996
 */

#ifdef FORMATS_H
#define FORMATS_H

extern void wav_end(struct AUDIO_HEADER *header);
extern void wav_begin(void);

void wav_end(struct AUDIO_HEADER *header);
void wav_begin(void);
#endif /* FORMATS_H */
