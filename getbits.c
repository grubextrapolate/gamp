/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* getbits.c  bit level routines, input buffer
 * 
 * Created by: tomislav uzelac  Apr 1996 
 * Last modified by:
 */
#include "amp.h"
#include "audio.h"
#include "formats.h"
#include <assert.h> /* nov27/2000 Victor Zandy <zandy@cs.wisc.edu> */

#define	GETBITS
#include "getbits.h"

static inline void _fillbfr(size)
int size;
{
	fread( _buffer , 1, size, in_file);
	_bptr=0;
}

inline unsigned int _getbits(n)
int n;
{
unsigned int pos,ret_value;

        pos = _bptr >> 3;
	ret_value = _buffer[pos] << 24 |
		    _buffer[pos+1] << 16 |
		    _buffer[pos+2] << 8 |
		    _buffer[pos+3];
        ret_value <<= _bptr & 7;
        ret_value >>= 32 - n;
        _bptr += n;
        return ret_value;
}       

void fillbfr(advance)
int advance;
{
int overflow;

	fread( &buffer[append], 1, advance, in_file);
	
	if ( append + advance >= BUFFER_SIZE ) {
		overflow = append + advance - BUFFER_SIZE;
		memcpy (buffer,&buffer[BUFFER_SIZE], overflow);
		if (overflow < 4) memcpy(&buffer[BUFFER_SIZE],buffer,4);
		append = overflow;
	} else append+=advance;
	return;
}

/* these functions read from the buffer. a separate viewbits/flushbits
 * functions are there primarily for the new huffman decoding scheme
 */
inline unsigned int viewbits(int n)
{
unsigned int pos,ret_value;

        pos = data >> 3;
	ret_value = buffer[pos] << 24 |
		    buffer[pos+1] << 16 |
		    buffer[pos+2] << 8 |
		    buffer[pos+3];
        ret_value <<= data & 7;
        ret_value >>= 32 - n;

        return ret_value;
}       

inline void sackbits(int n)
{
        data += n;
        data &= 8*BUFFER_SIZE-1;
}

unsigned int getbits(n)
int n;
{
        if (n) {
        unsigned int ret_value;
                ret_value=viewbits(n);
                sackbits(n);
                return ret_value;
        } else
                return 0;
}

int gethdr(struct AUDIO_HEADER *header)
{
int s;
	_fillbfr(4);
	if ((s=_getbits(12)) != 0xfff) {
		if (s==0xffe) return GETHDR_NS;
		else return GETHDR_ERR;
	}
	header->ID=_getbits(1);
	header->layer=_getbits(2);
	header->protection_bit=_getbits(1);
	header->bitrate_index=_getbits(4);
	header->sampling_frequency=_getbits(2);
	header->padding_bit=_getbits(1);
	header->private_bit=_getbits(1);
	header->mode=_getbits(2);
	header->mode_extension=_getbits(2);
	if (!header->mode) header->mode_extension=0; /* ziher je.. */
	header->copyright=_getbits(1);
	header->original=_getbits(1);
	header->emphasis=_getbits(2);
	return 0;
}

/* dummy function, to get crc out of the way
*/
void getcrc()
{
	_fillbfr(2);
	_getbits(16);
}

/* sizes of side_info:
 * MPEG1   1ch 17    2ch 32
 * MPEG2   1ch  9    2ch 17
 */
void getinfo(struct AUDIO_HEADER *header,struct SIDE_INFO *info)
{
int gr,ch,scfsi_band,region,window;
int nch;	
	if (header->mode==3) {
		nch=1;
		if (header->ID) {
			_fillbfr(17);
			info->main_data_begin=_getbits(9);
			_getbits(5);
		} else {
			_fillbfr(9);
			info->main_data_begin=_getbits(8);
			_getbits(1);
		}
	} else {
		nch=2;
                if (header->ID) {
			_fillbfr(32);
                        info->main_data_begin=_getbits(9);
                        _getbits(3);
                } else {
			_fillbfr(17);
                        info->main_data_begin=_getbits(8);
                        _getbits(2);
                }
	}

	if (header->ID) for (ch=0;ch<nch;ch++)
		for (scfsi_band=0;scfsi_band<4;scfsi_band++)
			info->scfsi[ch][scfsi_band]=_getbits(1);

	for (gr=0;gr<(header->ID ? 2:1);gr++)
		for (ch=0;ch<nch;ch++) {
			info->part2_3_length[gr][ch]=_getbits(12);
			info->big_values[gr][ch]=_getbits(9);
                        /* nov27/2000 Victor Zandy <zandy@cs.wisc.edu> */
                        if ((info->big_values[gr][ch] << 1) >= IS_SIZE) {
                                fprintf(stderr, "is array overflow\n");
                                assert(0);
                        }
			info->global_gain[gr][ch]=_getbits(8);
			if (header->ID) info->scalefac_compress[gr][ch]=_getbits(4);
			else info->scalefac_compress[gr][ch]=_getbits(9);
			info->window_switching_flag[gr][ch]=_getbits(1);

			if (info->window_switching_flag[gr][ch]) {
				info->block_type[gr][ch]=_getbits(2);
				info->mixed_block_flag[gr][ch]=_getbits(1);

				for (region=0;region<2;region++)
					info->table_select[gr][ch][region]=_getbits(5);
				info->table_select[gr][ch][2]=0;

				for (window=0;window<3;window++)
					info->subblock_gain[gr][ch][window]=_getbits(3);
			} else {
				for (region=0;region<3;region++)
					info->table_select[gr][ch][region]=_getbits(5);

				info->region0_count[gr][ch]=_getbits(4);
				info->region1_count[gr][ch]=_getbits(3);
				info->block_type[gr][ch]=0;
			}

			if (header->ID) info->preflag[gr][ch]=_getbits(1);
			info->scalefac_scale[gr][ch]=_getbits(1);
			info->count1table_select[gr][ch]=_getbits(1);
		}
	return;
}

