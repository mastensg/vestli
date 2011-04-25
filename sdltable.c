#include <err.h>
#include <stdio.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

static const int sw = 1024;
static const int sh = 768;
static int running = 1;

static SDL_Surface *screen;
static TTF_Font *font;

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
draw_table(int i) {
    static const SDL_Color bg = {0, 0, 0, 255};
    static const SDL_Color fg = {200, 200, 200, 255};

    char msg[20];
    sprintf(msg, "Now at %d.", i);
    SDL_Surface *text = TTF_RenderText_Shaded(font, msg, fg, bg);

    SDL_FillRect(screen, &screen->clip_rect, SDL_MapRGB(screen->format, 0, i % 0x100, 0xff));

    static SDL_Rect pos = {30, 40};
    SDL_BlitSurface(text, NULL, screen, &pos);

    SDL_Flip(screen);
}

static void
font_init(void) {
    if(TTF_Init() == -1)
        err(1, "Could not initialize font library");

    static const char *fontpath = "DejaVuSans.ttf";
    static const int fontsize = 48;

    font = TTF_OpenFont(fontpath, fontsize);
    if(font == NULL)
        err(1, "Could not load font \"%s\"", fontpath);
}

int
main(int argc, char *argv[]) {
    font_init();

    screen = SDL_SetVideoMode(sw, sh, 0, SDL_SWSURFACE | SDL_RESIZABLE);
    if(!screen)
        err(1, "Could not initialize screen");

    int i = 0;
    while(running) {
        handle_events();
        draw_table(i);
        ++i;
    }

    return EXIT_SUCCESS;
}
