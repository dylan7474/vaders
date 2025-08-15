#include <SDL2/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

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
#define ALIEN_COLS 8
#define ALIEN_WIDTH 40
#define ALIEN_HEIGHT 20
#define ALIEN_H_SPACING 20
#define ALIEN_V_SPACING 20
#define ALIEN_SPEED 2
#define ALIEN_STEP_DOWN 20
#define ALIEN_COUNT (ALIEN_ROWS * ALIEN_COLS)

#define ALIEN_BMP_W 10
#define ALIEN_BMP_H 5
#define SHIP_BMP_W 12
#define SHIP_BMP_H 4

/* Config */
static const SDL_Color COLOR_PLAYER        = {0, 255, 255, 255};
static const SDL_Color COLOR_PLAYER_BULLET = {255, 255, 255, 255};
static const SDL_Color COLOR_ALIEN         = {0, 255, 0, 255};
static const SDL_Color COLOR_ALIEN_BULLET  = {255, 255, 0, 255};
static const SDL_Color COLOR_HUD           = {255, 255, 255, 255};

#define SHAKE_DURATION 10
#define SHAKE_MAG 3
#define PARTICLE_MAX 256
#define PARTICLE_LIFETIME 300

/* Game state */
static SDL_Rect ship;
static SDL_Rect player_bullets[MAX_BULLETS];
static SDL_Rect alien_bullets[MAX_BULLETS];
static int player_bullet_count = 0;
static int alien_bullet_count = 0;
static SDL_Rect aliens[ALIEN_COUNT];
static float alien_fx[ALIEN_COUNT];
static int alien_alive[ALIEN_COUNT];
static int alien_direction = 1;
static int score = 0;
static int lives = 3;
static int wave = 1;
static float alien_base_speed = ALIEN_SPEED;
static int alien_fire_interval = 1500;
static int alien_fire_timer = 1500;
static int invuln_timer = 0;
static int wave_clear_timer = -1;
static int active = 1;
static int alien_flash[ALIEN_COUNT];
static int shake_timer = 0;
static int shake_x = 0, shake_y = 0;

typedef struct {
    float x, y;
    float vx, vy;
    int life;
    int active;
} Particle;

static Particle particles[PARTICLE_MAX];
static int muzzle_timer = 0;

/* Audio */
typedef enum { WAVE_SINE, WAVE_SQUARE, WAVE_NOISE } Waveform;

typedef struct {
    double attack;   /* seconds */
    double decay;    /* seconds */
    double sustain;  /* seconds */
    double release;  /* seconds */
    double sustain_level;
} ADSR;

typedef struct {
    int active;
    double freq;
    double phase;
    double t;
    double total;
    Waveform wave;
    ADSR env;
} ActiveSound;

#define MAX_ACTIVE_SOUNDS 32
static ActiveSound sounds[MAX_ACTIVE_SOUNDS];

typedef struct {
    double freq;
    int dur_ms;
    Waveform wave;
    ADSR env;
    int delay_ms;
} PendingSound;

#define MAX_PENDING_SOUNDS 64
static PendingSound pending_sounds[MAX_PENDING_SOUNDS];
static int pending_count = 0;

typedef struct {
    SDL_AudioDeviceID device;
    int freq;
} AudioData;

static AudioData audio = {0};

typedef enum {
    SND_PLAYER_SHOT,
    SND_ALIEN_HIT,
    SND_ALIEN_SHOT,
    SND_WAVE_CLEAR,
    SND_PLAYER_HIT
} SoundEvent;

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

void draw_bitmap(SDL_Renderer *renderer, int x, int y, int scale,
                 const uint8_t *bitmap, int w, int h) {
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            if (bitmap[row * w + col]) {
                SDL_Rect px = {x + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(renderer, &px);
            }
        }
    }
}

