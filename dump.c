/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* dump.c  binary/hex dump from buffer
 * 
 * Created by: tomislav uzelac  May 1996
 * Last modified by: 
 */
#include <unistd.h>

#include "audio.h"
#include "getbits.h"

#define DUMP
#include "dump.h"

/* no hex dump, sorry
 */
void dump(int *length)   /* in fact int length[4] */
{
int i,j;
int _data,space=0;
	printf(" *********** binary dump\n");
	_data=data;
	for (i=0;i<4;i++) {
		for (j=0;j<space;j++) printf(" ");
		for (j=0;j<length[i];j++) {
			printf("%1d",(buffer[_data/8] >> (7-(_data&7)) )&1 );
			space++;
			_data++;
			_data&=8*BUFFER_SIZE-1;
			if (!(_data & 7)) {
				printf(" ");
				space++;
				if (space>70) {
					printf("\n");
					space=0;
				}
			}
		}
		printf("~\n");
	}
}

inline void show_header(header,bitrate,fs,mean_frame_size,cnt)
struct AUDIO_HEADER *header;
int bitrate,fs,mean_frame_size,cnt;
{
	if (SHOW_BITRATE) printf(" bitrate=%d\n",bitrate);
	
	if (SHOW_HEADER) {
		printf(" bitrate=%6d         fs=%5d    \n",bitrate,fs);
		printf(" frame_size=%d\n",mean_frame_size+header->padding_bit);
		printf(" layer%d %s\n",4-header->layer,t_modes[header->mode]);      
		if (SHOW_HEADER_DETAIL) {
			printf(" ID%d   protection%d   padding%d\n",header->ID\
				,header->protection_bit,header->padding_bit);
			printf(" mode_ext=%d    emphasis=%d\n",\
				header->mode_extension,header->emphasis);
			printf(" private%d   copy%d   orig%d\n",\
				header->private_bit,header->copyright,\
				header->original);
		}
	}
}

inline void show_side_info(info,gr,ch,mdb)
struct SIDE_INFO *info;
int gr,ch,mdb;
{
int c,i;
	if ((SHOW_MDB || SHOW_MDB_DETAIL) && gr==0 && ch==0) {
		printf(" main_data_begin=%d\n",info->main_data_begin);
		if (SHOW_MDB_DETAIL) 
			printf(" mdb %d of %d\n",mdb,BUFFER_SIZE);
	}
	if (SHOW_SCFSI)
		if (gr==0 && ch==0) {
			for (c=0;c<nch;c++) {
				printf(" scfsi ch%d ",c);
				for (i=0;i<4;i++)
					printf(" %d ",info->scfsi[c][i]);
			printf("\n");
			}
		}
	if (SHOW_SIDE_INFO) {
		printf(" 2_3_len=%d\n",info->part2_3_length[gr][ch]);
		printf(" big_values=%d\n",info->big_values[gr][ch]);
		if (SHOW_SIDE_INFO_DETAIL) 
			printf(" global_gain=%d  scalefac_compress=%d\n",\
				info->global_gain[gr][ch],\
				info->scalefac_compress[gr][ch]);
	}
	if (SHOW_BLOCK_TYPE)
		if (info->window_switching_flag[gr][ch]) {
			if (info->block_type[gr][ch]==2) 
				if (info->mixed_block_flag[gr][ch])
					printf(" MIXED BLOCK\n");
				else 
					printf(" SHORT BLOCK\n");
			else if (info->block_type[gr][ch]==1)
					printf(" START BLOCK\n");
				else
					printf(" END BLOCK\n");	

		} else printf(" LONG BLOCK\n");
	if (SHOW_TABLES)
			printf(" TABLES: %d %d %d  %d(quad)\n",\
				 info->table_select[gr][ch][0],\
				 info->table_select[gr][ch][1],\
				 info->table_select[gr][ch][2],\
				 info->count1table_select[gr][ch]);
	if (SHOW_SIDE_INFO_DETAIL) {
		printf(" subblock gains: ");
		for (i=0;i<3;i++) printf(" %d",\
			info->subblock_gain[gr][ch][i]);
		printf("\n");
	}
	if (SHOW_SIDE_INFO) printf(" region0=%d  region1=%d\n",\
				info->region0_count[gr][ch],\
				info->region1_count[gr][ch]);
	if (SHOW_SIDE_INFO_DETAIL) 
			printf(" preflag=%d scf_scale=%d\n",\
				info->preflag[gr][ch],\
				info->scalefac_scale[gr][ch]);
}
