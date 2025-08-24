#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
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
#define SHIP_SPEED 5
#define SHIP_WIDTH 120
#define SHIP_HEIGHT 40

#define BULLET_WIDTH 4
#define BULLET_HEIGHT 10
#define BULLET_SPEED 10
#define MAX_BULLETS 128

#define ALIEN_ROWS 3
#define ALIEN_COLS 8
#define ALIEN_WIDTH 60
#define ALIEN_HEIGHT 40
#define ALIEN_H_SPACING 20
#define ALIEN_V_SPACING 20
#define ALIEN_SPEED 1
#define ALIEN_STEP_DOWN 40
#define ALIEN_COUNT (ALIEN_ROWS * ALIEN_COLS)


/* Config */
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

/* Textures */
static SDL_Texture *tex_ship;
static SDL_Texture *tex_aliens[ALIEN_ROWS];

typedef struct {
    float x, y;
    float vx, vy;
    int life;
    int active;
} Particle;

static Particle particles[PARTICLE_MAX];
static int muzzle_timer = 0;

/* Audio */
static Mix_Chunk *snd_player_shot;
static Mix_Chunk *snd_alien_hit;
static Mix_Chunk *snd_alien_shot;
static Mix_Chunk *snd_wave_clear;
static Mix_Chunk *snd_player_hit;

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

static SDL_Texture *load_texture(SDL_Renderer *renderer, const char *path) {
    SDL_Surface *src = IMG_Load(path);
    if (!src) {
        SDL_Log("Failed to load %s: %s", path, IMG_GetError());
        return NULL;
    }
    SDL_Surface *surf = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(src);
    if (!surf) {
        SDL_Log("Failed to convert %s: %s", path, SDL_GetError());
        return NULL;
    }
    Uint32 *pixels = (Uint32 *)surf->pixels;
    int count = surf->w * surf->h;
    Uint32 transparent = SDL_MapRGBA(surf->format, 255, 255, 255, 0);
    for (int i = 0; i < count; ++i) {
        Uint8 r, g, b;
        SDL_GetRGB(pixels[i], surf->format, &r, &g, &b);
        if (r > 250 && g > 250 && b > 250) {
            pixels[i] = transparent;
        }
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
        SDL_Log("Failed to create texture from %s: %s", path, SDL_GetError());
    } else {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }
    SDL_FreeSurface(surf);
    return tex;
}


/* -------------------- Audio -------------------- */

static void enqueue_sound(SoundEvent e) {
    Mix_Chunk *chunk = NULL;
    switch (e) {
        case SND_PLAYER_SHOT: chunk = snd_player_shot; break;
        case SND_ALIEN_HIT:   chunk = snd_alien_hit; break;
        case SND_ALIEN_SHOT:  chunk = snd_alien_shot; break;
        case SND_WAVE_CLEAR:  chunk = snd_wave_clear; break;
        case SND_PLAYER_HIT:  chunk = snd_player_hit; break;
    }
    if (chunk) {
        Mix_PlayChannel(-1, chunk, 0);
    }
}

/* -------------------- Game Helpers -------------------- */

void init_wave(int wave_number) {
    player_bullet_count = 0;
    alien_bullet_count = 0;
    float start_x = (WIDTH - (ALIEN_COLS * ALIEN_WIDTH + (ALIEN_COLS - 1) * ALIEN_H_SPACING)) / 2.0f;
    float start_y = 50;
    for (int r = 0; r < ALIEN_ROWS; ++r) {
        for (int c = 0; c < ALIEN_COLS; ++c) {
            int idx = r * ALIEN_COLS + c;
            float ax = start_x + c * (ALIEN_WIDTH + ALIEN_H_SPACING);
            float ay = start_y + r * (ALIEN_HEIGHT + ALIEN_V_SPACING);
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
    ship = (SDL_Rect){(WIDTH - SHIP_WIDTH) / 2, HEIGHT - SHIP_HEIGHT - 10,
                      SHIP_WIDTH, SHIP_HEIGHT};
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
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (!(IMG_Init(IMG_INIT_JPG) & IMG_INIT_JPG)) {
        SDL_Log("Failed to initialize SDL_image: %s", IMG_GetError());
    }
    tex_ship = load_texture(renderer, "Icon-SpaceShip.jpeg");
    tex_aliens[0] = load_texture(renderer, "Icon-Alien1.jpeg");
    tex_aliens[1] = load_texture(renderer, "Icon-Alien2.jpeg");
    tex_aliens[2] = load_texture(renderer, "Icon-Alien3.jpeg");
    for (int i = 0; i < ALIEN_ROWS; ++i) {
        if (!tex_aliens[i]) {
            SDL_Log("Failed to load alien texture %d", i);
        }
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Log("Failed to open audio: %s", Mix_GetError());
    }
    snd_player_shot = Mix_LoadWAV("laser.wav");
    if (!snd_player_shot) SDL_Log("Failed to load laser.wav: %s", Mix_GetError());
    snd_alien_hit = Mix_LoadWAV("explosion.wav");
    if (!snd_alien_hit) SDL_Log("Failed to load explosion.wav: %s", Mix_GetError());
    snd_alien_shot = Mix_LoadWAV("blip.wav");
    if (!snd_alien_shot) SDL_Log("Failed to load blip.wav: %s", Mix_GetError());
    snd_wave_clear = Mix_LoadWAV("blip.wav");
    if (!snd_wave_clear) SDL_Log("Failed to load blip.wav: %s", Mix_GetError());
    snd_player_hit = Mix_LoadWAV("death.wav");
    if (!snd_player_hit) SDL_Log("Failed to load death.wav: %s", Mix_GetError());

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
                            (SDL_Rect){ship.x + ship.w / 2 - BULLET_WIDTH / 2,
                                       ship.y - BULLET_HEIGHT, BULLET_WIDTH, BULLET_HEIGHT};
                        muzzle_timer = 50;
                        enqueue_sound(SND_PLAYER_SHOT);
                    }
                } else if (key == SDLK_r && !active) {
                    reset_game();
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
                if (ship.x > WIDTH - ship.w) ship.x = WIDTH - ship.w;
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

        for (int i = 0; i < ALIEN_COUNT; ++i) {
            if (alien_alive[i] || alien_flash[i] > 0) {
                int type = i / ALIEN_COLS;
                SDL_Rect dst = {aliens[i].x + shake_x, aliens[i].y + shake_y, ALIEN_WIDTH, ALIEN_HEIGHT};
                SDL_RenderCopy(renderer, tex_aliens[type], NULL, &dst);
            }
        }

        if (invuln_timer <= 0 || (SDL_GetTicks() / 100) % 2 == 0) {
            SDL_Rect dst = {ship.x + shake_x, ship.y + shake_y, ship.w, ship.h};
            SDL_RenderCopy(renderer, tex_ship, NULL, &dst);
        }

        if (muzzle_timer > 0) {
            SDL_SetRenderDrawColor(renderer, COLOR_PLAYER_BULLET.r, COLOR_PLAYER_BULLET.g, COLOR_PLAYER_BULLET.b, 255);
            int cx = ship.x + ship.w / 2 + shake_x;
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

    Mix_FreeChunk(snd_player_shot);
    Mix_FreeChunk(snd_alien_hit);
    Mix_FreeChunk(snd_alien_shot);
    Mix_FreeChunk(snd_wave_clear);
    Mix_FreeChunk(snd_player_hit);
    Mix_CloseAudio();
    Mix_Quit();
    SDL_DestroyTexture(tex_ship);
    for (int i = 0; i < ALIEN_ROWS; ++i) SDL_DestroyTexture(tex_aliens[i]);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}