static const uint8_t alien_bitmaps[ALIEN_ROWS][2][ALIEN_BMP_W * ALIEN_BMP_H] = {
    {
        { 0,0,1,1,1,1,1,1,0,0,
          0,1,1,0,0,0,0,1,1,0,
          1,1,1,1,1,1,1,1,1,1,
          1,0,1,1,1,1,1,1,0,1,
          0,0,1,0,0,0,0,1,0,0 },
        { 0,0,1,1,1,1,1,1,0,0,
          1,1,1,0,0,0,0,1,1,1,
          1,1,1,1,1,1,1,1,1,1,
          0,1,0,1,1,1,1,0,1,0,
          1,0,0,0,0,0,0,0,0,1 }
    },
    {
        { 0,0,1,1,1,1,1,1,0,0,
          0,1,0,0,1,1,0,0,1,0,
          1,1,1,1,1,1,1,1,1,1,
          0,1,1,0,0,0,0,1,1,0,
          1,0,0,1,1,1,1,0,0,1 },
        { 0,0,1,1,1,1,1,1,0,0,
          1,0,0,0,1,1,0,0,0,1,
          1,1,1,1,1,1,1,1,1,1,
          0,1,0,0,0,0,0,0,1,0,
          1,0,1,1,0,0,1,1,0,1 }
    },
    {
        { 0,0,0,1,1,1,1,0,0,0,
          0,0,1,1,1,1,1,1,0,0,
          0,1,1,1,1,1,1,1,1,0,
          1,1,0,1,1,1,1,0,1,1,
          0,1,0,0,0,0,0,0,1,0 },
        { 0,0,0,1,1,1,1,0,0,0,
          0,1,1,1,1,1,1,1,1,0,
          1,1,0,1,1,1,1,0,1,1,
          0,1,1,0,0,0,0,1,1,0,
          1,0,0,0,0,0,0,0,0,1 }
    }
};

static const uint8_t ship_bitmap[SHIP_BMP_W * SHIP_BMP_H] = {
    0,0,0,0,0,1,1,1,0,0,0,0,
    0,0,1,1,1,1,1,1,1,1,0,0,
    0,1,1,1,1,1,1,1,1,1,1,0,
    1,1,1,1,1,1,1,1,1,1,1,1
};

/* -------------------- Audio -------------------- */

static double envelope_amp(ActiveSound *s) {
    double t = s->t;
    double a = s->env.attack;
    double d = s->env.decay;
    double sus = s->env.sustain;
    double r = s->env.release;
    double level = s->env.sustain_level;
    if (t < a) return t / a;
    if (t < a + d) return 1.0 - (1.0 - level) * (t - a) / d;
    if (t < a + d + sus) return level;
    if (t < s->total) return level * (1.0 - (t - (a + d + sus)) / r);
    s->active = 0;
    return 0.0;
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    Sint16 *buffer = (Sint16 *)stream;
    int length = len / 2;
    for (int i = 0; i < length; ++i) {
        double sample = 0.0;
        for (int s = 0; s < MAX_ACTIVE_SOUNDS; ++s) {
            if (!sounds[s].active) continue;
            ActiveSound *as = &sounds[s];
            double amp = envelope_amp(as);
            double val = 0.0;
            switch (as->wave) {
                case WAVE_SINE:   val = sin(as->phase); break;
                case WAVE_SQUARE: val = sin(as->phase) > 0 ? 1.0 : -1.0; break;
                case WAVE_NOISE:  val = ((rand() % 20001) / 10000.0) - 1.0; break;
            }
            sample += val * amp;
            as->phase += 2.0 * M_PI * as->freq / audio.freq;
            as->t += 1.0 / audio.freq;
        }
        if (sample > 1.0) sample = 1.0;
        if (sample < -1.0) sample = -1.0;
        buffer[i] = (Sint16)(sample * 3000);
    }
}

