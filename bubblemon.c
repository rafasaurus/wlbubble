/*  WMBubble dockapp 2.0 - Wayland version
 *
 *  - dockapp for Wayland compositors
 *  - Code from Robert Jacobs <rjacobs@eamp.org>, 2010-2011
 *  - Code from the debian maintainers, 2005-2009
 *  - Code outside of bubblemon_update copyright 2000, 2001
 *    timecop@japan.co.jp
 *  - oleg dashevskii <od@iclub.nsu.ru> made changes to collect memory
 *    and cpu information on FreeBSD.  Some major performance improvements
 *    and other cool hacks.  Useful ideas - memscreen, load screen, etc.
 *  - Adrian B <midnight@bigpond.net.au> came up with the idea of load
 *    meter.
 *  - tarzeau@space.ch sent in cute duck gfx and suggestions, plus some
 *    code and duck motion fixes.
 *  - Phil Lu <wplu13@netscape.net> Dan Price <dp@rampant.org> - Solaris/SunOS
 *    port
 *  - Everything else copyright one of the guys below
 *  Bubbling Load Monitoring Applet
 *  - A GNOME panel applet that displays the CPU + memory load as a
 *    bubbling liquid.
 *  Copyright (C) 1999-2000 Johan Walles
 *  - d92-jwa@nada.kth.se
 *  Copyright (C) 1999 Merlin Hughes
 *  - merlin@merlin.org
 *  - http://nitric.com/freeware/
 *
 *  Wayland port 2025
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
#define _GNU_SOURCE

#define VERSION "2.0"

#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <math.h>
#include <inttypes.h>
#include <getopt.h>
#include <poll.h>

#include "wayland_surface.h"

#include "include/bubblemon.h"
#include "include/sys_include.h"

#include "include/numbers-2.h"

#include "include/ducks.h"
#include "include/digits.h"
#include "misc/numbers.xpm"
#include "misc/ofmspct.xpm"
#include "misc/datefont.xpm"

#define NAME "wmbubble"

#define GET_RED(x) (((x)>>16)&255)
#define GET_GRN(x) (((x)>> 8)&255)
#define GET_BLU(x) (((x)    )&255)

enum bubblebuf_values { watercolor, antialiascolor, aircolor };

void bubblemon_allocate_buffers(void);
void do_water_sim(int cpu);
void draw_watertank(void);
void bubblebuf_colorspace(void);
void build_graphs(void);

void make_new_bubblemon_dockapp(void);
void get_memory_load_percentage(void);
void parse_arguments(int argc, char **argv);
void draw_from_xpm(char **xpm, unsigned char *whither, unsigned int targetw,
                   unsigned int xpmx, unsigned int xpmy, unsigned int xpmw,
                   unsigned int xpmh, unsigned int color);
void draw_history(int num, int size, unsigned int *history, uint32_t *buf);
void draw_digit(unsigned char *from, uint32_t *whither);
void draw_string(char *string, int x, int y, int color);
void draw_cpudigit(int what, unsigned char *whither);
void draw_cpugauge(int cpu);
void draw_rgba_pixel(uint32_t *whither, int color, float opacity);
void draw_aa_line(float x1,float y1, float x2,float y2, int color);
void draw_clockhands(void);

void render_secondary(void);
void calculate_transparencies(int proximity);
void alpha_cpu(void);
void alpha_graph(void);
void alpha_digitalclock(struct tm * mytime);
void alpha_date(struct tm * mytime);
void roll_history(void);

void draw_dtchr(const char letter, uint32_t *where);
void draw_largedigit(char number, uint32_t *rgbbuf);

int animate_correctly(void);
void draw_duck(int x, int y, int frame_no, int flipx, int flipy);
void duck_swimmer(void);

BubbleMonData bm;

int duck_enabled = 1;
int upside_down_duck_enabled = 1;
int cpu_enabled = 1;
int memscreen_enabled = 1;
int memscreen_megabytes = 0;
int graph_digit_color = 0x308cf0;
int graph_warning_digit_color = 0xed1717;
int pale = 0;

int do_analog_clock = 0;
int hourcolor = 0xEEEEEE;
int mincolor = 0xBF0000;
int seccolor = 0xC79F2B;
int shifttime = 0;
int do_digital_clock = 0;
int do_date = 0;

int do_help = 0;

int delay_time = 25000;

int gauge_alpha = CPUMAXBLEND;
int graph_alpha = GRAPHMAXBLEND;

int duck_colors[4] = {0,0xF8FC00,0xF8B040,0};
int graph_labels = 0xC1C400;
int graph_field = 0x202020;
int graph_grid = 0x062A00;

int graph_max = 0x20B6AE;
int graph_bar = 0x007D71;
int graph_hundreds = 0x71E371;

unsigned char * empty_loadgraph, * empty_memgraph;
unsigned char * graph_numbers_n_rgb, * graph_numbers_b_rgb;
unsigned char cpu_gauge[25*9*3];

int datefont_widths[256];
char datefont_transparent;
unsigned int datefont_offset;

int duck_blink = 0;
int blinkdelay = 1;

static struct option long_options[] = {
    {"maxbubbles",    required_argument, 0, 'b'},
    {"air-noswap",    required_argument, 0, 'A'},
    {"air-maxswap",   required_argument, 0, 'a'},
    {"liquid-noswap", required_argument, 0, 'L'},
    {"liquid-maxswap",required_argument, 0, 'l'},
    {"duckbody",      required_argument, 0, 'B'},
    {"duckbill",      required_argument, 0, 'i'},
    {"duckeye",       required_argument, 0, 'e'},
    {"delay",         required_argument, 0, 'D'},
    {"ripples",       required_argument, 0, 'r'},
    {"gravity",       required_argument, 0, 'g'},
    {"volatility",    required_argument, 0, 'V'},
    {"viscosity",     required_argument, 0, 'v'},
    {"speed-limit",   required_argument, 0, 'S'},
    {"help",          no_argument,       0, 'h'},
    {"duck",          required_argument, 0, 'd'},
    {"upsidedown",    required_argument, 0, 'u'},
    {"cpumeter",      required_argument, 0, 'c'},
    {"graphdigit",    required_argument, 0, 'G'},
    {"graphwarn",     required_argument, 0, 'w'},
    {"graphlabel",    required_argument, 0, 'E'},
    {"graphfield",    required_argument, 0, 'F'},
    {"graphgrid",     required_argument, 0, 'I'},
    {"graphmax",      required_argument, 0, 'M'},
    {"graphbar",      required_argument, 0, 'R'},
    {"graphmarkers",  required_argument, 0, 'K'},
    {"graphs",        required_argument, 0, 'm'},
    {"units",         required_argument, 0, 'U'},
    {"shifttime",     required_argument, 0, 's'},
    {"digital",       required_argument, 0, 'T'},
    {"showdate",      required_argument, 0, 'O'},
    {"analog",        required_argument, 0, 'N'},
    {"hourcolor",     required_argument, 0, 'H'},
    {"mincolor",      required_argument, 0, 'n'},
    {"seccolor",      required_argument, 0, 'C'},
    {"pale",          no_argument,       0, 'p'},
    {0, 0, 0, 0}
};

static int parse_bool(const char *val) {
    if (!val) return 1;
    if (tolower(val[0]) == 'y' || tolower(val[0]) == 'm' || val[0] == '1' ||
        (tolower(val[0]) == 'o' && tolower(val[1]) == 'n'))
        return 1;
    return 0;
}

static int parse_color(const char *val) {
    if (!val) return 0;
    return (int)strtol(val, NULL, 0);
}

void parse_arguments(int argc, char **argv) {
    int c;
    int option_index = 0;

    bm.samples = 16;
    bm.air_noswap = 0x2299ff;
    bm.liquid_noswap = 0x0055ff;
    bm.air_maxswap = 0xff0000;
    bm.liquid_maxswap = 0xaa0000;
    bm.maxbubbles = 100;
    bm.ripples = .2;
    bm.gravity = 0.01;
    bm.volatility = 1;
    bm.viscosity = .98;
    bm.speed_limit = 1.0;

    while ((c = getopt_long(argc, argv, "hpb:A:a:L:l:B:i:e:D:r:g:V:v:S:d:u:c:G:w:E:F:I:M:R:K:m:U:s:T:O:N:H:n:C:",
                            long_options, &option_index)) != -1) {
        switch (c) {
        case 'h': do_help = 1; break;
        case 'p': pale = 1; break;
        case 'b': bm.maxbubbles = atoi(optarg); break;
        case 'A': bm.air_noswap = parse_color(optarg); break;
        case 'a': bm.air_maxswap = parse_color(optarg); break;
        case 'L': bm.liquid_noswap = parse_color(optarg); break;
        case 'l': bm.liquid_maxswap = parse_color(optarg); break;
        case 'B': duck_colors[1] = parse_color(optarg); break;
        case 'i': duck_colors[2] = parse_color(optarg); break;
        case 'e': duck_colors[3] = parse_color(optarg); break;
        case 'D': delay_time = atoi(optarg); break;
        case 'r': bm.ripples = strtod(optarg, NULL); break;
        case 'g': bm.gravity = strtod(optarg, NULL); break;
        case 'V': bm.volatility = strtod(optarg, NULL); break;
        case 'v': bm.viscosity = strtod(optarg, NULL); break;
        case 'S': bm.speed_limit = strtod(optarg, NULL); break;
        case 'd': duck_enabled = parse_bool(optarg); break;
        case 'u': upside_down_duck_enabled = parse_bool(optarg); break;
        case 'c': cpu_enabled = parse_bool(optarg); break;
        case 'G': graph_digit_color = parse_color(optarg); break;
        case 'w': graph_warning_digit_color = parse_color(optarg); break;
        case 'E': graph_labels = parse_color(optarg); break;
        case 'F': graph_field = parse_color(optarg); break;
        case 'I': graph_grid = parse_color(optarg); break;
        case 'M': graph_max = parse_color(optarg); break;
        case 'R': graph_bar = parse_color(optarg); break;
        case 'K': graph_hundreds = parse_color(optarg); break;
        case 'm': memscreen_enabled = parse_bool(optarg); break;
        case 'U': memscreen_megabytes = parse_bool(optarg); break;
        case 's': shifttime = atoi(optarg); break;
        case 'T': do_digital_clock = parse_bool(optarg); break;
        case 'O': do_date = parse_bool(optarg); break;
        case 'N': do_analog_clock = parse_bool(optarg); break;
        case 'H': hourcolor = parse_color(optarg); break;
        case 'n': mincolor = parse_color(optarg); break;
        case 'C': seccolor = parse_color(optarg); break;
        default: break;
        }
    }

    if (pale) {
        graph_digit_color = 0x9ec4ed;
        graph_warning_digit_color = 0x00ffe9;
    }

    bm.ripples_int = MAKE_INTEGER(bm.ripples);
    bm.gravity_int = MAKE_INTEGER(bm.gravity);
    bm.volatility_int = MAKE_INTEGER(bm.volatility);
    bm.viscosity_int = MAKE_INTEGER(bm.viscosity);
    bm.speed_limit_int = MAKE_INTEGER(bm.speed_limit);
}

void print_usage(void) {
    printf("WMBubble version "VERSION" (Wayland)\n"
           "Usage: "NAME" [options] [program1] [program2] [...]\n\n"
           "Options:\n"
           "  -h, --help              Show this help message\n"
           "  -b, --maxbubbles NUM    Maximum number of bubbles (default: 100)\n"
           "  --air-noswap COLOR      Air color when swap is 0%% (default: 0x2299ff)\n"
           "  --air-maxswap COLOR     Air color when swap is 100%% (default: 0xff0000)\n"
           "  --liquid-noswap COLOR   Liquid color when swap is 0%% (default: 0x0055ff)\n"
           "  --liquid-maxswap COLOR  Liquid color when swap is 100%% (default: 0xaa0000)\n"
           "  --duckbody COLOR        Duck body color (default: 0xF8FC00)\n"
           "  --duckbill COLOR        Duck bill color (default: 0xF8B040)\n"
           "  --duckeye COLOR         Duck eye color (default: 0x000000)\n"
           "  -D, --delay USEC        Delay between redraws in microseconds (default: 15000)\n"
           "  -r, --ripples FLOAT     Surface disturbance on bubble events (default: 0.2)\n"
           "  -g, --gravity FLOAT     Bubble upward acceleration (default: 0.01)\n"
           "  -V, --volatility FLOAT  Surface restorative force (default: 1.0)\n"
           "  -v, --viscosity FLOAT   Surface velocity attenuation (default: 0.98)\n"
           "  -S, --speed-limit FLOAT Maximum surface velocity (default: 1.0)\n"
           "  -d, --duck [y/n]        Enable duck (default: yes)\n"
           "  -u, --upsidedown [y/n]  Allow duck to flip upside down (default: yes)\n"
           "  -c, --cpumeter [y/n]    Show CPU meter (default: yes)\n"
           "  -m, --graphs [y/n]      Show graphs on hover (default: yes)\n"
           "  -U, --units [m/k]       Memory units: megabytes or kilobytes (default: k)\n"
           "  -p, --pale              Use pale digit colors\n"
           "  -N, --analog [y/n]      Show analog clock (default: no)\n"
           "  -T, --digital [y/n]     Show digital clock (default: no)\n"
           "  -O, --showdate [y/n]    Show date (default: no)\n"
           "  -s, --shifttime HOURS   Hours after midnight to treat as previous day\n"
           "  -H, --hourcolor COLOR   Hour hand color for analog clock\n"
           "  -n, --mincolor COLOR    Minute hand color for analog clock\n"
           "  -C, --seccolor COLOR    Second hand color for analog clock\n"
           "\nColor values should be specified in hexadecimal (e.g., 0xFF0000 for red)\n");
}

static long get_time_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

int main(int argc, char **argv) {
    char execute[256];
    unsigned int loadPercentage;
    int gaugedelay, gaugedivisor, graphdelay, graphdivisor;
    int proximity = 0;
    time_t mytt;
    struct tm * mytime;
    int mday=0, hours=0;
    struct pollfd pfd;
    int timeout_ms;
    uint32_t *buffer;
    char **exec_argv;
    int exec_argc;
    long last_frame_time, current_time, elapsed;
#ifdef FPS
    int frames_count;
    time_t last_time;
    frames_count = last_time = 0;
#endif

    memset(&bm, 0, sizeof(bm));

    setlocale(LC_ALL,"");

    parse_arguments(argc, argv);

    if (do_help) {
        print_usage();
        return 0;
    }

    exec_argv = argv + optind;
    exec_argc = argc - optind;

    if (wayland_init(NAME) < 0) {
        fprintf(stderr, "Failed to initialize Wayland\n");
        return 1;
    }

    make_new_bubblemon_dockapp();

    gaugedelay = gaugedivisor = 200000 / delay_time;
    if (gaugedivisor == 0) gaugedivisor = 1;

    graphdelay = graphdivisor = 1000000 / delay_time;
    if (graphdivisor == 0) graphdivisor = 1;

    blinkdelay = 150000 / delay_time;
    if (blinkdelay == 0) blinkdelay++;

    loadPercentage = 0;

    pfd.fd = wayland_get_fd();
    pfd.events = POLLIN;
    timeout_ms = delay_time / 1000;
    if (timeout_ms < 1) timeout_ms = 1;

    last_frame_time = get_time_usec();

    while (!wayland_should_close()) {
        WaylandEvent event;

        if (poll(&pfd, 1, timeout_ms) < 0)
            break;

        if (pfd.revents & POLLIN) {
            wayland_dispatch();
        } else {
            wayland_dispatch_pending();
        }

        current_time = get_time_usec();
        elapsed = current_time - last_frame_time;
        if (elapsed < delay_time) {
            continue;
        }
        last_frame_time = current_time;

        while (wayland_get_event(&event)) {
            switch (event.type) {
            case WL_EVENT_BUTTON:
                if (memscreen_enabled && event.button == 3) {
                    bm.picture_lock = !bm.picture_lock;
                } else if (event.button <= (unsigned)exec_argc && event.button > 0) {
                    snprintf(execute, 250, "%s &", exec_argv[event.button - 1]);
                    if (system(execute) == -1)
                        duck_blink += 6 * blinkdelay;
                }
                break;
            case WL_EVENT_ENTER:
                proximity = 1;
                if (!bm.picture_lock)
                    bm.screen_type = 1;
                break;
            case WL_EVENT_LEAVE:
                proximity = 0;
                break;
            case WL_EVENT_CLOSE:
                break;
            default:
                break;
            }
        }

        get_memory_load_percentage();

        if (++gaugedelay >= gaugedivisor) {
            loadPercentage = system_cpu();
            gaugedelay = 0;
        }

        if (memscreen_enabled && ++graphdelay >= graphdivisor) {
            roll_history();
            graphdelay = 0;
        }

        if (wayland_is_visible()) {
            do_water_sim(loadPercentage);
            draw_watertank();
            bubblebuf_colorspace();
        }

        if (duck_enabled && wayland_is_visible()) {
            duck_swimmer();
        }

        if (wayland_is_visible() && cpu_enabled && gaugedelay == 0)
            draw_cpugauge(loadPercentage);

        calculate_transparencies(proximity);

        if (wayland_is_visible() && memscreen_enabled && graph_alpha < GRAPHMAXBLEND && graphdelay == 0)
            render_secondary();

        if (wayland_is_visible() && cpu_enabled)
            alpha_cpu();

        if (wayland_is_visible() && do_analog_clock)
            draw_clockhands();

        time(&mytt);
        mytime = localtime(&mytt);
        mday = mytime->tm_mday;
        hours = 0;

        if (mytime->tm_hour<shifttime) {
            while (mday == mytime->tm_mday) {
                mytt -= 3600; hours++;
                mytime = localtime(&mytt);
            }
            mytime->tm_hour += hours;
        }

        if (wayland_is_visible() && do_digital_clock)
            alpha_digitalclock(mytime);

        if (wayland_is_visible() && do_date)
            alpha_date(mytime);

        if (wayland_is_visible() && memscreen_enabled && graph_alpha < GRAPHMAXBLEND)
            alpha_graph();

#ifdef FPS
        frames_count++;
        if(time(NULL)!=last_time) {
            fprintf(stderr,"%03dfps\n",frames_count);
            frames_count=0;
            last_time=time(NULL);
        }
#endif

        if (wayland_is_visible() && wayland_can_render()) {
            int xx, yy;
            uint32_t pixel;

            for (xx = 0; xx < BOX_SIZE - 1; xx++) {
                pixel = bm.rgb_buf[xx];
                bm.rgb_buf[xx] = ((GET_RED(pixel)/4) << 16) | ((GET_GRN(pixel)/4) << 8) | (GET_BLU(pixel)/4);
                pixel = bm.rgb_buf[(BOX_SIZE-1)*BOX_SIZE + xx + 1];
                bm.rgb_buf[(BOX_SIZE-1)*BOX_SIZE + xx + 1] = (((255+GET_RED(pixel))/2) << 16) | (((255+GET_GRN(pixel))/2) << 8) | ((255+GET_BLU(pixel))/2);
            }

            for (yy = 0; yy < BOX_SIZE - 1; yy++) {
                pixel = bm.rgb_buf[yy * BOX_SIZE];
                bm.rgb_buf[yy * BOX_SIZE] = ((GET_RED(pixel)/4) << 16) | ((GET_GRN(pixel)/4) << 8) | (GET_BLU(pixel)/4);
                pixel = bm.rgb_buf[yy * BOX_SIZE + BOX_SIZE - 1];
                bm.rgb_buf[yy * BOX_SIZE + BOX_SIZE - 1] = (((255+GET_RED(pixel))/2) << 16) | (((255+GET_GRN(pixel))/2) << 8) | ((255+GET_BLU(pixel))/2);
            }

            buffer = wayland_get_buffer();
            memcpy(buffer, bm.rgb_buf, BOX_SIZE * BOX_SIZE * sizeof(uint32_t));
            wayland_commit_buffer();
        }
    }

    wayland_cleanup();
    return 0;
}

void make_new_bubblemon_dockapp(void) {
    unsigned int cc, yy, maxwidth;
    int xx;

    bm.n_bubbles = 0;

    bubblemon_allocate_buffers();

    build_graphs();

    sscanf(datefont_xpm[0],"%u %u %u %u",&maxwidth,&yy,&datefont_offset,&cc);
    if (cc != 1) abort();

    datefont_offset++;

    for (yy = 1; yy < datefont_offset; yy++) {
        if (strcasestr(datefont_xpm[yy],"none")) {
            datefont_transparent = datefont_xpm[yy][0];
            yy = datefont_offset;
        }
    }

    for (cc = 33; cc < 128; cc++)
        for (xx = maxwidth-1; xx >= 0; xx--)
            for (yy = 0; yy < 8; yy++)
                if (datefont_xpm[(cc-32)*8+yy+datefont_offset][xx] != datefont_transparent) {
                    datefont_widths[cc] = xx+2;
                    xx = -1; yy = 9;
                }
    datefont_widths[' ']=2;
    for (cc = 0; cc < 32; cc++)
        datefont_widths[cc] = BOX_SIZE;
    for (cc = 128; cc < 256; cc++)
        datefont_widths[cc] = BOX_SIZE;
}

void do_water_sim(int loadPercentage) {
    unsigned int i, x;
    unsigned int waterlevels_goal;

    waterlevels_goal = MAKEY(BOX_SIZE) - ((bm.mem_percent * MAKEY(BOX_SIZE)) / 100);

    waterlevels_goal -= (1 << (POWER2 - 1));

    bm.waterlevels[0] = waterlevels_goal;
    bm.waterlevels[BOX_SIZE-1] = waterlevels_goal;

    for (x = 1; x < BOX_SIZE-1; x++) {
        bm.waterlevels_dy[x] +=
            (((bm.waterlevels[x - 1] + bm.waterlevels[x + 1] - 2 * bm.waterlevels[x])
              * bm.volatility_int) >> (POWER2 + 1));

        bm.waterlevels_dy[x] *= bm.viscosity_int;
        bm.waterlevels_dy[x] >>= POWER2;

        if (bm.waterlevels_dy[x] > bm.speed_limit_int)
            bm.waterlevels_dy[x] = bm.speed_limit_int;
        else if (bm.waterlevels_dy[x] < -bm.speed_limit_int)
            bm.waterlevels_dy[x] = -bm.speed_limit_int;
    }

    for (x = 1; x < BOX_SIZE-1; x++) {
        bm.waterlevels[x] = bm.waterlevels[x] + bm.waterlevels_dy[x];

        if (bm.waterlevels[x] > MAKEY(BOX_SIZE)) {
            bm.waterlevels[x] = MAKEY(BOX_SIZE);
            bm.waterlevels_dy[x] = 0;
        } else if (bm.waterlevels[x] < 0) {
            bm.waterlevels[x] = 0;
            bm.waterlevels_dy[x] = 0;
        }
    }

    if ((bm.n_bubbles < bm.maxbubbles)
        && ((rand() % 101) <= loadPercentage)) {
        bm.bubbles[bm.n_bubbles].x = (rand() % (BOX_SIZE-2)) + 1;
        bm.bubbles[bm.n_bubbles].y = MAKEY(BOX_SIZE-1);
        bm.bubbles[bm.n_bubbles].dy = 0;

        if (bm.bubbles[bm.n_bubbles].x > 2)
            bm.waterlevels[bm.bubbles[bm.n_bubbles].x - 2] -= bm.ripples_int;
        bm.waterlevels[bm.bubbles[bm.n_bubbles].x - 1] -= bm.ripples_int;
        bm.waterlevels[bm.bubbles[bm.n_bubbles].x] -= bm.ripples_int;
        bm.waterlevels[bm.bubbles[bm.n_bubbles].x + 1] -= bm.ripples_int;
        if (bm.bubbles[bm.n_bubbles].x < (BOX_SIZE-3))
            bm.waterlevels[bm.bubbles[bm.n_bubbles].x + 2] -= bm.ripples_int;

        bm.n_bubbles++;
    }

    for (i = 0; i < bm.n_bubbles; i++) {
        bm.bubbles[i].dy -= bm.gravity_int;

        bm.bubbles[i].y += bm.bubbles[i].dy;

        if (bm.bubbles[i].x < 1 || bm.bubbles[i].x > (BOX_SIZE-2) ||
            bm.bubbles[i].y > MAKEY(BOX_SIZE)) {
            bm.n_bubbles--;
            bm.bubbles[i].x = bm.bubbles[bm.n_bubbles].x;
            bm.bubbles[i].y = bm.bubbles[bm.n_bubbles].y;
            bm.bubbles[i].dy = bm.bubbles[bm.n_bubbles].dy;
            i--;
            continue;
        }

        if (bm.bubbles[i].y < bm.waterlevels[bm.bubbles[i].x]) {
            bm.waterlevels[bm.bubbles[i].x - 1] += bm.ripples_int;
            bm.waterlevels[bm.bubbles[i].x] += 3 * bm.ripples_int;
            bm.waterlevels[bm.bubbles[i].x + 1] += bm.ripples_int;

            bm.n_bubbles--;
            bm.bubbles[i].x = bm.bubbles[bm.n_bubbles].x;
            bm.bubbles[i].y = bm.bubbles[bm.n_bubbles].y;
            bm.bubbles[i].dy = bm.bubbles[bm.n_bubbles].dy;

            i--;
            continue;
        }
    }
}

void draw_watertank(void) {
    int x, y, i;
    unsigned char *bubblebuf_ptr;

    for (x = 0; x < BOX_SIZE; x++) {
        int water_y = REALY(bm.waterlevels[x]);
        unsigned char *col_ptr = &bm.bubblebuf[x];
        for (y = 0; y < water_y; y++) {
            *col_ptr = aircolor;
            col_ptr += BOX_SIZE;
        }
        for (; y < BOX_SIZE; y++) {
            *col_ptr = watercolor;
            col_ptr += BOX_SIZE;
        }
    }

    for (i = 0; i < bm.n_bubbles; i++) {
        int bx = bm.bubbles[i].x;
        int by = REALY(bm.bubbles[i].y);
        bubblebuf_ptr = &bm.bubblebuf[by * BOX_SIZE + bx];

        /* Draw 3x3 bubble centered at (bx, by) */
        /* Row by-1 */
        unsigned char *row_ptr = bubblebuf_ptr - BOX_SIZE;
        if (row_ptr[-1] < aircolor) row_ptr[-1]++;
        row_ptr[0] = aircolor;
        if (row_ptr[1] < aircolor) row_ptr[1]++;

        /* Row by */
        row_ptr = bubblebuf_ptr;
        row_ptr[-1] = aircolor;
        row_ptr[0] = aircolor;
        row_ptr[1] = aircolor;

        /* Row by+1 */
        if (by < BOX_SIZE - 1) {
            row_ptr = bubblebuf_ptr + BOX_SIZE;
            if (row_ptr[-1] < aircolor) row_ptr[-1]++;
            row_ptr[0] = aircolor;
            if (row_ptr[1] < aircolor) row_ptr[1]++;
        }
    }
}


