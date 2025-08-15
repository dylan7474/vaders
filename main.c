#include <SDL2/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WIDTH 800
#define HEIGHT 600
#define SHIP_WIDTH 60
#define SHIP_HEIGHT 20
#define SHIP_SPEED 5

#define BULLET_WIDTH 4
#define BULLET_HEIGHT 10
#define BULLET_SPEED 10
#define MAX_BULLETS 128

#define ALIEN_ROWS 3
#define ALIEN_COLS 5
#define ALIEN_WIDTH 40
#define ALIEN_HEIGHT 20
#define ALIEN_H_SPACING 20
#define ALIEN_V_SPACING 20
#define ALIEN_SPEED 2
#define ALIEN_STEP_DOWN 20
#define ALIEN_COUNT (ALIEN_ROWS * ALIEN_COLS)

typedef struct {
    char c;
    uint8_t rows[7];
} Glyph;

static const Glyph font[] = {
    {'A', {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'C', {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}},
    {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
    {'G', {0x0E,0x11,0x10,0x10,0x13,0x11,0x0E}},
    {'M', {0x11,0x1B,0x15,0x11,0x11,0x11,0x11}},
    {'O', {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'P', {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
    {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'S', {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E}},
    {'T', {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'V', {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
};

static const int digit_segments[10] = {
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F  /* 9 */
};

void draw_digit_7seg(SDL_Renderer *renderer, int x, int y, int scale, int digit) {
    if (digit < 0 || digit > 9) return;
    int pattern = digit_segments[digit];
    SDL_Rect seg[7] = {
        {x + scale,     y,             2*scale, scale},      /* a */
        {x + 3*scale,   y + scale,     scale,   2*scale},    /* b */
        {x + 3*scale,   y + 3*scale,   scale,   2*scale},    /* c */
        {x + scale,     y + 5*scale,   2*scale, scale},      /* d */
        {x,             y + 3*scale,   scale,   2*scale},    /* e */
        {x,             y + scale,     scale,   2*scale},    /* f */
        {x + scale,     y + 3*scale,   2*scale, scale}       /* g */
    };
    for (int i = 0; i < 7; ++i) {
        if (pattern & (1 << i)) {
            SDL_RenderFillRect(renderer, &seg[i]);
        }
    }
}

void draw_number(SDL_Renderer *renderer, int x, int y, int scale, int value) {
    char buf[16];
    sprintf(buf, "%d", value);
    for (int i = 0; buf[i]; ++i) {
        draw_digit_7seg(renderer, x, y, scale, buf[i] - '0');
        x += 5 * scale;
    }
}

static const uint8_t* glyph_for(char c) {
    c = toupper((unsigned char)c);
    for (size_t i = 0; i < sizeof(font)/sizeof(font[0]); ++i) {
        if (font[i].c == c) return font[i].rows;
    }
    return NULL;
}

int text_width_block(const char *text, int scale) {
    size_t len = strlen(text);
    if (len == 0) return 0;
    return (int)((len * 6 - 1) * scale);
}

void draw_char_block(SDL_Renderer *renderer, int x, int y, int scale, char c) {
    if (c == ' ') return;  /* spacing handled by caller */
    if (c == '-') {
        SDL_Rect r = {x, y + 3*scale, 5*scale, scale};
        SDL_RenderFillRect(renderer, &r);
        return;
    }
    if (c == ':') {
        SDL_Rect r = {x + 2*scale, y + scale, scale, scale};
        SDL_RenderFillRect(renderer, &r);
        r.y = y + 4*scale;
        SDL_RenderFillRect(renderer, &r);
        return;
    }
    const uint8_t *rows = glyph_for(c);
    if (!rows) return;
    for (int r = 0; r < 7; ++r) {
        for (int col = 0; col < 5; ++col) {
            if (rows[r] & (1 << (4 - col))) {
                SDL_Rect px = {x + col*scale, y + r*scale, scale, scale};
                SDL_RenderFillRect(renderer, &px);
            }
        }
    }
}

void draw_text_block(SDL_Renderer *renderer, int x, int y, int scale, const char *text) {
    for (int i = 0; text[i]; ++i) {
        draw_char_block(renderer, x, y, scale, text[i]);
        x += 6 * scale;
    }
}

typedef struct {
    Uint32 samples_left;
    double phase;
    SDL_AudioDeviceID device;
    int freq;
    double tone;
} AudioData;

void audio_callback(void *userdata, Uint8 *stream, int len) {
    AudioData *data = (AudioData *)userdata;
    Sint16 *buffer = (Sint16 *)stream;
    int length = len / 2;
    for (int i = 0; i < length; ++i) {
        if (data->samples_left > 0) {
            buffer[i] = (Sint16)(sin(data->phase) * 3000);
            data->phase += 2.0 * M_PI * data->tone / data->freq;
            data->samples_left--;
        } else {
            buffer[i] = 0;
        }
    }
    if (data->samples_left == 0) {
        SDL_PauseAudioDevice(data->device, 1);
    }
}

void reset_game(SDL_Rect *ship, int *bullet_count,
                SDL_Rect *aliens, float *alien_fx, int *alien_alive,
                int *alien_direction, int *score) {
    *ship = (SDL_Rect){ (WIDTH - SHIP_WIDTH) / 2, HEIGHT - SHIP_HEIGHT - 10, SHIP_WIDTH, SHIP_HEIGHT };
    *bullet_count = 0;
    for (int r = 0; r < ALIEN_ROWS; ++r) {
        for (int c = 0; c < ALIEN_COLS; ++c) {
            int idx = r * ALIEN_COLS + c;
            float ax = 100 + c * (ALIEN_WIDTH + ALIEN_H_SPACING);
            float ay = 50 + r * (ALIEN_HEIGHT + ALIEN_V_SPACING);
            alien_fx[idx] = ax;
            aliens[idx] = (SDL_Rect){ (int)ax, (int)ay, ALIEN_WIDTH, ALIEN_HEIGHT };
            alien_alive[idx] = 1;
        }
    }
    *alien_direction = 1;
    *score = 0;
}

int main(void) {
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

    SDL_Rect ship;
    SDL_Rect bullets[MAX_BULLETS];
    int bullet_count;
    SDL_Rect aliens[ALIEN_COUNT];
    float alien_fx[ALIEN_COUNT];
    int alien_alive[ALIEN_COUNT];
    int alien_direction;
    int score;

    reset_game(&ship, &bullet_count, aliens, alien_fx, alien_alive, &alien_direction, &score);
    const int initial_alien_count = ALIEN_COUNT;

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
    int active = 1; /* game playing state */
    while (running) {
        Uint32 start = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE) {
                    running = 0;
                } else if (key == SDLK_SPACE && active) {
                    if (bullet_count < MAX_BULLETS) {
                        bullets[bullet_count++] = (SDL_Rect){ ship.x + SHIP_WIDTH / 2 - BULLET_WIDTH / 2,
                                                                ship.y - BULLET_HEIGHT, BULLET_WIDTH, BULLET_HEIGHT };
                        if (audio.device) {
                            SDL_LockAudioDevice(audio.device);
                            audio.samples_left = (Uint32)(audio.freq * 0.15);
                            audio.phase = 0;
                            audio.tone = 880.0;
                            SDL_UnlockAudioDevice(audio.device);
                            SDL_PauseAudioDevice(audio.device, 0);
                        }
                    }
                } else if (key == SDLK_r) {
                    reset_game(&ship, &bullet_count, aliens, alien_fx, alien_alive, &alien_direction, &score);
                    active = 1;
                }
            }
        }

        const Uint8 *state = SDL_GetKeyboardState(NULL);
        if (active) {
            if (state[SDL_SCANCODE_LEFT]) {
                ship.x -= SHIP_SPEED;
                if (ship.x < 0) ship.x = 0;
            }
            if (state[SDL_SCANCODE_RIGHT]) {
                ship.x += SHIP_SPEED;
                if (ship.x > WIDTH - SHIP_WIDTH) ship.x = WIDTH - SHIP_WIDTH;
            }

            int alive_count = 0;
            for (int i = 0; i < ALIEN_COUNT; ++i) {
                if (alien_alive[i]) alive_count++;
            }
            float speed_multiplier = 1.0f + (initial_alien_count - alive_count) * 0.02f;
            float move = ALIEN_SPEED * speed_multiplier;

            int edge_hit = 0;
            for (int i = 0; i < ALIEN_COUNT; ++i) {
                if (!alien_alive[i]) continue;
                alien_fx[i] += alien_direction * move;
                aliens[i].x = (int)alien_fx[i];
                if (aliens[i].x < 0 || aliens[i].x + aliens[i].w > WIDTH) {
                    edge_hit = 1;
                }
            }
            if (edge_hit) {
                for (int i = 0; i < ALIEN_COUNT; ++i) {
                    if (!alien_alive[i]) continue;
                    alien_fx[i] -= alien_direction * move;
                    aliens[i].x = (int)alien_fx[i];
                    aliens[i].y += ALIEN_STEP_DOWN;
                }
                alien_direction *= -1;
            }

            for (int i = 0; i < bullet_count; ) {
                bullets[i].y -= BULLET_SPEED;
                int hit = -1;
                for (int a = 0; a < ALIEN_COUNT; ++a) {
                    if (alien_alive[a] && SDL_HasIntersection(&bullets[i], &aliens[a])) { hit = a; break; }
                }
                if (hit != -1) {
                    alien_alive[hit] = 0;
                    score += 10;
                    bullets[i] = bullets[--bullet_count];
                    if (audio.device) {
                        SDL_LockAudioDevice(audio.device);
                        audio.samples_left = (Uint32)(audio.freq * 0.2);
                        audio.phase = 0;
                        audio.tone = 220.0;
                        SDL_UnlockAudioDevice(audio.device);
                        SDL_PauseAudioDevice(audio.device, 0);
                    }
                    continue;
                }
                if (bullets[i].y + bullets[i].h < 0) {
                    bullets[i] = bullets[--bullet_count];
                } else {
                    ++i;
                }
            }

            for (int i = 0; i < ALIEN_COUNT; ++i) {
                if (alien_alive[i] && aliens[i].y + aliens[i].h >= ship.y) {
                    active = 0;
                    break;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (int i = 0; i < ALIEN_COUNT; ++i) {
            if (alien_alive[i]) {
                SDL_RenderFillRect(renderer, &aliens[i]);
            }
        }
        SDL_RenderFillRect(renderer, &ship);
        for (int i = 0; i < bullet_count; ++i) {
            SDL_RenderFillRect(renderer, &bullets[i]);
        }

        draw_text_block(renderer, 10, 10, 2, "SCORE:");
        int score_x = 10 + text_width_block("SCORE:", 2) + 2;
        draw_number(renderer, score_x, 10, 2, score);

        if (!active) {
            const char *msg = "GAME OVER - Press R to restart";
            int w = text_width_block(msg, 2);
            int x = (WIDTH - w) / 2;
            int y = HEIGHT / 2 - (7*2)/2;
            draw_text_block(renderer, x, y, 2, msg);
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