void play_beep(double freq, int dur_ms, Waveform wave, ADSR env) {
    SDL_LockAudioDevice(audio.device);
    env.attack /= 1000.0;
    env.decay  /= 1000.0;
    env.release/= 1000.0;
    if (env.attack < 0) env.attack = 0;
    if (env.decay < 0) env.decay = 0;
    if (env.release < 0) env.release = 0;
    env.sustain = (dur_ms / 1000.0) - (env.attack + env.decay + env.release);
    if (env.sustain < 0) env.sustain = 0;
    double total = env.attack + env.decay + env.sustain + env.release;
    for (int i = 0; i < MAX_ACTIVE_SOUNDS; ++i) {
        if (!sounds[i].active) {
            sounds[i].active = 1;
            sounds[i].freq = freq;
            sounds[i].phase = 0;
            sounds[i].t = 0;
            sounds[i].wave = wave;
            sounds[i].env = env;
            sounds[i].total = total;
            break;
        }
    }
    SDL_UnlockAudioDevice(audio.device);
    SDL_PauseAudioDevice(audio.device, 0);
}

void schedule_beep(double freq, int dur_ms, Waveform wave, ADSR env, int delay_ms) {
    if (pending_count >= MAX_PENDING_SOUNDS) return;
    pending_sounds[pending_count++] = (PendingSound){freq, dur_ms, wave, env, delay_ms};
}

void update_sounds(int dt_ms) {
    for (int i = 0; i < pending_count; ) {
        pending_sounds[i].delay_ms -= dt_ms;
        if (pending_sounds[i].delay_ms <= 0) {
            play_beep(pending_sounds[i].freq, pending_sounds[i].dur_ms,
                     pending_sounds[i].wave, pending_sounds[i].env);
            pending_sounds[i] = pending_sounds[--pending_count];
        } else {
            ++i;
        }
    }
}

void enqueue_sound(SoundEvent e) {
    ADSR env;
    switch (e) {
        case SND_PLAYER_SHOT:
            env = (ADSR){10, 40, 0, 40, 0.6};
            play_beep(880.0, 120, WAVE_SINE, env);
            break;
        case SND_ALIEN_HIT:
            env = (ADSR){10, 150, 0, 150, 0.5};
            play_beep(220.0, 300, WAVE_SQUARE, env);
            env = (ADSR){5, 60, 0, 60, 0.5};
            play_beep(0.0, 120, WAVE_NOISE, env);
            break;
        case SND_ALIEN_SHOT:
            env = (ADSR){5, 30, 0, 30, 0.6};
            play_beep(660.0, 80, WAVE_SINE, env);
            break;
        case SND_PLAYER_HIT:
            env = (ADSR){10, 100, 0, 100, 0.6};
            play_beep(180.0, 250, WAVE_SINE, env);
            break;
        case SND_WAVE_CLEAR:
            env = (ADSR){5, 50, 0, 50, 0.6};
            schedule_beep(440.0, 120, WAVE_SINE, env, 0);
            schedule_beep(660.0, 120, WAVE_SINE, env, 150);
            schedule_beep(880.0, 120, WAVE_SINE, env, 300);
            break;
    }
}

/* -------------------- Game Helpers -------------------- */

void init_wave(int wave_number) {
    player_bullet_count = 0;
    alien_bullet_count = 0;
    for (int r = 0; r < ALIEN_ROWS; ++r) {
        for (int c = 0; c < ALIEN_COLS; ++c) {
            int idx = r * ALIEN_COLS + c;
            float ax = 100 + c * (ALIEN_WIDTH + ALIEN_H_SPACING);
            float ay = 50 + r * (ALIEN_HEIGHT + ALIEN_V_SPACING);
            alien_fx[idx] = ax;
            aliens[idx] = (SDL_Rect){(int)ax, (int)ay, ALIEN_WIDTH, ALIEN_HEIGHT};
            alien_alive[idx] = 1;
        }
    }
    alien_direction = 1;
    alien_base_speed = ALIEN_SPEED * powf(1.1f, wave_number - 1);
    if (alien_base_speed > 8.0f) alien_base_speed = 8.0f;
    alien_fire_interval = (int)(1500 / powf(1.1f, wave_number - 1));
    if (alien_fire_interval < 400) alien_fire_interval = 400;
    alien_fire_timer = alien_fire_interval;
    for (int i = 0; i < ALIEN_COUNT; ++i) alien_flash[i] = 0;
    for (int i = 0; i < PARTICLE_MAX; ++i) particles[i].active = 0;
    muzzle_timer = 0;
    shake_timer = 0;
}