void bubblebuf_colorspace(void) {
    uint32_t palette[3];
    int count;

    palette[watercolor] =
        (((GET_RED(bm.liquid_maxswap) * bm.swap_percent +
           GET_RED(bm.liquid_noswap) * (100 - bm.swap_percent)) / 100) << 16) |
        (((GET_GRN(bm.liquid_maxswap) * bm.swap_percent +
           GET_GRN(bm.liquid_noswap) * (100 - bm.swap_percent)) / 100) << 8) |
        ((GET_BLU(bm.liquid_maxswap) * bm.swap_percent +
          GET_BLU(bm.liquid_noswap) * (100 - bm.swap_percent)) / 100);

    palette[aircolor] =
        (((GET_RED(bm.air_maxswap) * bm.swap_percent +
           GET_RED(bm.air_noswap) * (100 - bm.swap_percent)) / 100) << 16) |
        (((GET_GRN(bm.air_maxswap) * bm.swap_percent +
           GET_GRN(bm.air_noswap) * (100 - bm.swap_percent)) / 100) << 8) |
        ((GET_BLU(bm.air_maxswap) * bm.swap_percent +
          GET_BLU(bm.air_noswap) * (100 - bm.swap_percent)) / 100);

    palette[antialiascolor] =
        ((((palette[watercolor] >> 16) & 255) + ((palette[aircolor] >> 16) & 255)) / 2 << 16) |
        ((((palette[watercolor] >> 8) & 255) + ((palette[aircolor] >> 8) & 255)) / 2 << 8) |
        (((palette[watercolor] & 255) + (palette[aircolor] & 255)) / 2);

    for (count = 0; count < BOX_SIZE * BOX_SIZE; count++) {
        bm.rgb_buf[count] = palette[bm.bubblebuf[count]];
    }
}

