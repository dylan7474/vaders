#include <SDL2/SDL.h>

int main(void) {
    const int WIDTH = 800;
    const int HEIGHT = 600;
    const int SHIP_WIDTH = 60;
    const int SHIP_HEIGHT = 20;
    const int SHIP_SPEED = 5;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Space Invaders", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT, 0);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Rect ship = { (WIDTH - SHIP_WIDTH) / 2, HEIGHT - SHIP_HEIGHT - 10, SHIP_WIDTH, SHIP_HEIGHT };

    int running = 1;
    while (running) {
        Uint32 start = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = 0;
            }
        }

        const Uint8 *state = SDL_GetKeyboardState(NULL);
        if (state[SDL_SCANCODE_LEFT]) {
            ship.x -= SHIP_SPEED;
            if (ship.x < 0) ship.x = 0;
        }
        if (state[SDL_SCANCODE_RIGHT]) {
            ship.x += SHIP_SPEED;
            if (ship.x > WIDTH - SHIP_WIDTH) ship.x = WIDTH - SHIP_WIDTH;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &ship);

        SDL_RenderPresent(renderer);

        Uint32 frame_time = SDL_GetTicks() - start;
        if (frame_time < 16) {
            SDL_Delay(16 - frame_time);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
