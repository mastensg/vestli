#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <sys/time.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include "json.h"
#include "trafikanten.h"

#define MAX_CONF_SIZE 1024
#define DEFAULT_HFONTSIZE 48
#define DEFAULT_RFONTSIZE 56
#define DEFAULT_LINEHEIGHT_RATIO 12 / 10

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

static int running = 1;
static const SDL_Color bg = {0, 0, 0, 255};
static const SDL_Color fg = {255, 255, 255, 255};
static const int update_interval = 10;

static SDL_Surface *screen;
static TTF_Font *hfont;
static TTF_Font *rfont;
static struct station stations[64];
static int nstations;
static departure deps[256];
static departure adeps[64];
static departure bdeps[64];
static int anumdeps;
static int bnumdeps;
static int sw;
static int sh;
static char fontpath[256];
static int hfontsize = DEFAULT_HFONTSIZE;
static int hlineheight = DEFAULT_HFONTSIZE * DEFAULT_LINEHEIGHT_RATIO;
static int rfontsize = DEFAULT_RFONTSIZE;
static int rlineheight = DEFAULT_RFONTSIZE * DEFAULT_LINEHEIGHT_RATIO;
static int marginleft;
static int odinmode;

static int
depsort(const void *a, const void *b) {
    departure *depa = (departure *) a;
    departure *depb = (departure *) b;

    return depa->arrival - depb->arrival;
}

static void
update_rows(void) {
    static int station = 0;
    static int numdeps = 0;

    int i;
    for(i = 0; i < numdeps;) {
        if(deps[i].station == &stations[station])
            deps[i] = deps[--numdeps];
        else
            ++i;
    }

    int n = trafikanten_get_departures(&deps[numdeps], ARRAY_SIZE(deps) - numdeps, &stations[station]);
    if(n == -1)
        err(1, "trafikanten_get_departures");

    numdeps += n;

    qsort(deps, numdeps, sizeof(departure), depsort);

    anumdeps = 0;
    bnumdeps = 0;
    for(int i = 0; i < numdeps; ++i) {
        if(deps[i].direction == 1)
            adeps[anumdeps++] = deps[i];
        else if(deps[i].direction == 2)
            bdeps[bnumdeps++] = deps[i];
    }

    station = (station + 1) % nstations;
}

static void
format_time(char *str, time_t dt) {
    assert (dt >= 0);

    int minutes = dt / 60;
    int seconds = dt % 60;

    if(odinmode)
        sprintf(str, "%d min", minutes);
    else
        sprintf(str, "%2d:%02d", minutes, seconds);
}

static SDL_Color
row_color(time_t dt, time_t min_time) {
    time_t max_time = 3 * min_time;

    assert (dt >= min_time);
    dt -= min_time;

    float h = 1. / 3;

    if(dt < max_time)
        h *= ((float)dt / max_time);

    float r, g;

    if(h < 1. / 6) {
        r = 1;
        g = (h - 0. / 6) * 6;
    } else {
        r = 1 - (h - 1. / 6) * 6;
        g = 1;
    }

    SDL_Color c = {255 * r, 255 * g, 0};
    return c;
}

static int
time_width(void) {
    static int result = 0;
    if(result)
        return result;
    SDL_Color dummy_color;
    SDL_Surface *text = TTF_RenderUTF8_Shaded(rfont, "00:00", dummy_color, dummy_color);
    result = text->w;
    SDL_FreeSurface(text);
    return result;
}

static void
draw_text(char *str, int x, int y, TTF_Font *font, SDL_Color color, int rightalign) {
    SDL_Surface *text = TTF_RenderUTF8_Shaded(font, str, color, bg);

    SDL_Rect pos = {x, y};
    if(rightalign)
        pos.x -= text->w;

    SDL_BlitSurface(text, NULL, screen, &pos);
    SDL_FreeSurface(text);
}

static void
draw_clock(void) {
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    if(tmp == NULL)
        err(1, "localtime");

    char str[20];
    if(strftime(str, sizeof(str), "%H:%M:%S", tmp) == 0)
        err(1, "strftime");

    draw_text(str, sw, 0, hfont, fg, 1);
}

static void
draw_headline(char *str, int y) {
    draw_text(str, marginleft, y, hfont, fg, 0);
}

static void
draw_row(departure *dep, int y, time_t now) {
    int dt = dep->arrival - now;

    SDL_Color color = row_color(dt, dep->station->mintime);

    char time[8];
    format_time(time, dt);
    if(odinmode)
        draw_text(time, marginleft, y, rfont, color, 0);
    else
        draw_text(time, marginleft + time_width(), y, rfont, color, 1);

    draw_text(dep->line, marginleft + 8 * rfontsize, y, rfont, color, 1);
    draw_text(dep->destination, marginleft + 9 * rfontsize, y, rfont, color, 0);
}