void draw_from_xpm(char **xpm, unsigned char *whither, unsigned int targetw,
                   unsigned int xpmx, unsigned int xpmy, unsigned int xpmw,
                   unsigned int xpmh, unsigned int color) {
    unsigned char r=GET_RED(color),g=GET_GRN(color),b=GET_BLU(color);
    unsigned int yy,xx,ncolors,cpp;
    unsigned char * to;
    char * from;
    char transparent=0;

    sscanf(xpm[0],"%u %u %u %u",&xx,&yy,&ncolors,&cpp);
    if (cpp != 1) abort();
    if (xpmx+xpmw > xx || xpmy+xpmh > yy) return;

    for (yy=1;yy<=ncolors;yy++) {
        if (strcasestr(xpm[yy],"none")) {
            transparent = xpm[yy][0];
            yy=255;
        }
    }

    for (yy=0;yy<xpmh;yy++) {
        to = whither + targetw*3*yy;
        from = &xpm[1+ncolors+xpmy+yy][xpmx];
        for (xx=0;xx<xpmw;xx++,from++,to+=3) {
            if (*from != transparent) {
                to[0]=r; to[1]=g; to[2]=b;
            }
        }
    }
}

void draw_digit(unsigned char *from, uint32_t *whither) {
    int yy, xx;
    for (yy = 0; yy < 8; yy++) {
        for (xx = 0; xx < 4; xx++) {
            whither[xx] = (from[xx*3] << 16) | (from[xx*3+1] << 8) | from[xx*3+2];
        }
        from += 12;
        whither += BOX_SIZE;
    }
}

