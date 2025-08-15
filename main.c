#include <SDL2/SDL.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main(void) {
    const int WIDTH = 800;
    const int HEIGHT = 600;
    const int SHIP_WIDTH = 60;
    const int SHIP_HEIGHT = 20;
    const int SHIP_SPEED = 5;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
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

    const int BULLET_WIDTH = 4;
    const int BULLET_HEIGHT = 10;
    const int BULLET_SPEED = 10;
    const int MAX_BULLETS = 128;
    SDL_Rect bullets[MAX_BULLETS];
    int bullet_count = 0;

    typedef struct {
        Uint32 samples_left;
        double phase;
        SDL_AudioDeviceID device;
        int freq;
    } AudioData;

    void audio_callback(void *userdata, Uint8 *stream, int len) {
        AudioData *data = (AudioData *)userdata;
        Sint16 *buffer = (Sint16 *)stream;
        int length = len / 2;
        for (int i = 0; i < length; ++i) {
            if (data->samples_left > 0) {
                buffer[i] = (Sint16)(sin(data->phase) * 3000);
                data->phase += 2.0 * M_PI * 880.0 / data->freq;
                data->samples_left--;
            } else {
                buffer[i] = 0;
            }
        }
        if (data->samples_left == 0) {
            SDL_PauseAudioDevice(data->device, 1);
        }
    }

    AudioData audio = {0};
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = audio_callback;
    want.userdata = &audio;

    audio.device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio.device == 0) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
    } else {
        audio.freq = have.freq;
    }

    int running = 1;
    while (running) {
        Uint32 start = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    if (bullet_count < MAX_BULLETS) {
                        bullets[bullet_count++] = (SDL_Rect){ ship.x + SHIP_WIDTH / 2 - BULLET_WIDTH / 2,
                                                                ship.y - BULLET_HEIGHT, BULLET_WIDTH, BULLET_HEIGHT };
                        if (audio.device) {
                            SDL_LockAudioDevice(audio.device);
                            audio.samples_left = (Uint32)(audio.freq * 0.15);
                            audio.phase = 0;
                            SDL_UnlockAudioDevice(audio.device);
                            SDL_PauseAudioDevice(audio.device, 0);
                        }
                    }
                }
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

        for (int i = 0; i < bullet_count; ) {
            bullets[i].y -= BULLET_SPEED;
            if (bullets[i].y + bullets[i].h < 0) {
                bullets[i] = bullets[--bullet_count];
            } else {
                ++i;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &ship);
        for (int i = 0; i < bullet_count; ++i) {
            SDL_RenderFillRect(renderer, &bullets[i]);
        }

        SDL_RenderPresent(renderer);

        Uint32 frame_time = SDL_GetTicks() - start;
        if (frame_time < 16) {
            SDL_Delay(16 - frame_time);
        }
    }

    if (audio.device) {
        SDL_CloseAudioDevice(audio.device);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