static void
draw(void) {
    SDL_FillRect(screen, &screen->clip_rect, 0);

    draw_clock();

    time_t t = time(NULL);

    draw_headline("Eastbound", 0);
    for(int i = 0, y = hlineheight; y < sh / 2 - rlineheight && i < anumdeps; ++i) {
        if(adeps[i].arrival - t < adeps[i].station->mintime)
            continue;

        draw_row(&adeps[i], y, t);
        y += rlineheight;
    }

    draw_headline("Westbound", sh / 2);
    for(int i = 0, y = sh / 2 + hlineheight; y < sh - rlineheight && i < bnumdeps; ++i) {
        if(bdeps[i].arrival - t < bdeps[i].station->mintime)
            continue;

        draw_row(&bdeps[i], y, t);
        y += rlineheight;
    }

    SDL_Flip(screen);
}

static void
configure(const char *path) {
    FILE *f = fopen(path, "r");
    if(f == NULL)
        err(1, "cannot open configuration file \"%s\"", path);

    char buf[MAX_CONF_SIZE + 1];
    int nmemb = fread(buf, 1, MAX_CONF_SIZE, f);
    if(ferror(f))
        err(1, "cannot read configure file \"%s\"", path);
    buf[nmemb] = 0;

    int ret = fclose(f);
    if(ret == -1)
        err(1, "cannot close configure file \"%s\"", path);

    struct json_value *j;

    j = json_decode(buf);
    if(j == NULL)
        (errno ? err : errx)(1, "json_decode of \"%s\" failed", path);

    if(j->type != json_object)
        errx(1, "\"%s\" is not a JSON object", path);

    for(struct json_node *n = j->v.object; n; n = n->next) {
        if(!strcmp(n->name, "FontPath") && n->value->type == json_string) {
            strcpy(fontpath, n->value->v.string);
        } else if(!strcmp(n->name, "HeadFontSize") && n->value->type == json_number) {
            hfontsize = (int)n->value->v.number;
            hlineheight = hfontsize * 12 / 10;
        } else if(!strcmp(n->name, "RowFontSize") && n->value->type == json_number) {
            rfontsize = (int)n->value->v.number;
            rlineheight = rfontsize * 12 / 10;
        } else if(!strcmp(n->name, "MarginLeft") && n->value->type == json_number) {
            marginleft = (int)n->value->v.number;
        } else if(!strcmp(n->name, "OdinMode") && n->value->type == json_boolean) {
            odinmode = n->value->v.boolean;
        } else if(!strcmp(n->name, "Stations") && n->value->type == json_array) {
            for(struct json_value *jstation = n->value->v.array; jstation; jstation = jstation->next, ++nstations) {
                struct station station;
                if(jstation->type != json_object)
                    errx(1, "station %d in \"%s\" is not a JSON object", nstations, path);

                memset(&station, 0, sizeof(station));

                for(struct json_node *m = jstation->v.object; m; m = m->next) {
                    if(!strcmp(m->name, "ID") && m->value->type == json_string)
                        strncpy(station.id, m->value->v.string, sizeof(station.id));
                    else if(!strcmp(m->name, "MinTime") && m->value->type == json_number)
                        station.mintime = (int)m->value->v.number;
                }

                if(!station.id[0])
                    errx(1, "missing ID for station %d in \"%s\"", nstations, path);

                stations[nstations] = station;
            }
        }
    }

    if (!fontpath[0])
        errx(1, "missing FontPath in \"%s\"", path);

    json_free(j);
}

static void
font_init(void) {
    if(TTF_Init() == -1)
        err(1, "cannot initialize font library");

    hfont = TTF_OpenFont(fontpath, hfontsize);
    if(hfont == NULL)
        err(1, "cannot load font \"%s\"", fontpath);

    rfont = TTF_OpenFont(fontpath, rfontsize);
    if(rfont == NULL)
        err(1, "cannot load font \"%s\"", fontpath);
}

static void
screen_init() {
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

    const SDL_VideoInfo *info = SDL_GetVideoInfo();
    if(info == NULL)
        err(1, "SDL_GetVideoInfo");

    sw = info->current_w;
    sh = info->current_h;

    screen = SDL_SetVideoMode(sw, sh, 0, SDL_RESIZABLE);
    if(!screen)
        err(1, "cannot initialize screen");

    SDL_ShowCursor(SDL_DISABLE);
}

static void
handle_events(void) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE)
                running = 0;

            break;
        case SDL_VIDEORESIZE:
            sw = event.resize.w;
            sh = event.resize.h;
            break;
        default:
            break;
        }
    }
}

static int program_argc;
static char **program_argv;

void
restart(int signal) {
    char **argv = calloc(program_argc + 1, sizeof *argv);
    memcpy(argv, program_argv, program_argc * sizeof *argv);

    extern char **environ;
    execve(BINDIR "/" PROGRAM_NAME, argv, environ);
}

int
main(int argc, char **argv) {
    if(argc != 2) {
        printf("usage: %s <configuration-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    program_argc = argc;
    program_argv = argv;
    signal(SIGHUP, restart);

    configure(argv[1]);
    font_init();
    screen_init();

    int last_update = SDL_GetTicks();
    update_rows();

    while(running) {
        int lastTick = SDL_GetTicks();

        if(lastTick - last_update > update_interval * 1000) {
            last_update = SDL_GetTicks();
            update_rows();
        }

        handle_events();
        draw();

        struct timeval tv;
        gettimeofday(&tv, 0);
        tv.tv_sec = 0;
        usleep(1000000 - tv.tv_usec);
    }

    SDL_Quit();
    return EXIT_SUCCESS;
}
