#include <math.h>

#define NUM_BANDS 16

void show_spectrum(WINDOW *win, struct AUDIO_HEADER *header) {
    char dirfilecat[150];
    struct SIDE_INFO info;

    getinfo(header,info)


    mvwaddstr(win, 1, 1, tmpstr);
}

static void sanalyzer_render_freq(struct SIDE_INFO *data) {

    int i,c;
    int y;

    int xscale[] = {0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255};

    for(i = 0; i < NUM_BANDS; i++) {
        for(c = xscale[i], y = 0; c < xscale[i + 1]; c++) {
            if(data[0][c] > y)
                y = data[0][c];
        }
        y >>= 7;
        if(y != 0) {
            y = (gint)(log(y) * scale);
            if(y > HEIGHT - 1)
                y = HEIGHT - 1;
        }
 
        if(y > bar_heights[i])
            bar_heights[i] = y;
        else if(bar_heights[i] > 4)
            bar_heights[i] -= 4;
        else
            bar_heights[i] = 0;

    }
    draw_func(NULL);
    return;			
}

static int draw_func(gpointer data) {
    gint i;

    for(i = 0; i < NUM_BANDS; i++) {
        /*if(bar_heights[i] > 4)
            bar_heights[i] -= 4;
        else
            bar_heights[i] = 0;*/
        gdk_draw_pixmap(draw_pixmap,gc,bar, 0,HEIGHT - 1 - bar_heights[i], i * (WIDTH / NUM_BANDS), HEIGHT - 1 - bar_heights[i], (WIDTH / NUM_BANDS) - 1, bar_heights[i]);

    }

    return TRUE;
}
