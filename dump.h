/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* dump.h
 * 
 * Last modified by: tomislav uzelac Dec 22 1996
 */

#ifndef DUMP_H
#define DUMP_H

extern void dump(int *length);
extern inline void show_header(struct AUDIO_HEADER *header,int bitrate,int fs,int mean_frame_size,int cnt);
extern inline void show_side_info(struct SIDE_INFO *info,int gr,int ch,int mdb);

void dump(int *length);
inline void show_header(struct AUDIO_HEADER *header,int bitrate,int fs,int mean_frame_size,int cnt);
inline void show_side_info(struct SIDE_INFO *info,int gr,int ch,int mdb);

static char *t_modes[] = {
        "stereo","joint_stereo","dual_channel","single_channel"};

#endif /* DUMP_H */
