
int inline
processHeader(struct AUDIO_HEADER *header, int cnt)
{
	int g;

	/* getbits funcs expect first four bytes after BUFFER_SIZE have same contents
	 * as the first four bytes of the buffer. fillbfr() normally takes care of
	 * this when it hits the end of the buffer, but not this first time
	 */
	if (cnt==1) memcpy(&buffer[BUFFER_SIZE],buffer,4);	/* XXX WHAT!!! */
	
	if ((g=gethdr(header))!=0) {
		if (g != GETHDR_ERR) {
			warn(" this is a file in MPEG 2.5 format, which is not defined\n");
			warn(" by ISO/MPEG. It is \"a special Fraunhofer format\".\n");
			warn(" amp does not support this format. sorry.\n");
		} else {
			/* here we should check whether it was an EOF or a sync err. later.
			 */	
			if (A_FORMAT_WAVE) wav_end(header);
			
			/* this is the message we'll print once we check for sync errs
			 * warn(" amp encountered a synchronization error\n");
			 * warn(" sorry.\n");
			 */
		}
		return(1);
	}

	/* crc is read just to get out of the way.
	 */
	if (header->protection_bit==0) getcrc();

	return(0);
}

void
statusDisplay(struct AUDIO_HEADER *header, int frameNo)
{
	int minutes,seconds;

	if ((A_SHOW_CNT || A_SHOW_TIME) && !(frameNo%10))
		msg("\r");
	if (A_SHOW_CNT && !(frameNo%10) ) {
		msg("	 {%d} ",frameNo);
	}
	if (A_SHOW_TIME && !(frameNo%10)) {
		seconds=frameNo*1152/t_sampling_frequency[header->ID][header->sampling_frequency];
		minutes=seconds/60;
		seconds=seconds % 60;
		msg("	 %2d:%02d ",minutes,seconds);
	}
	if (A_SHOW_CNT || A_SHOW_TIME)
		fflush(stderr);
}


int
decodeMPEG(char *inFileStr, char *outFileStr)
{
	struct AUDIO_HEADER header;
	int cnt,err=0;
	
	if (strcmp(inFileStr,"-")==0)
		in_file=stdin;
	else {
		if ((in_file=fopen(inFileStr,"r"))==NULL) {
			warn("Could not open file: %s\n",inFileStr);
			return(1);
		}
	}

	if (outFileStr) {
		if (strcmp(outFileStr,"-")==0)
			out_file=stdout;
	else
		if ((out_file=fopen(outFileStr,"w"))==NULL) {
			warn("Could not write to file: %s\n",outFileStr);
			return(1);
		}
		msg("Converting: %s\n",inFileStr);
	}

	if (A_AUDIO_PLAY)
		msg("Playing: %s\n",inFileStr);

	append=data=nch=0;																	/* initialize globals */

	if (A_FORMAT_WAVE) wav_begin();

	for (cnt=0;;cnt++) {																				/* _the_ loop */
		if (processHeader(&header,cnt))
			break;

		statusDisplay(&header,cnt);	
		
		if (!cnt && A_AUDIO_PLAY) { /* setup the audio when we have the frame info */
																			 /* audioOpen(frequency, stereo, volume) */
			if (AUDIO_BUFFER_SIZE==0)
				audioOpen(t_sampling_frequency[header.ID][header.sampling_frequency],
									(header.mode!=3),
									A_SET_VOLUME);
			else
				audioBufferOpen(t_sampling_frequency[header.ID][header.sampling_frequency],
									(header.mode!=3),
									A_SET_VOLUME);
		}

		if (layer3_frame(&header,cnt)) {
			warn(" error. blip.\n");
			err=1;
			break;
		} 
	}
	fclose(in_file);
	if (A_AUDIO_PLAY)
		if (AUDIO_BUFFER_SIZE!=0)
			audioBufferClose();
		else
			audioClose();
	else
		fclose(out_file);

	msg("\n");
	return(err);
}
