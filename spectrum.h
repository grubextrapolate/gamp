void draw_func(WINDOW *win, int height, int width) {
    int i,j;
    char star[3] = "**";
    char space[3] = "  ";

    for(i = 0; i < NUM_BANDS; i++) {
        for (j = 0; j < height; j++) {
            if (j < bar_heights[i])
                mvwaddstr(win, height - j, (i * 4) + 5, star);
            else
                mvwaddstr(win, height - j, (i * 4) + 5, space);
        }
    }
}

void sanalyzer_render_freq(WINDOW *win, int height, int width) {

    int i, c;
    int y = 0;
    float scale;
/*
    int xscale[] = {0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255};
*/
    scale = height / log(578);
    for(i = 0; i < 18; i++) {
        for(c = 0, y = 0; c < 32; c++) {
            if (nch == 2) {
               if(stereo_samples[i][c][0] > y)
                   y = stereo_samples[i][c][0];
            } else {
               if(mono_samples[i][c] > y)
                   y = mono_samples[i][c];
            }
        }
        y >>= 7;
        if(y != 0) {
            y = (int)(log(y) * scale);
            if(y > height - 1)
                y = height - 1;
        }
 
        if(y > bar_heights[i])
            bar_heights[i] = y;
        else if(bar_heights[i] > 1)
            bar_heights[i] -= 1;
        else
            bar_heights[i] = 0;

    }
    draw_func(win, height, width);
}
