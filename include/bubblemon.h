/*  WMBubble dockapp 2.0 - Wayland version
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _BUBBLEMON_H_
#define _BUBBLEMON_H_

#include <stdint.h>
#include <sys/types.h>

#ifndef BOX_SIZE
#define BOX_SIZE 58
#endif

#define CPUMINBLEND 80
#define CPUMAXBLEND 190
#define GRAPHMINBLEND 40
#define GRAPHMAXBLEND 256

#define DUCKBLEND 100

#define MULTIPLIER 4096.0
#define POWER2 12
#define REALY(y) ((y) >> POWER2)
#define MAKEY(y) ((y) << POWER2)
#define MAKE_INTEGER(x) ((int)((x)*MULTIPLIER+0.5))

#ifndef u_int64_t
typedef uint64_t u_int64_t;
#endif

typedef struct {
    int x;
    int y;
    int dy;
} Bubble;

typedef struct {
    int i;
    int f;
} LoadAvg;

typedef struct {
    unsigned char rgb_buf[BOX_SIZE * BOX_SIZE * 3 + 1];

    unsigned char mem_buf[BOX_SIZE * BOX_SIZE * 3 + 1];

    int screen_type;
    int picture_lock;

    int samples;
    unsigned char bubblebuf[BOX_SIZE * (BOX_SIZE+4)];

    int waterlevels[BOX_SIZE];
    int waterlevels_dy[BOX_SIZE];
    Bubble *bubbles;
    int n_bubbles;

    int air_noswap, liquid_noswap, air_maxswap, liquid_maxswap;

    int loadIndex;
    u_int64_t *load, *total;

    int maxbubbles;
    double ripples;
    double gravity;
    double volatility;
    double viscosity;
    double speed_limit;

    int ripples_int;
    int gravity_int;
    int volatility_int;
    int viscosity_int;
    int speed_limit_int;

    u_int64_t mem_used;
    u_int64_t mem_max;
    u_int64_t swap_used;
    u_int64_t swap_max;
    unsigned int swap_percent;
    unsigned int mem_percent;
    unsigned int memhist[BOX_SIZE-3];
    unsigned int memadd;

    LoadAvg loadavg[3];
    unsigned int history[BOX_SIZE-3];
    unsigned int hisadd;
} BubbleMonData;
#endif