void spawn_alien_bullet(SDL_Rect from) {
    if (alien_bullet_count >= MAX_BULLETS) return;
    alien_bullets[alien_bullet_count++] =
        (SDL_Rect){from.x + from.w / 2 - BULLET_WIDTH / 2, from.y + from.h, BULLET_WIDTH, BULLET_HEIGHT};
    enqueue_sound(SND_ALIEN_SHOT);
}

void spawn_particles(int x, int y) {
    int n = 12 + rand() % 9;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < PARTICLE_MAX; ++j) {
            if (!particles[j].active) {
                float angle = (float)rand() / RAND_MAX * 2.0f * (float)M_PI;
                float speed = 50.0f + rand() % 100; /* px per second */
                particles[j].active = 1;
                particles[j].life = PARTICLE_LIFETIME;
                particles[j].x = (float)x;
                particles[j].y = (float)y;
                particles[j].vx = cosf(angle) * speed;
                particles[j].vy = sinf(angle) * speed;
                break;
            }
        }
    }
}

void update_particles(int dt) {
    for (int i = 0; i < PARTICLE_MAX; ++i) {
        if (!particles[i].active) continue;
        particles[i].life -= dt;
        if (particles[i].life <= 0) {
            particles[i].active = 0;
            continue;
        }
        particles[i].x += particles[i].vx * dt / 1000.0f;
        particles[i].y += particles[i].vy * dt / 1000.0f;
    }
}

void draw_particles(SDL_Renderer *renderer) {
    for (int i = 0; i < PARTICLE_MAX; ++i) {
        if (!particles[i].active) continue;
        Uint8 alpha = (Uint8)(255.0f * particles[i].life / PARTICLE_LIFETIME);
        SDL_SetRenderDrawColor(renderer, COLOR_ALIEN.r, COLOR_ALIEN.g, COLOR_ALIEN.b, alpha);
        SDL_Rect r = {(int)particles[i].x + shake_x, (int)particles[i].y + shake_y, 2, 2};
        SDL_RenderFillRect(renderer, &r);
    }
}

void check_collisions(void) {
    for (int i = 0; i < player_bullet_count;) {
        player_bullets[i].y -= BULLET_SPEED;
        int hit = -1;
        for (int a = 0; a < ALIEN_COUNT; ++a) {
            if (alien_alive[a] && SDL_HasIntersection(&player_bullets[i], &aliens[a])) { hit = a; break; }
        }
        if (hit != -1) {
            SDL_Rect a = aliens[hit];
            alien_alive[hit] = 0;
            alien_flash[hit] = 50;
            spawn_particles(a.x + a.w / 2, a.y + a.h / 2);
            shake_timer = SHAKE_DURATION;
            score += 10;
            player_bullets[i] = player_bullets[--player_bullet_count];
            enqueue_sound(SND_ALIEN_HIT);
            continue;
        }
        if (player_bullets[i].y + player_bullets[i].h < 0) {
            player_bullets[i] = player_bullets[--player_bullet_count];
        } else {
            ++i;
        }
    }

    for (int i = 0; i < alien_bullet_count;) {
        alien_bullets[i].y += BULLET_SPEED;
        if (alien_bullets[i].y > HEIGHT) {
            alien_bullets[i] = alien_bullets[--alien_bullet_count];
            continue;
        }
        if (invuln_timer <= 0 && SDL_HasIntersection(&alien_bullets[i], &ship)) {
            alien_bullets[i] = alien_bullets[--alien_bullet_count];
            lives--;
            invuln_timer = 1000;
            enqueue_sound(SND_PLAYER_HIT);
            if (lives <= 0) active = 0;
            continue;
        }
        ++i;
    }

    for (int i = 0; i < ALIEN_COUNT; ++i) {
        if (alien_alive[i] && aliens[i].y + aliens[i].h >= ship.y) {
            active = 0;
            break;
        }
    }
}

