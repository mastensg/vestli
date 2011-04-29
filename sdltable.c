#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include "trafikanten.h"

#define LENGTH(array) (sizeof(array) / sizeof(array[0]))

static departure deps[256];
static departure adeps[64];
static departure bdeps[64];
static int anumdeps;
static int bnumdeps;
static int sw;
static int sh;
static int running = 1;
static const SDL_Color bg = {0, 0, 0, 255};
static const SDL_Color fg = {255, 255, 255, 255};
static const char *fontpath = "DejaVuSans-Bold.ttf";
static const int cfontsize = 32;
static const int clineheight = 1.2 * 32;
static const int hfontsize = 32;
static const int hlineheight = 1.2 * 32;
static const int rfontsize = 48;
static const int rlineheight = 1.2 * 48;
static const int update_interval = 5;

static SDL_Surface *screen;
static TTF_Font *cfont;
static TTF_Font *hfont;
static TTF_Font *rfont;

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

    static const char *stations[] = {"3012323", "3010370", "3012322"};

    for(int i = 0; i < LENGTH(stations); ++i) {
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
    SDL_Surface *text = TTF_RenderText_Shaded(font, str, color, bg);

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

    draw_text(str, sw, 0, cfont, fg, 1);
}

static void
draw_headline(char *str, int y) {
    draw_text(str, 0, y, hfont, fg, 0);
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
    draw_text(time, 0, y, rfont, color, 0);

    draw_text(dep->line, 8 * rfontsize, y, rfont, color, 1);
    draw_text(dep->destination, 9 * rfontsize, y, rfont, color, 0);
}

static void
draw(void) {
    SDL_FillRect(screen, &screen->clip_rect, 0);

    draw_clock();

    draw_headline("Mot sentrum", 0);

    for(int i = 0, y = hlineheight; y < sh / 2 - rlineheight && i < anumdeps; y += rlineheight, ++i)
        draw_row(&adeps[i], y);

    draw_headline("Fra sentrum", sh / 2);
    for(int i = 0, y = sh / 2 + hlineheight; y < sh - rlineheight && i < bnumdeps; y += rlineheight, ++i)
        draw_row(&bdeps[i], y);

    SDL_Flip(screen);
}

static void
font_init(void) {
    if(TTF_Init() == -1)
        err(1, "Could not initialize font library");

    cfont = TTF_OpenFont(fontpath, cfontsize);
    if(cfont == NULL)
        err(1, "Could not load font \"%s\"", fontpath);

    hfont = TTF_OpenFont(fontpath, hfontsize);
    if(hfont == NULL)
        err(1, "Could not load font \"%s\"", fontpath);

    rfont = TTF_OpenFont(fontpath, rfontsize);
    if(rfont == NULL)
        err(1, "Could not load font \"%s\"", fontpath);
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
        err(1, "Could not initialize screen");
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
        // 100 - ... to make ESC faster
        int sleep = 100 - (currentTick - lastTick);
        if(sleep > 10)
            SDL_Delay(sleep);
    }

    SDL_Quit();
    return EXIT_SUCCESS;
}
