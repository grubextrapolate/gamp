#include <math.h>
#include <curses.h>

#include "audio.h"
#include "formats.h"
#include "getbits.h"
#include "huffman.h"
#include "layer3.h"
#include "transform.h"

void draw_func(WINDOW *win, int height, int width) {
    int i,j;
    char star[3] = "**";
    char space[3] = "  ";

    for(i = 0; i < NUM_BANDS; i++) {
        for (j = 0; j < height; j++) {
            if (j < bar_heights[i])
                mvwaddstr(win, height - j, i * 3 + 1, star);
            else
                mvwaddstr(win, height - j, i * 3 + 1, space);
        }
    }
}

void sanalyzer_render_freq(WINDOW *win, int height, int width) {

    int i, c;
    int y;
    float scale;
/*
    int xscale[] = {0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255};
*/
    scale = height / log(578);

    for(i = 0; i < NUM_BANDS; i++) {
        for(c = t_b8_l[1][3][i], y = 0; c < t_b8_l[1][3][i + 1]; c++) {
            if(is[0][c] > y)
                y = is[0][c];
        }
        y >>= 7;
        if(y != 0) {
            y = (int)(log(y) * scale);
            if(y > height - 1)
                y = height - 1;
        }
 
        if(y > bar_heights[i])
            bar_heights[i] = y;
        else if(bar_heights[i] > 4)
            bar_heights[i] -= 4;
        else
            bar_heights[i] = 0;

    }
    draw_func(win, height, width);
}

