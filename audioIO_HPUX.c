/* this file is a part of amp software, (C) tomislav uzelac 1996,1997

	Origional code by: Lutz Vieweg
	Modified by:
	* Andrew Richards - moved code from audio.c

 */

/* Support for Linux and BSD sound devices */

#include "amp.h"
#include <sys/audio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include "audioIO.h"

/* declare these static to effectively isolate the audio device */

static int audio_fd;


/* audioOpen() */
/* should open the audio device, perform any special initialization		 */
/* Set the frequency, no of channels and volume. Volume is only set if */
/* it is not -1 */

void
audioOpen(int frequency, int stereo, int volume)
{
	int failed = 0;
	
	if ((audio_fd = open("/dev/audio",O_RDWR))==-1)
		die(" unable to open the audio device\n");

	if ( ioctl(audio_fd, AUDIO_SET_DATA_FORMAT, AUDIO_FORMAT_LINEAR16BIT) < 0 ||
			 ioctl(audio_fd, AUDIO_SET_CHANNELS, stereo ? 2 : 1) < 0 ||
			 ioctl(audio_fd, AUDIO_SET_OUTPUT, AUDIO_OUT_SPEAKER | AUDIO_OUT_HEADPHONE
						 | AUDIO_OUT_LINE) < 0 ||
			 ioctl(audio_fd, AUDIO_SET_SAMPLE_RATE, frequency) < 0) {
		failed = -1;		
	}
	{
		struct audio_describe description;
		struct audio_gains gains;
		float volume = 1.0;
		if (ioctl(audio_fd, AUDIO_DESCRIBE, &description)) {
			failed = -1;
		}
		if (ioctl (audio_fd, AUDIO_GET_GAINS, &gains)) {
			failed = -1;
		}
		
		gains.transmit_gain = (int)((float)description.min_transmit_gain +
																(float)(description.max_transmit_gain
																				- description.min_transmit_gain)
																* volume);
		if (ioctl (audio_fd, AUDIO_SET_GAINS, &gains)) {
			failed = -1;
		}
	}
	
	if (ioctl(audio_fd, AUDIO_SET_TXBUFSIZE, 4096 * 8)) {
		failed = -1;
	}
	if (failed)
		die(" unable to setup /dev/audio\n");
}
	

/* audioSetVolume - only code this if your system can change the volume while */
/*									playing. sets the output volume 0-100 */

void
audioSetVolume(int volume)
{
	/* DB(audio, msg("Setting volume to: %d\n",volume) ); */
}


/* audioClose() */
/* should close the audio device and perform any special shutdown */

void
audioClose()
{
	close(audio_fd);
	DB(audio, msg("audio: closed %d\n",audio_fd) );
}


/* audioWrite */
/* writes count bytes from buffer to the audio device */
/* returns the number of bytes actually written */

inline int
audioWrite(char *buffer, int count)
{
	DB(audio, msg("audio: Writing %d bytes to audio descriptor %d\n",count,getAudioFd()) );
	return(write(audio_fd,buffer,count));
}


/* Let buffer.c have the audio descriptor so it can select on it. This means	*/
/* that the program is dependent on an file descriptor to work. Should really */
/* move the select's etc (with inlines of course) in here so that this is the */
/* ONLY file which has hardware dependent audio stuff in it										*/

int
getAudioFd()
{
	return(audio_fd);
}
