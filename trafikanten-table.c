#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include <json/json.h>

#include "trafikanten.h"

#define MAX_CONF_SIZE 1024

#define LENGTH(array) (sizeof(array) / sizeof(array[0]))

static int running = 1;
static const SDL_Color bg = {0, 0, 0, 255};
static const SDL_Color fg = {255, 255, 255, 255};
static const int update_interval = 30;

static SDL_Surface *screen;
static TTF_Font *hfont;
static TTF_Font *rfont;
static char stations[64][64];
static int nstations;
static departure deps[256];
static departure adeps[64];
static departure bdeps[64];
static int anumdeps;
static int bnumdeps;
static int sw;
static int sh;
static char fontpath[256];
static int hfontsize;
static int hlineheight;
static int rfontsize;
static int rlineheight;
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
    departure *d = deps;
    int totnumdeps = 0;

    for(int i = 0; i < nstations; ++i) {
        int n = trafikanten_get_departures(d, LENGTH(deps) - totnumdeps, stations[i]);
        if(n == -1)
            err(1, "trafikanten_get_departures");

        d += n;
        totnumdeps += n;
    }

    qsort(deps, totnumdeps, sizeof(departure), depsort);

    anumdeps = 0;
    bnumdeps = 0;
    for(int i = 0; i < totnumdeps; ++i) {
        if(deps[i].direction == 1)
            memcpy(&adeps[anumdeps++], &deps[i], sizeof(departure));
        else if(deps[i].direction == 2)
            memcpy(&bdeps[bnumdeps++], &deps[i], sizeof(departure));
    }
}

static void
format_time(char *str, time_t dt) {
    char sign = '+';
    if(dt < 0) {
        sign = '-';
        dt = -dt;
    }

    int minutes = dt / 60;
    int seconds = (dt - minutes) % 60;

    if(odinmode)
        sprintf(str, "%02d min", minutes);
    else
        sprintf(str, "%c%02d:%02d", sign, minutes, seconds);
}

static SDL_Color
row_color(time_t dt) {
    static const int threshold = 20 * 60;

    float h = 1. / 3;
    if(dt < threshold)
        h *= ((float)dt / threshold);

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
draw_row(departure *dep, int y) {
    time_t now = time(NULL);
    if(now == -1)
        err(1, "time");

    int dt = dep->arrival - now;

    SDL_Color color = row_color(dt);

    char time[8];
    format_time(time, dt);
    draw_text(time, marginleft, y, rfont, color, 0);

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
        if(adeps[i].arrival - t < 60)
            continue;

        draw_row(&adeps[i], y);
        y += rlineheight;
    }

    draw_headline("Westbound", sh / 2);
    for(int i = 0, y = sh / 2 + hlineheight; y < sh - rlineheight && i < bnumdeps; ++i) {
        if(bdeps[i].arrival - t < 60)
            continue;

        draw_row(&bdeps[i], y);
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

    JSON *j = json_tokener_parse(buf);
    if(j == NULL)
        errx(1, "invalid configuration file");

    JSON *jfontpath = json_object_object_get(j, "FontPath");
    if(jfontpath == NULL)
        errx(1, "invalid or no FontPath");
    const char *tmpfontpath = json_object_get_string(jfontpath);
    strcpy(fontpath, tmpfontpath);
    json_object_put(jfontpath);

    JSON *jhfontsize = json_object_object_get(j, "HeadFontSize");
    if(jhfontsize == NULL)
        errx(1, "invalid or no HeadFontSize");
    hfontsize = json_object_get_int(jhfontsize);
    hlineheight = 1.2 * hfontsize;
    json_object_put(jhfontsize);

    JSON *jrfontsize = json_object_object_get(j, "RowFontSize");
    if(jrfontsize == NULL)
        errx(1, "invalid or no RowFontSize");
    rfontsize = json_object_get_int(jrfontsize);
    rlineheight = 1.2 * rfontsize;
    json_object_put(jrfontsize);

    JSON *jstations = json_object_object_get(j, "Stations");
    if(jstations == NULL)
        errx(1, "invalid or no Stations");

    JSON *jmarginleft = json_object_object_get(j, "MarginLeft");
    if(jmarginleft == NULL)
        errx(1, "invalid or no MarginLeft");
    marginleft = json_object_get_int(jmarginleft);
    json_object_put(jmarginleft);

    JSON *jodinmode = json_object_object_get(j, "OdinMode");
    if(jodinmode == NULL)
        errx(1, "invalid or no OdinMode");
    odinmode = json_object_get_boolean(jodinmode);
    json_object_put(jodinmode);

    int n = json_object_array_length(jstations);
    for(nstations = 0; nstations < n && nstations < LENGTH(stations); ++nstations) {
        JSON *jstation = json_object_array_get_idx(jstations, nstations);

        char const *station = json_object_get_string(jstation);
        strcpy(stations[nstations], station);

        json_object_put(jstation);
    }

    json_object_put(j);
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

int
main(int argc, char **argv) {
    if(argc != 2) {
        printf("usage: %s <configuration-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

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

        int currentTick = SDL_GetTicks();
        int sleep = 1000 - (currentTick - lastTick);
        if(sleep > 10)
            SDL_Delay(sleep);
    }

    SDL_Quit();
    return EXIT_SUCCESS;
}