void draw_string(char *string, int x, int y, int color) {
    unsigned char c;
    unsigned char *graph_numbers = graph_numbers_n_rgb;

    if (color) graph_numbers = graph_numbers_b_rgb;

    while ((c = *string++)) {
        if (c == 'K') c = 10;
        else if (c == 'M') c = 11;
        else if (c >= '0' && c <= '9') c -= '0';

        if (c <= 11)
            draw_digit(&graph_numbers[3*4*9*c],
                       &bm.mem_buf[y*BOX_SIZE+x]);
        x += 4;
    }
}

void draw_history(int num, int size, unsigned int *history, uint32_t *buf) {
    int pixels_per_byte;
    int yy, xx;
    int height;
    uint32_t max_color, bar_color, hundreds_color;

    pixels_per_byte = 100;

    for (xx = 0; xx < num; xx++) {
        while (history[xx] > (unsigned)pixels_per_byte)
            pixels_per_byte += 100;
    }

    max_color = graph_max;
    bar_color = graph_bar;
    hundreds_color = graph_hundreds;

    for (xx = 0; xx < num; xx++) {
        height = size - size * history[xx] / pixels_per_byte;

        for (yy = height; yy < height+2 && yy < size; yy++) {
            buf[yy*BOX_SIZE+xx+2] = max_color;
        }

        for (;yy < size; yy++) {
            buf[yy*BOX_SIZE+xx+2] = bar_color;
        }
    }

    for (yy = pixels_per_byte - 100; yy > 0; yy -= 100) {
        height = size - size * yy / pixels_per_byte;
        for (xx = 0; xx < num; xx++) {
            buf[height*BOX_SIZE+xx+2] = hundreds_color;
        }
    }
}

