typedef struct __audioinfo {   
   int bitrate;   /* Bitrate */  
   int frequency; /* Frequency */
   int stereo;    /* 2 = Stereo / 1 = Mono */
   int type;      /* Layer1, Layer2, Layer3, Wav .. */
   int sample;    /* Sample size. */

   int mode;
   int crc;
   int copyright;
   int original;
   char *ident;
} AudioInfo;