int count_active_player_bullets(void) {
    int count = 0;
    for (int i = 0; i < player_bullet_count; ++i) {
        if (player_bullets[i].y + player_bullets[i].h >= 0) {
            count++;
        }
    }
    return count;
}

void draw_hud(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, COLOR_HUD.r, COLOR_HUD.g, COLOR_HUD.b, COLOR_HUD.a);
    int scale = 2;
    int y = 10 + shake_y;
    int x = 10 + shake_x;
    draw_text_block(renderer, x, y, scale, "SCORE:");
    x += text_width_block("SCORE:", scale) + 2;
    draw_number(renderer, x, y, scale, score);

    x = 250 + shake_x;
    draw_text_block(renderer, x, y, scale, "LIVES:");
    x += text_width_block("LIVES:", scale) + 2;
    draw_number(renderer, x, y, scale, lives);

    x = 450 + shake_x;
    draw_text_block(renderer, x, y, scale, "WAVE:");
    x += text_width_block("WAVE:", scale) + 2;
    draw_number(renderer, x, y, scale, wave);
}

void reset_game(void) {
    ship = (SDL_Rect){(WIDTH - SHIP_WIDTH) / 2, HEIGHT - SHIP_HEIGHT - 10, SHIP_WIDTH, SHIP_HEIGHT};
    score = 0;
    lives = 3;
    wave = 1;
    invuln_timer = 0;
    active = 1;
    init_wave(1);
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
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = audio_callback;

    audio.device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio.device == 0) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
    } else {
        audio.freq = have.freq;
    }

    srand((unsigned int)SDL_GetTicks());
    reset_game();

    int running = 1;
    Uint32 last = SDL_GetTicks();
    while (running) {
        Uint32 now = SDL_GetTicks();
        int dt = (int)(now - last);
        last = now;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE) {
                    running = 0;
                } else if (key == SDLK_SPACE && active) {
                    if (count_active_player_bullets() < 3 && player_bullet_count < MAX_BULLETS) {
                        player_bullets[player_bullet_count++] =
                            (SDL_Rect){ship.x + SHIP_WIDTH / 2 - BULLET_WIDTH / 2,
                                       ship.y - BULLET_HEIGHT, BULLET_WIDTH, BULLET_HEIGHT};
                        muzzle_timer = 50;
                        enqueue_sound(SND_PLAYER_SHOT);
                    }
                } else if (key == SDLK_r && !active) {
                    reset_game();
                }
            }
        }

        update_sounds(dt);

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
            float speed_multiplier = 1.0f + (ALIEN_COUNT - alive_count) * 0.02f;
            float move = alien_base_speed * speed_multiplier;

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

            alien_fire_timer -= dt;
            if (alien_fire_timer <= 0) {
                int cols[ALIEN_COLS];
                int colcount = 0;
                for (int c = 0; c < ALIEN_COLS; ++c) {
                    for (int r = ALIEN_ROWS - 1; r >= 0; --r) {
                        int idx = r * ALIEN_COLS + c;
                        if (alien_alive[idx]) { cols[colcount++] = c; break; }
                    }
                }
                if (colcount > 0) {
                    int col = cols[rand() % colcount];
                    for (int r = ALIEN_ROWS - 1; r >= 0; --r) {
                        int idx = r * ALIEN_COLS + col;
                        if (alien_alive[idx]) { spawn_alien_bullet(aliens[idx]); break; }
                    }
                }
                alien_fire_timer = alien_fire_interval;
            }

            check_collisions();

            if (invuln_timer > 0) invuln_timer -= dt;

            if (alive_count == 0 && wave_clear_timer < 0) {
                wave_clear_timer = 1500;
                enqueue_sound(SND_WAVE_CLEAR);
            }
            if (wave_clear_timer >= 0) {
                wave_clear_timer -= dt;
                if (wave_clear_timer <= 0) {
                    wave++;
                    init_wave(wave);
                }
            }
        }

        update_particles(dt);
        if (muzzle_timer > 0) muzzle_timer -= dt;
        if (shake_timer > 0) shake_timer -= dt;
        for (int i = 0; i < ALIEN_COUNT; ++i) {
            if (alien_flash[i] > 0) alien_flash[i] -= dt;
        }
        if (shake_timer > 0) {
            shake_x = (rand() % (SHAKE_MAG * 2 + 1)) - SHAKE_MAG;
            shake_y = (rand() % (SHAKE_MAG * 2 + 1)) - SHAKE_MAG;
        } else {
            shake_x = shake_y = 0;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        int alien_frame = (SDL_GetTicks() / 500) % 2;
        int alien_scale = ALIEN_WIDTH / ALIEN_BMP_W;
        for (int i = 0; i < ALIEN_COUNT; ++i) {
            if (alien_alive[i] || alien_flash[i] > 0) {
                if (alien_flash[i] > 0) {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, COLOR_ALIEN.r, COLOR_ALIEN.g, COLOR_ALIEN.b, 255);
                }
                int type = i / ALIEN_COLS;
                draw_bitmap(renderer, aliens[i].x + shake_x, aliens[i].y + shake_y, alien_scale,
                            alien_bitmaps[type][alien_frame], ALIEN_BMP_W, ALIEN_BMP_H);
            }
        }

        if (invuln_timer <= 0 || (SDL_GetTicks() / 100) % 2 == 0) {
            SDL_SetRenderDrawColor(renderer, COLOR_PLAYER.r, COLOR_PLAYER.g, COLOR_PLAYER.b, 255);
            int ship_scale = SHIP_WIDTH / SHIP_BMP_W;
            draw_bitmap(renderer, ship.x + shake_x, ship.y + shake_y, ship_scale, ship_bitmap,
                        SHIP_BMP_W, SHIP_BMP_H);
        }

        if (muzzle_timer > 0) {
            SDL_SetRenderDrawColor(renderer, COLOR_PLAYER_BULLET.r, COLOR_PLAYER_BULLET.g, COLOR_PLAYER_BULLET.b, 255);
            int cx = ship.x + SHIP_WIDTH / 2 + shake_x;
            int cy = ship.y + shake_y;
            SDL_Rect r1 = {cx - 1, cy - 8, 2, 8};
            SDL_Rect r2 = {cx - 4, cy - 4, 8, 2};
            SDL_RenderFillRect(renderer, &r1);
            SDL_RenderFillRect(renderer, &r2);
        }

        SDL_SetRenderDrawColor(renderer, COLOR_PLAYER_BULLET.r, COLOR_PLAYER_BULLET.g, COLOR_PLAYER_BULLET.b, 255);
        for (int i = 0; i < player_bullet_count; ++i) {
            SDL_Rect r = player_bullets[i];
            r.x += shake_x;
            r.y += shake_y;
            SDL_RenderFillRect(renderer, &r);
        }
        SDL_SetRenderDrawColor(renderer, COLOR_ALIEN_BULLET.r, COLOR_ALIEN_BULLET.g, COLOR_ALIEN_BULLET.b, 255);
        for (int i = 0; i < alien_bullet_count; ++i) {
            SDL_Rect r = alien_bullets[i];
            r.x += shake_x;
            r.y += shake_y;
            SDL_RenderFillRect(renderer, &r);
        }

        draw_particles(renderer);

        draw_hud(renderer);

        if (!active) {
            const char *msg = "GAME OVER - Press R to restart";
            int w = text_width_block(msg, 2);
            int x = (WIDTH - w) / 2 + shake_x;
            int y = HEIGHT / 2 - (7 * 2) / 2 + shake_y;
            SDL_SetRenderDrawColor(renderer, COLOR_HUD.r, COLOR_HUD.g, COLOR_HUD.b, 255);
            draw_text_block(renderer, x, y, 2, msg);
        }

        SDL_RenderPresent(renderer);

        Uint32 frame_time = SDL_GetTicks() - now;
        if (frame_time < 16) SDL_Delay(16 - frame_time);
    }

    if (audio.device) SDL_CloseAudioDevice(audio.device);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