void render_secondary(void) {
    char percent[4];
    char number[8];
    int i;
    unsigned char *src = bm.screen_type ? empty_loadgraph : empty_memgraph;

    for (i = 0; i < BOX_SIZE * BOX_SIZE; i++) {
        bm.mem_buf[i] = (src[i*3] << 16) | (src[i*3+1] << 8) | src[i*3+2];
    }

    if (bm.screen_type) {
#if BOX_SIZE >= 55
        for (i = 0; i < 3; i++) {
            sprintf(number, "%2d", bm.loadavg[i].i);
            draw_string(number, ((BOX_SIZE-1)/2-27) + 19*i,      9, 0);
            sprintf(number, "%02d", bm.loadavg[i].f);
            draw_string(number, ((BOX_SIZE-1)/2-27) + 19*i + 10, 9, 0);
        }
#endif
        draw_history(BOX_SIZE-4, BOX_SIZE-4-21, bm.history, &bm.mem_buf[21*BOX_SIZE]);
    } else {
        if (memscreen_megabytes || bm.mem_used > (999999<<10))
            snprintf(number, 8, "%6"PRIu64"M", bm.mem_used >> 20);
        else
            snprintf(number, 8, "%"PRIu64"K", bm.mem_used >> 10);
        snprintf(percent, 4, "%+3d", bm.mem_percent);
        draw_string(number, 3, 2, (bm.mem_percent > 90) ? 1 : 0);
        draw_string(percent, 39, 2, (bm.mem_percent > 90) ? 1 : 0);

        if (memscreen_megabytes || bm.swap_used > (999999<<10))
            snprintf(number, 8, "%6"PRIu64"M", bm.swap_used >> 20);
        else
            snprintf(number, 8, "%6"PRIu64"K", bm.swap_used >> 10);
        snprintf(percent, 4, "%+3d", bm.swap_percent);
        draw_string(number, 3, 11, (bm.swap_percent > 90) ? 1 : 0);
        draw_string(percent, 39, 11, (bm.swap_percent > 90) ? 1 : 0);

        draw_history(BOX_SIZE-4, BOX_SIZE-4-19, bm.memhist, &bm.mem_buf[19*BOX_SIZE]);
    }
}

void roll_history(void)  {
    system_loadavg();

    bm.history[BOX_SIZE-4] = bm.loadavg[0].f + (bm.loadavg[0].i * 100);
    bm.memhist[BOX_SIZE-4] = bm.mem_percent;

    memmove(&bm.history[0], &bm.history[1], sizeof(bm.history));
    memmove(&bm.memhist[0], &bm.memhist[1], sizeof(bm.memhist));
}

void draw_cpudigit(int what, unsigned char *whither) {
    unsigned int y;
    unsigned char *from = digits + what * 3 * 6;
    for (y = 0; y < 9; y++) {
        uint32_t *w = (uint32_t *)whither;
        uint32_t *f = (uint32_t *)from;
        w[0] = f[0];
        w[1] = f[1];
        w[2] = f[2];
        w[3] = f[3];
        w[4] = f[4];
        whither[20] = from[20];
        whither += 3*25;
        from += 3*95;
    }
}

void draw_cpugauge(int cpu) {
    if (cpu >= 100) {
        draw_cpudigit(1, cpu_gauge);
        draw_cpudigit(0, &cpu_gauge[3*6]);
        draw_cpudigit(0, &cpu_gauge[3*12]);
    } else {
        draw_cpudigit(12, cpu_gauge);
        draw_cpudigit(cpu / 10, &cpu_gauge[3*6]);
        draw_cpudigit(cpu % 10, &cpu_gauge[3*12]);
    }
    draw_cpudigit(10, &cpu_gauge[3*18]);
}

void draw_dtchr(const char letter, uint32_t *rgbbuf) {
    int x,y;
    char * xpm_line;

    for (y=0;y<8;y++) {
        xpm_line = datefont_xpm[((unsigned char)letter-32)*8+y+datefont_offset];
        for (x=0;x<datefont_widths[(unsigned char)letter]-1;x++)
            if (xpm_line[x] != datefont_transparent) {
                uint32_t pixel = rgbbuf[y*BOX_SIZE+x];
                int r = GET_RED(pixel) >> 1;
                int g = GET_GRN(pixel) >> 1;
                int b = GET_BLU(pixel) >> 1;
                rgbbuf[y*BOX_SIZE+x] = (r << 16) | (g << 8) | b;
            }
    }
}

void draw_largedigit(char number, uint32_t *rgbbuf) {
    int x,y;
    int v;
    unsigned char *from;

    if (number>='0' && number<='9') number-='0';
    if (number>=0 && number<=9) {
        for (y=0;y<32;y++) {
            from=&bigdigits[number*13+y*130];
            for (x=0;x<13;x++) {
                uint32_t pixel = rgbbuf[y*BOX_SIZE+x];
                v=from[x]>>2;
                int r = GET_RED(pixel) + v; if (r > 255) r = 255;
                int g = GET_GRN(pixel) + v; if (g > 255) g = 255;
                int b = GET_BLU(pixel) + v; if (b > 255) b = 255;
                rgbbuf[y*BOX_SIZE+x] = (r << 16) | (g << 8) | b;
            }
        }
    }
}

void draw_rgba_pixel(uint32_t *whither, int color, float opacity) {
    uint32_t pixel = *whither;
    int r = (int)(opacity * GET_RED(color) + (1 - opacity) * GET_RED(pixel));
    int g = (int)(opacity * GET_GRN(color) + (1 - opacity) * GET_GRN(pixel));
    int b = (int)(opacity * GET_BLU(color) + (1 - opacity) * GET_BLU(pixel));
    *whither = (r << 16) | (g << 8) | b;
}

