/* this file is a part of amp software

   buffer.c: written by Andrew Richards  <A.Richards@phys.canterbury.ac.nz>
             (except printout())

   Last modified by:
*/

#include "amp.h"
#include "transform.h"

#include <sys/types.h>
#ifdef HAVE_MLOCK
#ifdef OS_Linux
#include <sys/mman.h>
#endif
#endif
#ifdef HAVE_SYS_SELECT
#include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audioIO.h"
#include "audio.h"

struct ringBuffer {					/* A ring buffer to store the data in					 */
	char *bufferPtr;					/* buffer pointer															 */
	int inPos, outPos;				/* positions for reading and writing					 */
	int dataFinished;
};


static buffer_fd;

/* little endian systems do not require any byte swapping whatsoever. 
 * big endian systems require byte swapping when writing .wav files, 
 * but not when playing directly
 */


void printout(void)
{
if (A_WRITE_TO_FILE) {
#ifndef NO_BYTE_SWAPPING
int i,j;
short *ptr;

        if (nch==2) {
                ptr=(short*)stereo_samples;
                i=j=32 * 18 * 2;
        } else {
                ptr=(short*)mono_samples;
                i=j=32 * 18;
        }

        for (;i>=0;--i)
                ptr[i] = ptr[i] << 8 | ptr[i] >> 8;
#endif

        if (nch==2)
                fwrite(stereo_samples,1,sizeof stereo_samples,out_file);
        else
                fwrite(mono_samples,1,sizeof mono_samples,out_file);
}


        if (A_AUDIO_PLAY) {
		if (AUDIO_BUFFER_SIZE==0)
			if (nch==2) audioWrite((char*)stereo_samples,2*32*18*2);
			else audioWrite((char*)mono_samples,2*32*18);
		else
			if (nch==2) audioBufferWrite((char*)stereo_samples,2*32*18*2);
			else audioBufferWrite((char*)mono_samples,2*32*18);
        }

}

int AUDIO_BUFFER_SIZE;

#define bufferSize(A) (((A)->inPos+AUDIO_BUFFER_SIZE-(A)->outPos) % AUDIO_BUFFER_SIZE)
#define bufferFree(A) (AUDIO_BUFFER_SIZE-1-bufferSize(A))

void 
initBuffer(struct ringBuffer *buffer)
{
	buffer->bufferPtr=malloc(AUDIO_BUFFER_SIZE);
	if (buffer->bufferPtr==NULL) {
		die("Unable to allocate write buffer\n");
	}
#ifdef HAVE_MLOCK
	mlock(buffer->bufferPtr,AUDIO_BUFFER_SIZE);
#endif
	buffer->inPos = 0;
	buffer->outPos = 0;
	DB(buffer2, msg("Allocated %d bytes for audio buffer\n",AUDIO_BUFFER_SIZE) );
}

void
freeBuffer(struct ringBuffer *buffer)
{
#ifdef HAVE_MLOCK
  munlock(buffer->bufferPtr,AUDIO_BUFFER_SIZE);
#endif
	free(buffer->bufferPtr);
}

void
bufferGet(struct ringBuffer *buffer, char *miniBuffer, int count)
{
	int bytesToEnd;
	
	bytesToEnd=AUDIO_BUFFER_SIZE-buffer->outPos;
	memcpy(miniBuffer,buffer->bufferPtr+buffer->outPos,MIN(bytesToEnd,count));
	if (count>bytesToEnd) { /* handle wrap around the end of the buffer */
		memcpy(miniBuffer+bytesToEnd,buffer->bufferPtr,count-bytesToEnd);
	}
	buffer->outPos=(buffer->outPos+count) % AUDIO_BUFFER_SIZE;
	
}

/* bufferPut - assumes there is enough space to add data */
void
bufferPut(struct ringBuffer *buffer, char *miniBuffer, int count)
{
	int bytesToEnd;
	
	DB(buffer2, msg("putting %d bytes in to buffer\n",count) );
	
	bytesToEnd=AUDIO_BUFFER_SIZE-buffer->inPos;
	DB(buffer2, msg("bytesToEnd=%d\n",bytesToEnd) );
	memcpy(buffer->bufferPtr+buffer->inPos,miniBuffer,MIN(bytesToEnd,count));
	if (count>bytesToEnd) { /* handle wrap around the end of the buffer */
		memcpy(buffer->bufferPtr,miniBuffer+bytesToEnd,count-bytesToEnd);
	}
	buffer->inPos=(buffer->inPos+count) % AUDIO_BUFFER_SIZE;
}


/* The decoded data are stored in a the ring buffer until they can be sent */
/* to the audio device. Variables are named in relation to the buffer			 */
/* ie writes are writes from the codec to the buffer and reads are reads	 */
/* from the buffer to the audio device																		 */

