#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <curl/curl.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

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

static SDL_Surface *screen;
static TTF_Font *cfont;
static TTF_Font *hfont;
static TTF_Font *rfont;

static void
handle_events(void) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE)
                running = 0;

            break;
        default:
            break;
        }
    }
}

static void
draw_text(char *str, int x, int y, TTF_Font *font, SDL_Color color) {
    SDL_Surface *text = TTF_RenderText_Shaded(font, str, color, bg);

    SDL_Rect pos = {x, y};
    if(x == -1)
        pos.x = sw - text->w;

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

    draw_text(str, -1, 0, cfont, fg);
}

static void
draw_headline(char *str, int y) {
    draw_text(str, 0, y, hfont, fg);
}

static void
draw_row(char *time, char *num, char *name, int y) {
    draw_text(time, 0, y, rfont, fg);
    draw_text(num, 4 * rfontsize, y, rfont, fg);
    draw_text(name, 7 * rfontsize, y, rfont, fg);
}

static void
draw(void) {
    SDL_FillRect(screen, &screen->clip_rect, 0);

    draw_clock();

    draw_headline("Mot sentrum", 0);
    for(int y = hlineheight; y < sh / 2 - rlineheight; y += rlineheight)
        draw_row("00:04", "3", "Mortensrud", y);

    draw_headline("Fra sentrum", sh / 2);
    for(int y = sh / 2 + hlineheight; y < sh - rlineheight; y += rlineheight)
        draw_row("00:04", "3", "Mortensrud", y);

    SDL_Flip(screen);
}

static void
usage(void) {
    printf("usage: sdltable width, height\n");
    exit(EXIT_FAILURE);
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
screen_init(int argc, char **argv) {
    if(argc != 3)
        usage();

    sw = atoi(argv[1]);
    sh = atoi(argv[2]);

    screen = SDL_SetVideoMode(sw, sh, 0, 0);
    if(!screen)
        err(1, "Could not initialize screen");
}

typedef struct {
    char data[65536];
    size_t size;
} lolbuffer;

static size_t
memory(void *ptr, size_t size, size_t nmemb, void *data) {
    size_t realsize = nmemb * size;
    lolbuffer *buf = (lolbuffer *)data;

    if(buf->size + realsize > 65536)
        err(1, "not enough lol");

    memcpy(buf->data + buf->size, ptr, realsize);
    buf->size += realsize;
    buf->data[buf->size] = 0;

    printf("lol: %d\n", realsize);

    return realsize;
}

static void
trafikanten_get(void) {
    lolbuffer buf;
    memset(&buf, 0, sizeof(lolbuffer));

    CURL *curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, "http://www.sis.trafikanten.no:8088/xmlrtpi/dis/request?DISID=SN$03010360");
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, memory);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&buf);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "lol/1337");
    curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);

    puts(buf.data);
}

int
main(int argc, char **argv) {
    trafikanten_get();
    return 0;

    font_init();
    screen_init(argc, argv);

    while(running) {
        int lastTick = SDL_GetTicks();

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