float fpart(float in) { return in - floor(in); }

void draw_aa_line(float x1,float y1, float x2,float y2, int color) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float tmp, gradient;
    float xend, yend, xgap, ygap, intery, interx;
    int x1pxl, x2pxl, y1pxl, y2pxl, xx, yy;

    if (fabsf(dx) > fabsf(dy)) {
        if (x2 < x1) {
            tmp=x1; x1=x2; x2=tmp;
            tmp=y1; y1=y2; y2=tmp;
        }
        gradient = dy / dx;

        xend = floor(x1+.5);
        yend = y1 + gradient * (xend - x1);
        xgap = 1-fpart(x1 + 0.5);

        x1pxl = xend;
        y1pxl = floor(yend);
        draw_rgba_pixel(&bm.rgb_buf[x1pxl+BOX_SIZE*(y1pxl  )], color, (1-fpart(yend)) * xgap );
        draw_rgba_pixel(&bm.rgb_buf[x1pxl+BOX_SIZE*(y1pxl+1)], color, fpart(yend) * xgap);
        intery = yend + gradient;

        xend = floor(x2+.5);
        yend = y2 + gradient * (xend - x2);
        xgap = fpart(x2 + 0.5);
        x2pxl = xend;
        y2pxl = floor(yend);
        draw_rgba_pixel(&bm.rgb_buf[x2pxl+BOX_SIZE*(y2pxl  )], color, (1-fpart(yend)) * xgap );
        draw_rgba_pixel(&bm.rgb_buf[x2pxl+BOX_SIZE*(y2pxl+1)], color, fpart(yend) * xgap);

        for(xx = x1pxl + 1; xx <= x2pxl - 1; xx++) {
            draw_rgba_pixel(&bm.rgb_buf[xx+BOX_SIZE*(int)(intery)], color, 1-fpart (intery) );
            draw_rgba_pixel(&bm.rgb_buf[xx+BOX_SIZE*(int)(intery+1)], color, fpart (intery) );
            intery = intery + gradient;
        }
    } else {
        if (y2 < y1) {
            tmp=y1; y1=y2; y2=tmp;
            tmp=x1; x1=x2; x2=tmp;
        }
        gradient = dx / dy;

        yend = floor(y1+.5);
        xend = x1 + gradient * (yend - y1);
        ygap = 1-fpart(y1 + 0.5);

        y1pxl = yend;
        x1pxl = floor(xend);
        draw_rgba_pixel(&bm.rgb_buf[  x1pxl+BOX_SIZE*y1pxl], color, (1-fpart(xend)) * ygap );
        draw_rgba_pixel(&bm.rgb_buf[1+x1pxl+BOX_SIZE*y1pxl], color, fpart(xend) * ygap);
        interx = xend + gradient;

        yend = floor(y2+.5);
        xend = x2 + gradient * (yend - y2);
        ygap = fpart(y2 + 0.5);
        y2pxl = yend;
        x2pxl = floor(xend);
        draw_rgba_pixel(&bm.rgb_buf[  x2pxl+BOX_SIZE*y2pxl], color, (1-fpart(xend)) * ygap );
        draw_rgba_pixel(&bm.rgb_buf[1+x2pxl+BOX_SIZE*y2pxl], color, fpart(xend) * ygap);

        for(yy = y1pxl + 1; yy <= y2pxl - 1; yy++) {
            draw_rgba_pixel(&bm.rgb_buf[  (int)interx+BOX_SIZE*yy], color, 1-fpart (interx) );
            draw_rgba_pixel(&bm.rgb_buf[1+(int)interx+BOX_SIZE*yy], color, fpart (interx) );
            interx = interx + gradient;
        }
    }
}

void draw_clockhands(void) {
    struct tm * mytime;
    struct timeval tv;
    float theta;

    gettimeofday(&tv,NULL);
    mytime = localtime(&tv.tv_sec);

    theta = (mytime->tm_hour * 3600u + mytime->tm_min * 60u + mytime->tm_sec) * (M_PI / 21600.0);
    draw_aa_line(BOX_SIZE/2,
                 (BOX_SIZE-10)/2,
                 BOX_SIZE/2 + 0.6*(BOX_SIZE-10)/2 * sin(theta),
                 (BOX_SIZE-10)/2 - 0.6*(BOX_SIZE-10)/2 * cos(theta),
                 hourcolor);

    theta = (mytime->tm_min * 60000000u + mytime->tm_sec * 1000000u + tv.tv_usec) * (M_PI / 1800000000u);
    draw_aa_line(BOX_SIZE/2,
                 (BOX_SIZE-10)/2,
                 BOX_SIZE/2 + 0.75*(BOX_SIZE-10)/2 * sin(theta),
                 (BOX_SIZE-10)/2 - 0.75*(BOX_SIZE-10)/2 * cos(theta),
                 mincolor);

    theta = (mytime->tm_sec * 1000000u + tv.tv_usec) * (M_PI / 30000000u);
    draw_aa_line(BOX_SIZE/2,
                 (BOX_SIZE-10)/2,
                 BOX_SIZE/2 + 0.95*(BOX_SIZE-10)/2 * sin(theta),
                 (BOX_SIZE-10)/2 - 0.95*(BOX_SIZE-10)/2 * cos(theta),
                 seccolor);
}

void alpha_date(struct tm * mytime) {
    const char *roman[]={"I","II","III","IV","V","VI","VII","VIII","IX","X","XI","XII"};
    char format[32];
    int ii, width;
    uint32_t *rgbptr;

    if (strftime(format,32,"%a %b %d",mytime) != 0) {
        for (width = ii = 0; ii < (int)strlen(format); ii++)
            width += datefont_widths[(unsigned char)format[ii]];
        if (width > BOX_SIZE - 1) {
            snprintf(format,32,"%s-%d",roman[mytime->tm_mon],mytime->tm_mday);
            for (width = ii = 0; ii < (int)strlen(format); ii++)
                width += datefont_widths[(unsigned char)format[ii]];
        }
    } else {
        snprintf(format,32,"%s-%d",roman[mytime->tm_mon],mytime->tm_mday);
        for (width = ii = 0; ii < (int)strlen(format); ii++)
            width += datefont_widths[(unsigned char)format[ii]];
    }

    rgbptr = &bm.rgb_buf[2*BOX_SIZE+(BOX_SIZE-width)/2];

    for (ii = 0; ii < (int)strlen(format); ii++) {
        draw_dtchr(format[ii],rgbptr);
        rgbptr += datefont_widths[(unsigned char)format[ii]];
    }
}

void alpha_digitalclock(struct tm * mytime) {
#if BOX_SIZE >= 54
    draw_largedigit(mytime->tm_hour/10,
                    &bm.rgb_buf[(BOX_SIZE/2)-26+BOX_SIZE*(BOX_SIZE/2-16)]);
    draw_largedigit(mytime->tm_hour%10,
                    &bm.rgb_buf[(BOX_SIZE/2)-13+BOX_SIZE*(BOX_SIZE/2-16)]);
    draw_largedigit(mytime->tm_min/10,
                    &bm.rgb_buf[(BOX_SIZE/2)   +BOX_SIZE*(BOX_SIZE/2-16)]);
    draw_largedigit(mytime->tm_min%10,
                    &bm.rgb_buf[(BOX_SIZE/2)+13+BOX_SIZE*(BOX_SIZE/2-16)]);
#endif
}

