#include "amp.h"
#include "getopt.h"
#include <stdio.h>
#include <stdlib.h>
#include "audio.h"


void
displayDisclaimer()
{
	/* print a warm, friendly message
	 */
	msg("\n amp %d.%d.%d, (C) Tomislav Uzelac 1996,1997\n",MAJOR,MINOR,PATCH);
	msg(" THIS PROGRAM COMES WITH ABSOLUTELY NO WARRANTY\n");
	msg(" PLEASE READ THE DOCUMENTATION FOR DETAILS\n\n");
}

void
displayUsage()
{
	int idx;

	/* Losts of printfs for stupid compilers that can't handle literal newlines */
	/* in strings */

	printf("\
usage: gamp [options] [ start directory ]\n\
  -h, -help           display this usage information and exit\n\
  -v, -version        display the version information and exit\n\
  -b, -buffer <size>  set the audio buffer size to <size> k\n\
      -nobuffer       do not use an audio buffer\n\
      -volume <vol>   set the volume to <vol> (0-100)\n\
      -debug <opts..> When compiled in debug <opt, opt2,...> code sections\n\
                      Options: ");

	for(idx=0;debugLookup[idx].name!=0;idx++)
		printf("%s,",debugLookup[idx].name);
	printf("\010 \n");

	exit(0);
}

void
displayVersion()
{
	printf("gamp - %d.%d.%d\n",MAJOR,MINOR,PATCH);
	exit(0);
}



int
argVal(char *name, char *num, int min, int max)
{
	int val;
	
	val=atoi(num);
	if ((val<min) || (val>max))
		die("%s parameter %d - out of range (%d-%d)\n",name,val,min,max);
	return(val);
}


int
args(int argc,char **argv)
{
	int c;

	static int showusage=0, showversion=0;

/* if the number of these variables gets out of hand, convert them to flags
 * variables without A_ prepended don't have an option letter assigned, but
 * you could do so within this func. (they work otherwise)
 */
	SHOW_HEADER=SHOW_HEADER_DETAIL=FALSE;
	SHOW_SIDE_INFO=SHOW_SIDE_INFO_DETAIL=FALSE;
	SHOW_MDB=SHOW_MDB_DETAIL=FALSE;
	SHOW_HUFFMAN_ERRORS=FALSE;
	SHOW_HUFFBITS=FALSE;
	SHOW_SCFSI=SHOW_BLOCK_TYPE=SHOW_TABLES=SHOW_BITRATE=FALSE;
	AUDIO_BUFFER_SIZE=300*1024;

	A_DUMP_BINARY=FALSE;
	A_QUIET=FALSE;
	A_FORMAT_WAVE=FALSE;
	A_SHOW_CNT=FALSE;
	currentVolume=-1;
	A_SHOW_TIME=0;
	A_AUDIO_PLAY=TRUE;
	A_WRITE_TO_FILE=FALSE;
	A_MSG_STDOUT=FALSE;

	while (1) {
		static struct option long_options[] =	{
			{"help", no_argument, 0, 'h'},
			{"debug", required_argument, 0, 1},
			{"version", no_argument, 0, 'v'},
			{"quiet", no_argument, 0, 'q'},
			{"play", no_argument, 0, 'p'},
			{"convert", no_argument, 0, 'c'},
			{"volume", required_argument, 0, 2},
			{"time", no_argument, 0, 't'},
			{"frame", no_argument, 0, 's'},
			{"gui", no_argument, 0, 'g'},
			{"buffer", required_argument, 0, 'b'},
			{"nobuffer", no_argument, &AUDIO_BUFFER_SIZE, 0},
			{"dump", required_argument, 0, 'd'},
			{0, 0, 0, 0}
		};

		c = getopt_long_only(argc, argv, "qgstwdpcb:vh", long_options,0);

		if (c == -1) break;
		switch (c) {
		case	1 : debugSetup(optarg); break;
		case	2 : currentVolume=argVal("Volume",optarg,0,100); break;
		case 'q': A_QUIET=TRUE; break;
		case 's': A_SHOW_CNT=TRUE; break;
		case 't': A_SHOW_TIME=TRUE; break;
		case 'w': A_FORMAT_WAVE=TRUE; break;
		case 'd': A_DUMP_BINARY=TRUE; break;
		case 'g': A_MSG_STDOUT=TRUE; break;
		case 'p': A_AUDIO_PLAY=TRUE;A_WRITE_TO_FILE=FALSE;break;
		case 'c': A_AUDIO_PLAY=FALSE;A_WRITE_TO_FILE=TRUE;break;
		case 'b': AUDIO_BUFFER_SIZE=1024*argVal("Buffer size",optarg,64,10000); break;
		case 'v': showversion=1; break;
		case 'h': showusage=1; break;
		case '?': exit(1);
		case ':': printf("Missing parameter for option -%c\n",c); break;
		}
	}
	if (showversion) displayVersion();
	if (showusage) displayUsage();

	return(optind);
}