int
audioBufferOpen(int frequency, int stereo, int volume)
{
	struct ringBuffer audioBuffer;
	char miniBuffer[AUSIZ];
	
	int inFd,outFd,cnt,cntr=0,pid;
	int inputFinished=FALSE;
	int percentFull;
	fd_set inFdSet,outFdSet;
	fd_set *inFdPtr,*outFdPtr; 
	struct timeval timeout;
	int filedes[2];
	
	DB(buffer, msg("Starting audio buffer\n") );
	
	pipe(filedes);
	DB(buffer, msg("readpipe=%d writepipe=%d\n",filedes[0],filedes[1]) );
	if ((pid=fork())!=0) {  /* if we are the parent */
		close(filedes[0]);
		buffer_fd=filedes[1];
		return(pid);	        /* return the pid */
	}
	
	DB(buffer, msg("Audio buffer (pipe) starting\n") );
	
	/* we are the child */
	close(filedes[1]);
	inFd=filedes[0];
	audioOpen(frequency,stereo,volume);
	outFd=getAudioFd();

	initBuffer(&audioBuffer);
	
	while(1) {
		timeout.tv_sec=0;
		timeout.tv_usec=0;
		FD_ZERO(&inFdSet);
		FD_ZERO(&outFdSet);
		FD_SET(inFd,&inFdSet);
		FD_SET(outFd,&outFdSet);
		
		DB(buffer,
			 msg("BS=%d inPos=%d outPos=%d bufferSize=%d bufferFree=%d\n",
					 AUDIO_BUFFER_SIZE,audioBuffer.inPos,audioBuffer.outPos,
					 bufferSize(&audioBuffer),bufferFree(&audioBuffer)) );
		
		if (bufferSize(&audioBuffer)<AUSIZ) {					/* is the buffer too empty */
			outFdPtr=NULL;															/* yes, don't try to write */
			if (inputFinished)					/* no more input, buffer exhausted -> exit */
				break;
		} else
			outFdPtr=&outFdSet;															/* no, select on write */
		
		/* check we have at least 1024 bytes left (don't want <1k bits) */
		if ((bufferFree(&audioBuffer)>=AUSIZ) && !inputFinished)
			inFdPtr=&inFdSet;																 /* no, select on read */
		else
			inFdPtr=NULL; /* buffer full or no more data?, yes, don't want to read */
		
/* The following selects() are basically all that is left of the system
   dependent code outside the audioIO_*.c files. These selects really
   need to be moved into the audioIO_*.c files and replaced with a
   function like audioIOReady(inFd, &checkIn, &checkAudio, wait) where
   it checks the status of the input or audio output if checkIn or
   checkAudio are set and returns with checkIn or checkAudio set to TRUE
   or FALSE depending on whether or not data is available. If wait is
   FALSE the function should return immediately, if wait is TRUE the
   process should BLOCK until the required condition is met. NB: The
   process MUST relinquish the CPU during this check or it will gobble
   up all the available CPU which sort of defeats the purpose of the
   buffer.

   This is tricky for people who don't have file descriptors (and
   select) to do the job. In that case a buffer implemented using
   threads should work. The way things are set up now a threaded version
   shouldn't be to hard to implement. When I get some time... */

		/* check if we need to write first */
		if (!select(outFd+1,NULL,outFdPtr,NULL,&timeout)) {
			FD_ZERO(&outFdSet);
			FD_SET(outFd,&outFdSet);														/* wait for R or W */
			select(MAX(inFd,outFd)+1,inFdPtr,outFdPtr,NULL,NULL);
		}
		else
			inFdPtr=NULL;
		
		if (outFdPtr && FD_ISSET(outFd,outFdPtr)) {							/* need to write */
			percentFull=100*bufferSize(&audioBuffer)/AUDIO_BUFFER_SIZE;
			if ((cntr++ % (16/(AUSIZ/4096)))==0) msg("\rBuffer (%2d%%) %6d",percentFull,bufferSize(&audioBuffer));
			bufferGet(&audioBuffer,miniBuffer,AUSIZ);
			cnt=audioWrite(miniBuffer,AUSIZ);
			DB(buffer2, msg("buffer: wrote %d bytes (to audio)\n",cnt) );
		}
		if (inFdPtr && FD_ISSET(inFd,inFdPtr)) {								 /* need to read */
			cnt=read(inFd,miniBuffer,AUSIZ);
			DB(buffer2, msg("buffer: read %d bytes\n",cnt) );
			bufferPut(&audioBuffer,miniBuffer,cnt);
			if (cnt==0) {
				inputFinished=TRUE;
				DB(buffer, msg("inputFinished\n") );
			}
		}
		
	}
	close(inFd);
	DB(buffer, msg("Buffer pipe closed %d\n",inFd); )
	audioClose();
	exit(0);
}

/* This is a separate (but inlined) function to make it easier to implement */
/* a threaded buffer */

inline void
audioBufferWrite(char *buf,int bytes)
{
	write(buffer_fd, buf, bytes);
}


void
audioBufferClose()
{
	/* Close the buffer pipe and wait for the buffer process to close */
	close(buffer_fd);
	waitpid(0,0,0);
}
