/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/

/* layer3.c  layer3 audio decoding
 *
 * Created by: tomislav uzelac  Mar  1 97
 * Last modified by: 
 */
#include "amp.h"
#include "audio.h"
#include "dump.h"
#include "getbits.h"
#include "getdata.h"
#include "huffman.h"
#include "misc2.h"
#include "transform.h"

#define LAYER3
#include "layer3.h"

/* this function decodes one layer3 audio frame, except for the header decoding
 * which is done in main() [audio.c]. returns 0 if everything is ok.
 */
int layer3_frame(struct AUDIO_HEADER *header,int cnt)
{
static struct SIDE_INFO info;

int gr,ch,sb,i,tmp;
int mean_frame_size,bitrate,fs,hsize,ssize;

/* we need these later, hsize is the size of header+side_info
*/
	if (header->ID) 
		if (header->mode==3) {
			nch=1;
			hsize=21;
		} else {
			nch=2;
			hsize=36;
		}
	else
		if (header->mode==3) {
			nch=1;
			hsize=13;
		} else {
			nch=2;
			hsize=21;
		}

/* crc increases hsize by 2
*/
	if (header->protection_bit==0) hsize+=2;


/* read layer3 specific side_info
*/
        getinfo(header,&info);


/* MPEG2 only has one granule
*/
	bitrate=t_bitrate[header->ID][3-header->layer][header->bitrate_index];
	fs=t_sampling_frequency[header->ID][header->sampling_frequency];
        if (header->ID) mean_frame_size=144000*bitrate/fs;
        else mean_frame_size=72000*bitrate/fs;


/* check if mdb is too big for the first few frames. this means that
 * a part of the stream could be missing. We must still fill the buffer
 */
	if (info.main_data_begin > append) 
		if (cnt*mean_frame_size < 960) {
			warn(" frame %d discarded, incomplete main_data\n",cnt);
			fillbfr(mean_frame_size + header->padding_bit - hsize);
			return 0;
		}


/* now update the data 'pointer' (counting in bits) according to
 * the main_data_begin information
 */
        data = 8 * ((append - info.main_data_begin) & (BUFFER_SIZE-1));


/* read into the buffer all bytes up to the start of next header
*/
        fillbfr(mean_frame_size + header->padding_bit - hsize);


/* these two should go away
*/
	t_l=&t_b8_l[header->ID][header->sampling_frequency][0];
	t_s=&t_b8_s[header->ID][header->sampling_frequency][0];

/* debug/dump stuff
*/
	show_header(header,bitrate,fs,mean_frame_size,0);
	if (A_DUMP_BINARY) dump((int *)info.part2_3_length);

/* decode the scalefactors and huffman data
 * this part needs to be enhanced for error robustness
 */
	for (gr=0;gr < ((header->ID) ? 2 : 1);gr++) {
		for (ch=0;ch<nch;ch++) {
			show_side_info(&info,gr,ch,0); /* this is for debug/dump */
			ssize=decode_scalefactors(&info,header,gr,ch);
			decode_huffman_data(&info,gr,ch,ssize);
#ifdef DEBUG_HUFFMAN
			{
 			int i;
 			FILE *tf;
	 			tf=fopen("_ah","w");
	 			for (i=0;i<576;i++) fprintf(tf,"%d\n",is[ch][i]);
				fclose(tf);
			}
#endif /* DEBUG_HUFFMAN */
		}
		
	/* requantization, stereo processing, reordering(shortbl)
	*/
		if (header->mode!=1 || (header->mode==1 && header->mode_extension==0))
			for (ch=0;ch<nch;ch++) requantize_mono(gr,ch,&info,header);
		else requantize_ms(gr,&info,header);

	/* antialiasing butterflies
	*/
		for (ch=0;ch<nch;ch++) {
#ifdef DEBUG_REQ
			{
                        int i;
                        FILE *tf;
                        tf=fopen("_ar","w");
                        for (i=0;i<576;i++) fprintf(tf,"%f\n",xr[ch][i]);
                        fclose(tf);
                        }
#endif /* DEBUG_REQ */
			if(!(info.window_switching_flag[gr][ch] && info.block_type[gr][ch]==2))
                                alias_reduction(ch);
		}

	/* just which window?
	*/
		for (ch=0;ch<nch;ch++) {
		int win_type; /* same as in the standard, long=0, start=1 ,.... */

			if (info.window_switching_flag[gr][ch] && info.block_type[gr][ch]==2 && info.mixed_block_flag[gr][ch])
				win_type=0;
			else if (!info.window_switching_flag[gr][ch]) win_type=0;
			else win_type=info.block_type[gr][ch];

		/* imdct ...
		*/
			for (sb=0;sb<2;sb++)
				imdct(win_type,sb,ch);

			if (info.window_switching_flag[gr][ch] && info.block_type[gr][ch]==2 && info.mixed_block_flag[gr][ch])
				win_type=2;

		/* no_of_imdcts tells us how many subbands from the top are all zero
		 * it is set by the requantize functions in misc2.c
		 */
			for (sb=2;sb<no_of_imdcts[ch];sb++)
				imdct(win_type,sb,ch);

			/* clear s[][][] first so we don't totally blow the cache */
			tmp = sb;
			for (;sb<32;sb++) 
				for (i=0;i<18;i++) {
					res[sb][i]=s[ch][sb][i];
					s[ch][sb][i]=0.0f;
				}
	
		/* polyphase filterbank
		*/
			/* if (nch == 2) this was a bug, tomislav */
			        for (i=0;i<18;i++)
				        poly(ch,i);
		}
		printout(); /* this one's really dumb */
	}    /*  for (gr... */ 

	/* return status: 0 for ok, errors will be added
	*/
	return 0;

} 
