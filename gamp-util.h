
/*
 * moves the file pointer to where the id3 header should be.
 */
int id3_seek_header (FILE *fp, char *fn) {

    if (fseek(fp, -128, SEEK_END) < 0) {
        fclose(fp);
        die("id3_seek_header: Error reading file %s", fn);
        return FALSE;
    }

    return TRUE;
}

/*
 * reads info from file into id3tag.
 */
int id3_read_file (char *dest, unsigned long size, FILE *fp, char *fn) { 
    if (fread(dest, size, 1, fp) != 1) {
        die("id3_read_file: Error reading %s", fn);
        fclose(fp);
        return FALSE;
    }

    if (ferror(fp) != 0) {
        die("id3_read_file: Error reading %s", fn);
        clearerr(fp);
        fclose(fp);   
        return FALSE;  
    }

    if (feof(fp)) {
        die("id3_read_file: Premature end of file in %s", fn);
        fclose(fp);
        return FALSE;
    }

    return TRUE;
}

/*
 * reads an id3tag from the open file fp. returns TRUE if successful and
 * FALSE otherwise. the id3 tag info is stored in ptrtag.
 */
int read_tag (FILE *fp, char *fn) {  

    if (ptrtag == NULL)
        ptrtag = (ID3_tag *)malloc(sizeof(ID3_tag));

    if (id3_seek_header(fp, fn) == FALSE)
        return FALSE;

    if (!id3_read_file(ptrtag->tag, (sizeof(ptrtag->tag)-1), fp, fn))
        return FALSE;

    if (strcmp(ptrtag->tag, "TAG") != 0)
        return FALSE;

    if (!id3_read_file(ptrtag->songname, (sizeof(ptrtag->songname)-1), fp, fn))
        return FALSE;
    if (!id3_read_file(ptrtag->artist, (sizeof(ptrtag->artist)-1), fp, fn))
        return FALSE;
    if (!id3_read_file(ptrtag->album, (sizeof(ptrtag->album)-1), fp, fn))
        return FALSE;
    if (!id3_read_file(ptrtag->year, (sizeof(ptrtag->year)-1), fp, fn))
        return FALSE;
    if (!id3_read_file(ptrtag->comment, (sizeof(ptrtag->comment)-1), fp, fn))
        return FALSE;

    ptrtag->genre = fgetc(fp);
    
    if (ptrtag->genre == EOF) {
        fclose(fp);
        die("tag_file: Error reading %s", fn);
        return FALSE;
    }

    strtrim(ptrtag->artist);
    strtrim(ptrtag->songname);
    strtrim(ptrtag->album);
    strtrim(ptrtag->year);
    strtrim(ptrtag->comment);

    return TRUE;
}

/*
 * attempts to read an id3 tag from a file. returns a pointer to a string
 * containing the name. if no id3 tag is present the result is simply the
 * name of the file. returns the filename if IGNORE_ID3TAGS is set.
 */
char *getName(char *filename) {

    char *tmp;
    FILE *mp3file;
    int result;

    tmp = (char *)malloc(sizeof(char)*150);

#ifndef IGNORE_ID3TAGS
    mp3file = fopen(filename, "r");

    result = read_tag(mp3file, filename);

    fclose(mp3file);

    if (result == TRUE) {

        if ((strcmp(ptrtag->songname, "") != 0) &&
            (strcmp(ptrtag->artist, "") != 0))
            sprintf(tmp, "%s - %s", ptrtag->artist, ptrtag->songname);
        else
            strncpy(tmp, filename, strlen(filename) - 4);

    } else
        strncpy(tmp, filename, strlen(filename) - 4);

    return tmp;
#else
    strncpy(tmp, filename, strlen(filename) - 4);
    return tmp;
#endif

}

/*
 * finds the total time for the current track based on header info and on
 * file size. this function calls time_per_frame and bits_per_frame.
 */
int get_time(struct AUDIO_HEADER *header) {

    long pos, len;
    double tpf, bpf;
    char tmp[4];

    tpf = time_per_frame(header);
    bpf = bits_per_frame(header);
    pos = ftell(in_file);
    fseek(in_file, 0, SEEK_END);
    len = ftell(in_file) - pos + header->framesize + 4;
    fseek(in_file, -128, SEEK_END);
    fread(tmp, 1, 3, in_file);
    if (!strncmp(tmp, "TAG", 3))
        len -= 128;
    fseek(in_file, pos, SEEK_SET);
    return ((int) ((len / bpf) * tpf));

}

/*
 * calculates frame size (in bits) from header info
 */