void calculate_transparencies(int proximity) {
    static int gauge_rate, graph_transparent_rate, graph_opaque_rate;

    if (gauge_rate == 0) {
        gauge_rate = delay_time * (CPUMAXBLEND-CPUMINBLEND) / 412500;
        if (gauge_rate == 0) gauge_rate++;
        graph_transparent_rate = delay_time * (GRAPHMAXBLEND-GRAPHMINBLEND) / 540000;
        if (graph_transparent_rate == 0) graph_transparent_rate++;
        graph_opaque_rate = delay_time * (GRAPHMAXBLEND-GRAPHMINBLEND) / 324000;
        if (graph_opaque_rate == 0) graph_opaque_rate++;
    }

    if (proximity) {
        gauge_alpha -= gauge_rate;
        if (gauge_alpha < CPUMINBLEND) {
            gauge_alpha = CPUMINBLEND;
            if (memscreen_enabled && !bm.picture_lock) {
                if (graph_alpha == GRAPHMAXBLEND) {
                    render_secondary();
                }
                graph_alpha -= graph_transparent_rate;
                if (graph_alpha < GRAPHMINBLEND) {
                    graph_alpha = GRAPHMINBLEND;
                }
            }
        }
    } else {
        gauge_alpha += gauge_rate;
        if (gauge_alpha > CPUMAXBLEND) {
            gauge_alpha = CPUMAXBLEND;
        }
        if (memscreen_enabled && !bm.picture_lock) {
            graph_alpha += graph_opaque_rate;
            if (memscreen_enabled && graph_alpha > GRAPHMAXBLEND) {
                graph_alpha = GRAPHMAXBLEND;
            }
        }
    }
}


void alpha_cpu(void) {
    unsigned char *gaugeptr;
    int y, x;
    gaugeptr = cpu_gauge;
    for (y = 0; y < 9; y++) {
        uint32_t *rgbptr = &bm.rgb_buf[(y + (BOX_SIZE-10)) * BOX_SIZE + (BOX_SIZE/2-12)];
        for (x = 0; x < 25; x++) {
            uint32_t pixel = rgbptr[x];
            int r = (gauge_alpha * GET_RED(pixel) + (256 - gauge_alpha) * gaugeptr[0]) >> 8;
            int g = (gauge_alpha * GET_GRN(pixel) + (256 - gauge_alpha) * gaugeptr[1]) >> 8;
            int b = (gauge_alpha * GET_BLU(pixel) + (256 - gauge_alpha) * gaugeptr[2]) >> 8;
            rgbptr[x] = (r << 16) | (g << 8) | b;
            gaugeptr += 3;
        }
    }
}

void alpha_graph(void) {
    int count;
    for (count = 0; count < BOX_SIZE * BOX_SIZE; count++) {
        uint32_t rgb_pixel = bm.rgb_buf[count];
        uint32_t mem_pixel = bm.mem_buf[count];
        int r = (graph_alpha * GET_RED(rgb_pixel) + (256 - graph_alpha) * GET_RED(mem_pixel)) >> 8;
        int g = (graph_alpha * GET_GRN(rgb_pixel) + (256 - graph_alpha) * GET_GRN(mem_pixel)) >> 8;
        int b = (graph_alpha * GET_BLU(rgb_pixel) + (256 - graph_alpha) * GET_BLU(mem_pixel)) >> 8;
        bm.rgb_buf[count] = (r << 16) | (g << 8) | b;
    }
}

void draw_duck(int x, int y, int frame_no, int flipx, int flipy) {
    int xx, yy;
    int real_x;
    int real_y;
    int pos;
    int duck_right, duck_left, duck_bottom, duck_top;
    int cmap;
    duck_top = 0;
    if (y < 0)
        duck_top = -(y);
    duck_bottom = 17;
    if ((y + 17) > BOX_SIZE)
        duck_bottom = BOX_SIZE - y;
    duck_right = 18;
    if (x > BOX_SIZE-18)
        duck_right = 18 - (x - (BOX_SIZE-18));
    duck_left = 0;
    if (x < 0)
        duck_left = -(x);
    if (duck_blink > 0) {
        if (duck_blink % blinkdelay == 0)
            duck_colors[1] = 0x808080 ^ duck_colors[1];
        duck_blink--;
    }

    for (yy = duck_top; yy < duck_bottom; yy++) {
        int ypos = (yy + y) * BOX_SIZE;
        real_y = (flipy && upside_down_duck_enabled) ? 16 - yy : yy;
        int *duck_row = &duck_data[frame_no][real_y * 18];
        for (xx = duck_left; xx < duck_right; xx++) {
            real_x = flipx ? 17 - xx : xx;
            if ((cmap = duck_row[real_x]) != 0) {
                uint32_t duck_color = duck_colors[cmap];
                pos = ypos + xx + x;

                if (yy + y < REALY(bm.waterlevels[xx + x])) {
                    bm.rgb_buf[pos] = duck_color;
                } else {
                    uint32_t pixel = bm.rgb_buf[pos];
                    int r = (DUCKBLEND * GET_RED(pixel) + (256 - DUCKBLEND) * GET_RED(duck_color)) >> 8;
                    int g = (DUCKBLEND * GET_GRN(pixel) + (256 - DUCKBLEND) * GET_GRN(duck_color)) >> 8;
                    int b = (DUCKBLEND * GET_BLU(pixel) + (256 - DUCKBLEND) * GET_BLU(duck_color)) >> 8;
                    bm.rgb_buf[pos] = (r << 16) | (g << 8) | b;
                }
            }
        }
    }
}

int animate_correctly(void) {
    const int outp[16] =
        { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1 };
    static int totalcounter = -1;

    if (++totalcounter > 15)
        totalcounter = 0;

    return outp[totalcounter];
}

void duck_swimmer() {
    static int tx = -19;
    static int rp;
    static int rev = 1;
    static int upsidedown = 0;
    int posy, avx;

    avx = tx-2;
    if (avx<0) avx=0;
    if (avx>BOX_SIZE-5) avx=BOX_SIZE-5;

    posy = REALY(bm.waterlevels[avx] + bm.waterlevels[avx+1]*2 +
                 bm.waterlevels[avx+2]*2 + bm.waterlevels[avx+3]*2 +
                 bm.waterlevels[avx+4])/8 - 13;

    if (upside_down_duck_enabled) {
        if (upsidedown == 0 && posy < 3)
            upsidedown = 1;
        else if (upsidedown == 1 && posy > 6)
            upsidedown = 0;

        if (upsidedown)
            posy += 10;
    }
    if (rp++ < 10) {
        draw_duck(tx, posy, animate_correctly(), rev, upsidedown);
        return;
    }

    rp = 0;
    if (!rev) {
        if (tx-- < -18) {
            tx = -18;
            rev = 1;
        }
    } else {
        if (tx++ > BOX_SIZE+1) {
            tx = BOX_SIZE+1;
            rev = 0;
        }
    }
    draw_duck(tx, posy, animate_correctly(), rev, upsidedown);
}

void bubblemon_allocate_buffers(void) {
    int ii;

    bm.bubbles = (Bubble *) malloc(sizeof(Bubble) * bm.maxbubbles);

    for (ii = 0; ii < BOX_SIZE; ii++) {
        bm.waterlevels[ii] = MAKEY(BOX_SIZE);
    }

    empty_loadgraph = calloc(BOX_SIZE * BOX_SIZE,3);
    empty_memgraph = calloc(BOX_SIZE * BOX_SIZE,3);
    graph_numbers_n_rgb = calloc(3*4*9*12,1);
    graph_numbers_b_rgb = calloc(3*4*9*12,1);

    for (ii = 0; ii < 12; ii++) {
        draw_from_xpm(numbers_xpm,&graph_numbers_n_rgb[ii*3*4*9],4,
                      4*ii,0,3,9,graph_digit_color);
        draw_from_xpm(numbers_xpm,&graph_numbers_b_rgb[ii*3*4*9],4,
                      4*ii,0,3,9,graph_warning_digit_color);
    }

    bm.loadIndex = 0;
    bm.load = calloc(bm.samples, sizeof(u_int64_t));
    bm.total = calloc(bm.samples, sizeof(u_int64_t));
}

