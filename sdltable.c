#include <err.h>
#include <stdio.h>

#include <SDL/SDL.h>

#define LENGTH(array)   (sizeof(array) / sizeof(array[0]))

#if 0
void
handle_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = false;
                lift = true;
                break;
            case SDL_KEYUP:
                lift = false;
                break;
            default:
                break;
        }
    }
}

SDL_Surface *
loadImage(const char *filename) {
    SDL_Surface *loaded_image = IMG_Load(filename);
    SDL_Surface *optimized_image;

    if (loaded_image)
        optimized_image = SDL_DisplayFormatAlpha(loaded_image);

    SDL_FreeSurface(loaded_image);
    return optimized_image;
}

void
paint(void) {
    int i;
    SDL_Rect level_line = {0, 0};

    SDL_FillRect(screen, &screen->clip_rect, SDL_MapRGB(screen->format, 0, 0, 0xff));
    for (i = level_elapsed; i < level_elapsed + window_width; ++i) {
        ++level_line.x;
        level_line.y = window_height - world[i];
        level_line.w = 1;
        level_line.h = world[i];
        SDL_FillRect(screen, &level_line, SDL_MapRGB(screen->format, 0, 0xff, 0));
    }
    SDL_BlitSurface(bumblebee, NULL, screen, &bumblebee_position);
    SDL_Flip(screen);
}

void
run(void) {
    while (running) {
        handle_events();
        update_gamestate();
        paint();
    }
}

void
update_gamestate(void) {
    Uint32 t = SDL_GetTicks();
    Uint32 dt = t - last_t;
    float acceleration;

    last_t = t;
    if ((accum_dt += dt) < max_dt)
        return;

    accum_dt = 0;
    acceleration = lift ? 1.3 : -1.7;
    velocity += acceleration;
    bumblebee_position.y -= 0.04 * velocity;

    if (bumblebee_position.y > window_height - world[level_elapsed + bumblebee_position.x]) {
        bumblebee_position.y = window_height / 2;
        velocity = 0;
        level_elapsed = 0;
    } else
        ++level_elapsed;
}
#endif

int
main(int argc, char *argv[]) {
    static const int sw = 100;
    static const int sh = 100;

    SDL_Surface *screen = SDL_SetVideoMode(sw, sh, 0, SDL_SWSURFACE | SDL_RESIZABLE);
    if(!screen)
        err(1, "Could not initialize screen");

    for(;;);

    return EXIT_SUCCESS;
}