int get_frame_size(struct AUDIO_HEADER *header) {

    switch (header->layer) {
        case 1:
            header->framesize = (long) t_bitrate[header->ID][3-header->layer][header->bitrate_index] * 12000;
            header->framesize /= t_sampling_frequency[header->ID][header->sampling_frequency];
            header->framesize = ((header->framesize + header->padding_bit) << 2) - 4;
            break;
        case 2:
            header->framesize = (long) t_bitrate[header->ID][3-header->layer][header->bitrate_index] * 144000;
            header->framesize /= t_sampling_frequency[header->ID][header->sampling_frequency];
            header->framesize += header->padding_bit - 4;
            break;
        case 3:
            header->framesize = (long) t_bitrate[header->ID][3-header->layer][header->bitrate_index] * 144000;
            header->framesize /= t_sampling_frequency[header->ID][header->sampling_frequency] << (header->ID);
            header->framesize = header->framesize + header->padding_bit - 4;
            break;
        default:
            return (0);
    }

    return(1);

}

/*
 * calculates time per frame (in seconds) based on header info.
 */
double time_per_frame(struct AUDIO_HEADER *header) {
    double tpf;

    tpf = (double) bs[header->layer];
    tpf /= t_sampling_frequency[header->ID][header->sampling_frequency] << (header->ID);
    return tpf;

}

/*
 * calculates number of bits per frame from header info
 */
double bits_per_frame(struct AUDIO_HEADER *header) {

    double bpf;

    switch (header->layer) {
        case 1:
            bpf = t_bitrate[header->ID][3-header->layer][header->bitrate_index];
            bpf *= 12000.0 * 4.0;
            bpf /= t_sampling_frequency[header->ID][header->sampling_frequency] << (header->ID);
            break;
        case 3: case 2:
            bpf = t_bitrate[header->ID][3-header->layer][header->bitrate_index];
            bpf *= 144000;
            bpf /= t_sampling_frequency[header->ID][header->sampling_frequency] << (header->ID);
            break;
        default:
            bpf = 1.0;
    }

    return bpf;
}

/*
 * checks if a given filename is an mp3 based on its extension. case
 * insensitive.
 */
int ismp3(char *name) {

    int len = 0;
    len = strlen(name);

    if (len > 3) {
        if ((strcmp(name+len-4, ".mp3") == 0) ||
            (strcmp(name+len-4, ".Mp3") == 0) ||
            (strcmp(name+len-4, ".mP3") == 0) ||
            (strcmp(name+len-4, ".MP3") == 0))
            return 1;
    }
    return 0;
}

/*
 * function to turncate strings to fit in window
 */
void strtrunc(char *str, int length) {

    int len = strlen(str);
    int i;

    sprintf(tmpstr, "%-*s", length, str);

    /* convert long files like:
     * long-filename.ext -> long-fi...ext
     * subject to the length requirements as passed
     */
    if(len > length) {
        tmpstr[length] = 0;
        for(i = 0; i < 3; i++) tmpstr[--length] = tmpstr[--len];
        for(i = 0; i < 3; i++) tmpstr[--length] = '.';
    }
}

/*
 * trims trailing ' ', '\n', or '\t' from the end of a string by replacing
 * these characters by '\0'. used to trim id3 tags and gamp logo
 */
void strtrim(char *string) {

    int i, len;
    len = strlen(string);
    i = len - 1;

    /* if current char is ' ', '\n', or '\t' then replace it with '\0' */
    while (((*(string + i) == ' ') || (*(string + i) == '\n') ||
           (*(string + i) == '\t')) && (i >= 0)) {
        *(string + i) = '\0';
        i--;
    }
}

/*
 * function to read in the gamp logo.
 *
 * FIXME: commented out until i get a .gamprc or something working. dont
 *        want to hard code a directory.
 */
void read_logo() {
/*    FILE *infile;
    int i = 0, width = 0;
    char *pos;

    char logofile[] = "/home/grub/devel/gamp-0.2.0/gamp.logo";
*/
    /* opens input file. dies if there is an error */
/*    if (!(infile = fopen(logofile, "r")))
       die("read_logo: error opening file %s\n", logofile);

    for (i = 0; (i < LOGO_Y) && (feof(infile) == 0); i ++) {
*/        /* reset our buffer */
/*        tmpstring[0] = '\0';
*/
        /* reads in our chunk of the input file */
/*        fgets(logo[i],sizeof(logo[i]),infile);

        logo_height++;

        pos = rindex(logo[i], '*');
        if (pos != NULL) width = pos - logo[i];
        if (width > logo_width) logo_width = width;

        strtrim(logo[i]);

    }

    fclose(infile);
*/

strcpy(logo[0], "  ******     ***** ** *** *** ***   *** *****");
strcpy(logo[1], "***    *** ***    *** ************* ***     ***");
strcpy(logo[2], "***    *** ***    *** ***  ***  *** ***     ***");
strcpy(logo[3], " *********   ***** ** ***  ***  *** *********");
strcpy(logo[4], "       ***                          ***");
strcpy(logo[5], "***    ***                          ***");
strcpy(logo[6], "  ******                            ***");

logo_height = 7;
logo_width = strlen(logo[1]);

}