void build_graphs(void) {
    int xx, yy;
    unsigned char r,g,b;

    draw_from_xpm(ofmspct_xpm,&empty_memgraph[3*(32+4*BOX_SIZE)],BOX_SIZE,
                  6,0,5,5,graph_labels);
    draw_from_xpm(ofmspct_xpm,&empty_memgraph[3*(51+2*BOX_SIZE)],BOX_SIZE,
                  18,0,4,8,graph_digit_color);
    draw_from_xpm(ofmspct_xpm,&empty_memgraph[3*(32+13*BOX_SIZE)],BOX_SIZE,
                  12,0,5,5,graph_labels);
    draw_from_xpm(ofmspct_xpm,&empty_memgraph[3*(51+11*BOX_SIZE)],BOX_SIZE,
                  18,0,4,8,graph_digit_color);

    draw_from_xpm(ofmspct_xpm,&empty_loadgraph[3*(((BOX_SIZE-1)/2-20)+2*BOX_SIZE)],BOX_SIZE,
                  0,0,2,5,graph_labels);
    draw_from_xpm(ofmspct_xpm,&empty_loadgraph[3*(((BOX_SIZE-1)/2- 1)+2*BOX_SIZE)],BOX_SIZE,
                  3,0,2,5,graph_labels);
    draw_from_xpm(ofmspct_xpm,&empty_loadgraph[3*(((BOX_SIZE-1)/2+17)+2*BOX_SIZE)],BOX_SIZE,
                  0,0,2,5,graph_labels);
    draw_from_xpm(ofmspct_xpm,&empty_loadgraph[3*(((BOX_SIZE-1)/2+20)+2*BOX_SIZE)],BOX_SIZE,
                  3,0,2,5,graph_labels);
    draw_from_xpm(ofmspct_xpm,&empty_loadgraph[3*(((BOX_SIZE-1)/2-19)+15*BOX_SIZE)],BOX_SIZE,
                  18,0,1,2,graph_digit_color);
    draw_from_xpm(ofmspct_xpm,&empty_loadgraph[3*(((BOX_SIZE-1)/2   )+15*BOX_SIZE)],BOX_SIZE,
                  18,0,1,2,graph_digit_color);
    draw_from_xpm(ofmspct_xpm,&empty_loadgraph[3*(((BOX_SIZE-1)/2+19)+15*BOX_SIZE)],BOX_SIZE,
                  18,0,1,2,graph_digit_color);

    r = GET_RED(graph_digit_color);
    g = GET_GRN(graph_digit_color);
    b = GET_BLU(graph_digit_color);

    for (xx = 2; xx < BOX_SIZE - 2; xx++) {
        empty_memgraph[3*(xx + 20 * BOX_SIZE)  ] = r;
        empty_memgraph[3*(xx + 20 * BOX_SIZE)+1] = g;
        empty_memgraph[3*(xx + 20 * BOX_SIZE)+2] = b;
        empty_memgraph[3*(xx + (BOX_SIZE-3) * BOX_SIZE)  ] = r;
        empty_memgraph[3*(xx + (BOX_SIZE-3) * BOX_SIZE)+1] = g;
        empty_memgraph[3*(xx + (BOX_SIZE-3) * BOX_SIZE)+2] = b;
        empty_loadgraph[3*(xx + 18 * BOX_SIZE)  ] = r;
        empty_loadgraph[3*(xx + 18 * BOX_SIZE)+1] = g;
        empty_loadgraph[3*(xx + 18 * BOX_SIZE)+2] = b;
        empty_loadgraph[3*(xx + (BOX_SIZE-3) * BOX_SIZE)  ] = r;
        empty_loadgraph[3*(xx + (BOX_SIZE-3) * BOX_SIZE)+1] = g;
        empty_loadgraph[3*(xx + (BOX_SIZE-3) * BOX_SIZE)+2] = b;
    }

    r = GET_RED(graph_field);
    g = GET_GRN(graph_field);
    b = GET_BLU(graph_field);

    for (yy = 22; yy < BOX_SIZE - 4; yy++)
        for (xx = 2; xx < BOX_SIZE - 2; xx++) {
            empty_memgraph[3*(xx+yy*BOX_SIZE)  ] = r;
            empty_memgraph[3*(xx+yy*BOX_SIZE)+1] = g;
            empty_memgraph[3*(xx+yy*BOX_SIZE)+2] = b;
        }

    for (yy = 20; yy < BOX_SIZE - 4; yy++)
        for (xx = 2; xx < BOX_SIZE - 2; xx++) {
            empty_loadgraph[3*(xx+yy*BOX_SIZE)  ] = r;
            empty_loadgraph[3*(xx+yy*BOX_SIZE)+1] = g;
            empty_loadgraph[3*(xx+yy*BOX_SIZE)+2] = b;
        }

    r = GET_RED(graph_grid);
    g = GET_GRN(graph_grid);
    b = GET_BLU(graph_grid);

    for (yy = 1; yy < 4; yy++)
        for (xx = 2; xx < BOX_SIZE - 2; xx++) {
            empty_memgraph[3*(xx+(22+yy*(BOX_SIZE-4-22)/4)*BOX_SIZE)  ] = r;
            empty_memgraph[3*(xx+(22+yy*(BOX_SIZE-4-22)/4)*BOX_SIZE)+1] = g;
            empty_memgraph[3*(xx+(22+yy*(BOX_SIZE-4-22)/4)*BOX_SIZE)+2] = b;
            empty_loadgraph[3*(xx+(20+yy*(BOX_SIZE-4-20)/4)*BOX_SIZE)  ] = r;
            empty_loadgraph[3*(xx+(20+yy*(BOX_SIZE-4-20)/4)*BOX_SIZE)+1] = g;
            empty_loadgraph[3*(xx+(20+yy*(BOX_SIZE-4-20)/4)*BOX_SIZE)+2] = b;
        }

    for (xx = BOX_SIZE - 3 - 7; xx > 2; xx -= 8)
        for (yy = 22; yy < BOX_SIZE - 4; yy++) {
            empty_memgraph[3*(xx+yy*BOX_SIZE)  ] = r;
            empty_memgraph[3*(xx+yy*BOX_SIZE)+1] = g;
            empty_memgraph[3*(xx+yy*BOX_SIZE)+2] = b;
        }
    for (xx = BOX_SIZE - 3 - 7; xx > 2; xx -= 8)
        for (yy = 20; yy < BOX_SIZE - 4; yy++) {
            empty_loadgraph[3*(xx+yy*BOX_SIZE)  ] = r;
            empty_loadgraph[3*(xx+yy*BOX_SIZE)+1] = g;
            empty_loadgraph[3*(xx+yy*BOX_SIZE)+2] = b;
        }
}

void get_memory_load_percentage(void) {
    system_memory();
    bm.mem_percent = (100 * bm.mem_used) / bm.mem_max;

    if (bm.swap_max != 0) {
        bm.swap_percent = (100 * bm.swap_used) / bm.swap_max;
    } else {
        bm.swap_percent = 0;
    }
}
