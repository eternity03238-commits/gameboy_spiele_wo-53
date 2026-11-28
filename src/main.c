/* Gameboy - OSTWALD EDITION, Retro-Spiele fuer den Geschichtsvortrag
 * Wilhelm-Ostwald-Gymnasium Leipzig, Klasse 5/3
 * Gebaut mit GBDK-2020 fuer den monochromen Game Boy.
 */
#include <gb/gb.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <rand.h>
#include <gbdk/console.h>
#include "tiles.h"
#include "font5x7.h"
#include "logo.h"
#include "menuart.h"

/* ---------- kleine Helfer ---------- */

/* ---------- Sound: kleiner Melodie-Spieler, laeuft im VBlank-Interrupt ---------- */
/* Tonhoehe als Gameboy-Registerwert: 2048 - 131072 / Hz */
#define C4 1547
#define E4 1651
#define G4 1714
#define C5 1797
#define E5 1849
#define G5 1881
#define B5 1915
#define C6 1923
#define D6 1936
#define E6 1949
#define G6 1964
#define C7 1985
#define REST 0

typedef struct { uint16_t f; uint8_t len; } Note;
static const Note *volatile snd_ptr;
static volatile uint8_t snd_cnt, snd_env;

static const Note M_BOOT[]   = { {C6, 5}, {C7, 12}, {0, 0} };          /* das klassische "Pling" */
static const Note M_MOVE[]   = { {E6, 3}, {0, 0} };
static const Note M_SELECT[] = { {G5, 4}, {C6, 4}, {G6, 10}, {0, 0} };
static const Note M_COIN[]   = { {B5, 4}, {E6, 14}, {0, 0} };
static const Note M_BATT[]   = { {D6, 3}, {G6, 3}, {C7, 8}, {0, 0} };
static const Note M_LEVEL[]  = { {C5, 5}, {E5, 5}, {G5, 5}, {C6, 5}, {G5, 4}, {C6, 18}, {0, 0} };
static const Note M_START[]  = { {C5, 4}, {E5, 4}, {G5, 4}, {C6, 12}, {0, 0} };
static const Note M_RIGHT[]  = { {C6, 5}, {G6, 16}, {0, 0} };
static const Note M_WRONG[]  = { {E4, 8}, {C4, 24}, {0, 0} };
static const Note M_OVER[]   = { {G5, 9}, {E5, 9}, {C5, 9}, {G4, 9}, {REST, 4}, {C4, 36}, {0, 0} };
static const Note M_WIN[]    = { {C5, 6}, {E5, 6}, {G5, 6}, {C6, 8}, {REST, 4}, {G5, 6}, {C6, 30}, {0, 0} };

static void snd_note(uint16_t f, uint8_t env) {
    if (!f) { NR12_REG = 0x00; NR14_REG = 0x80; return; }
    NR10_REG = 0x00; NR11_REG = 0x80; NR12_REG = env;
    NR13_REG = (uint8_t)(f & 0xFF); NR14_REG = 0x80 | (uint8_t)(f >> 8);
}

static void snd_tick(void) {
    if (!snd_ptr) return;
    if (snd_cnt) { snd_cnt--; return; }
    if (!snd_ptr->len) { snd_ptr = 0; return; }
    snd_note(snd_ptr->f, snd_env);
    snd_cnt = snd_ptr->len - 1;
    snd_ptr++;
}

static void play(const Note *m, uint8_t env) {
    disable_interrupts();
    snd_ptr = m; snd_cnt = 0; snd_env = env;
    enable_interrupts();
}

static void boom(void) {   /* Rauschen auf Kanal 4 */
    NR41_REG = 0x00; NR42_REG = 0xF4; NR43_REG = 0x61; NR44_REG = 0x80;
}

static void wait_frames(uint8_t n) { while (n--) wait_vbl_done(); }

static void wait_release(void) { while (joypad()) wait_vbl_done(); }

static uint8_t wait_press(uint8_t mask) {
    uint8_t keys;
    wait_release();
    while (!(keys = (joypad() & mask))) wait_vbl_done();
    wait_release();
    return keys;
}

static void clear_screen(void) {
    HIDE_SPRITES;
    cls();
    gotoxy(0, 0);
}

static void put(uint8_t x, uint8_t y, uint8_t t) { set_bkg_tile_xy(x, y, t); }

static uint8_t glyph_tile[FONT_COUNT];

/* Direkter Kacheltext vermeidet das automatische Scrollen unten rechts. */
static void text(uint8_t x, uint8_t y, const char *s) {
    while (*s && x < 20) {
        uint8_t c = *s++;
        if (c < FONT_FIRST || c >= FONT_FIRST + FONT_COUNT) c = '?';
        put(x++, y, glyph_tile[c - FONT_FIRST]);
    }
}

static void text_c(uint8_t y, const char *s) { text((20 - (uint8_t)strlen(s)) / 2, y, s); }

static void clear_row(uint8_t y) { text(0, y, "                    "); }

/* ---------- Pause: liegt im Fenster-Layer ueber dem Spiel ---------- */
static void wtext(uint8_t x, uint8_t y, const char *s) {
    while (*s && x < 20) set_win_tile_xy(x++, y, glyph_tile[*s++ - FONT_FIRST]);
}

static void pause_init(void) {
    uint8_t i;
    for (i = 0; i < 20; i++) {
        set_win_tile_xy(i, 0, T_WALL);
        set_win_tile_xy(i, 1, glyph_tile[0]);
        set_win_tile_xy(i, 2, glyph_tile[0]);
    }
    wtext(7, 1, "PAUSE");
    wtext(1, 2, "A: WEITER B: MENUE");
    move_win(7, 120);
}

/* Gibt 1 zurueck, wenn ins Menue gewechselt werden soll. */
static uint8_t pause_game(void) {
    uint8_t keys, sprites = LCDC_REG & LCDCF_OBJON;
    HIDE_SPRITES;
    SHOW_WIN;
    play(M_MOVE, 0xA1);
    keys = wait_press(J_A | J_B | J_START);
    HIDE_WIN;
    if (sprites) SHOW_SPRITES;
    return (keys & J_B) ? 1 : 0;
}

/* ================================================================
 * SPIEL 1: QUELLEN-SNAKE
 * Die Schlange frisst Quellen. Jede Quelle zeigt ihre Quellenart.
 * ================================================================ */

#define MAXLEN 120
static uint8_t sx[MAXLEN], sy[MAXLEN], occupied[18][20];
static uint8_t head_slot, tail_slot;

static uint8_t next_slot(uint8_t slot) {
    return slot + 1 == MAXLEN ? 0 : slot + 1;
}
static uint8_t slen, dir, ndir, fx, fy, ftype, score, collected;

static const uint8_t food_tile[4] = { T_COIN, T_LETTER, T_CASS, T_FOTO };
static const char * const food_name[4] = {
    "MUENZE=GEGENSTAENDL.",
    "BRIEF=SCHRIFTQUELLE ",
    "KASSETTE=TONQUELLE  ",
    "FOTO=BILDQUELLE     "
};

/* Spielfeld: x 1..18, y 3..16, Wand drumherum */
static void place_food(void) {
    do {
        fx = 1 + (rand() % 18);
        fy = 3 + (rand() % 14);
    } while (occupied[fy][fx]);
    ftype = rand() & 3;
    if (collected != 0x0F) {
        while (collected & (1u << ftype)) ftype = (ftype + 1) & 3;
    }
    put(fx, fy, food_tile[ftype]);
}

/* Level = Zeitalter. Zahl = Bilder pro Schritt (60 Bilder = 1 Sekunde) */
#define NLEVEL 5
static const uint8_t lv_speed[NLEVEL] = { 30, 22, 16, 11, 7 };
static const char * const lv_name[NLEVEL] = {
    "STEINZEIT ", "RITTERZEIT", "DAMPFLOK  ", "GAMEBOY   ", "ZUKUNFT   "
};
static uint8_t level;

static void snake_hud(void) {
    gotoxy(0, 0); printf("L%u %s%3u", (unsigned)(level + 1), lv_name[level], (unsigned)score);
}

/* Auswahl-Bildschirm fuer Level / Schwierigkeit. Gibt 0..n-1 zurueck. */
static uint8_t choose(const char *title, const char *prompt, const char * const *names,
                      uint8_t n, const char *h1, const char *h2) {
    uint8_t sel = 0, i, keys, prev = 0, old;
    clear_screen();
    text_c(1, title);
    text(1, 3, prompt);
    for (i = 0; i < n; i++) {
        gotoxy(3, 5 + i * 2); printf("%u %s", (unsigned)(i + 1), names[i]);
    }
    text(1, 15, h1);
    text(1, 16, h2);
    text(0, 17, "A: SPIEL  B: ZURUECK");
    put(1, 5, T_ARROW);
    wait_release();
    while (1) {
        keys = joypad();
        old = sel;
        if ((keys & J_UP) && !(prev & J_UP) && sel) sel--;
        if ((keys & J_DOWN) && !(prev & J_DOWN) && sel + 1 < n) sel++;
        if (old != sel) {
            put(1, 5 + old * 2, 0);
            put(1, 5 + sel * 2, T_ARROW);
            play(M_MOVE, 0xA1);
        }
        prev = keys;
        if (keys & J_B) { wait_release(); return 0xFF; }
        if (keys & (J_A | J_START)) break;
        wait_vbl_done();
    }
    play(M_START, 0xF2);
    wait_release();
    return sel;
}

static void game_snake(void) {
    uint8_t i, x, y, speed, tick = 0, keys, eating, growing;
    uint8_t album_notice = 0;
    const char *message = "FUELLE DEIN ALBUM!  ";
    level = choose("QUELLEN-SNAKE", "WAEHLE DEIN LEVEL:", lv_name, NLEVEL, "ALLE 5 QUELLEN GEHT", "ES EIN LEVEL HOEHER");
    if (level == 0xFF) return;
    clear_screen();
    for (x = 0; x < 20; x++) { put(x, 2, T_WALL); put(x, 17, T_WALL); }
    for (y = 2; y < 18; y++) { put(0, y, T_WALL); put(19, y, T_WALL); }
    slen = 3; score = 0; collected = 0; dir = ndir = 0; /* 0=rechts 1=links 2=hoch 3=runter */
    memset(occupied, 0, sizeof occupied);
    tail_slot = 0; head_slot = slen - 1;
    for (i = 0; i < slen; i++) {
        sx[i] = 6 + i; sy[i] = 9;
        occupied[9][sx[i]] = 1;
        put(sx[i], 9, i == head_slot ? T_HEAD : T_BODY);
    }
    snake_hud();
    for (i = 0; i < 4; i++) put(16 + i, 0, T_GHOST_COIN + i);
    text(0, 1, message);
    place_food();
    speed = lv_speed[level];
    wait_release();

    while (1) {
        keys = joypad();
        if ((keys & J_RIGHT) && dir != 1) ndir = 0;
        else if ((keys & J_LEFT) && dir != 0) ndir = 1;
        else if ((keys & J_UP) && dir != 3) ndir = 2;
        else if ((keys & J_DOWN) && dir != 2) ndir = 3;
        if ((keys & J_START) && pause_game()) return;
        wait_vbl_done();
        if (album_notice && !--album_notice) {
            for (i = 1; i < 19; i++) put(i, 17, T_WALL);
        }
        if (++tick < speed) continue;
        tick = 0;
        dir = ndir;

        x = sx[head_slot]; y = sy[head_slot];
        if (dir == 0) x++; else if (dir == 1) x--; else if (dir == 2) y--; else y++;

        eating = x == fx && y == fy;
        growing = eating && slen < MAXLEN;
        if (x == 0 || x == 19 || y == 2 || y == 17 ||
            (occupied[y][x] && (growing || x != sx[tail_slot] || y != sy[tail_slot]))) {
            boom();
            play(M_OVER, 0xF4);
            break;
        }

        if (eating) {
            play(M_COIN, 0xF2);
            if (!(collected & (1u << ftype))) {
                collected |= 1u << ftype;
                put(16 + ftype, 0, food_tile[ftype]);
                if (collected == 0x0F) {
                    text(1, 17, "SAMMLUNG KOMPLETT!");
                    album_notice = 90;
                    play(M_WIN, 0xC2);
                }
            }
            score++;
            message = food_name[ftype];
            text(0, 1, message);
            if ((score % 5) == 0 && level < NLEVEL - 1) {
                level++;
                speed = lv_speed[level];
                play(M_LEVEL, 0xF2);
            }
            snake_hud();
        }
        if (growing) slen++;
        else {
            occupied[sy[tail_slot]][sx[tail_slot]] = 0;
            put(sx[tail_slot], sy[tail_slot], 0);
            tail_slot = next_slot(tail_slot);
        }
        put(sx[head_slot], sy[head_slot], T_BODY);
        head_slot = next_slot(head_slot);
        sx[head_slot] = x; sy[head_slot] = y; occupied[y][x] = 1;
        put(x, y, T_HEAD);
        if (eating) place_food();
    }

    wait_frames(40);
    clear_screen();
    text(5, 2, "GAME OVER!");
    gotoxy(2, 4); printf("DU HAST %u QUELLEN", (unsigned)score);
    text(2, 5, "GESAMMELT.");
    gotoxy(2, 7); printf("ZEIT: %s", lv_name[level]);
    text(1, 9, collected == 0x0F ? "SAMMLUNG KOMPLETT!" : "DEINE QUELLEN:");
    if (!collected) text(1, 11, "NOCH KEINE.");
    y = 11;
    for (i = 0; i < 4; i++) {
        if (collected & (1u << i)) {
            put(0, y, food_tile[i]);
            text(1, y++, food_name[i]);
        }
    }
    text(1, 17, "A: ZUM MENUE");
    wait_press(J_START | J_A);
}

/* ================================================================
 * SPIEL 2: BATTERIE-REGEN
 * Der Gameboy faengt Batterien. Hausaufgaben kosten ein Leben.
 * ================================================================ */

#define NOBJ 4
static uint8_t ox[NOBJ], oy[NOBJ], otype[NOBJ], oactive[NOBJ];

static void draw_player(uint8_t px) {
    move_sprite(0, px + 8,  136);
    move_sprite(1, px + 16, 136);
    move_sprite(2, px + 8,  144);
    move_sprite(3, px + 16, 144);
}

/* Schwierigkeit: Fall-Verzoegerung (Bilder pro Pixel), Abstand neuer Dinge,
   Stromverbrauch (Bilder pro Prozent), Hausaufgaben-Chance (1 von x), Leben */
static const char * const rain_name[3] = { "LEICHT", "MITTEL", "SCHWER" };
static const uint8_t rain_fdelay[3] = { 3, 2, 1 };
static const uint8_t rain_spawn[3]  = { 80, 60, 45 };
static const uint8_t rain_drain[3]  = { 16, 11, 8 };
static const uint8_t rain_hw[3]     = { 6, 4, 3 };
static const uint8_t rain_lives[3]  = { 5, 3, 3 };


static void game_rain(void) {
    uint8_t px = 72, i, batt = 0, spawn = 0, keys, ftick = 0;
    uint8_t energy = 100, etick = 0, notice = 0, lost_life = 0;
    uint8_t counters_dirty = 1, energy_dirty = 1, old_px;
    uint8_t filled_slots = 0, glow = 0;
    uint8_t d = choose("BATTERIE-REGEN", "WAEHLE DEIN LEVEL:", rain_name, 3, "BATTERIEN FANGEN!", "HEFTEN AUSWEICHEN!");
    uint8_t lives, fdelay, spawnrate;
    if (d == 0xFF) return;
    lives = rain_lives[d];
    fdelay = rain_fdelay[d];
    spawnrate = rain_spawn[d];

    clear_screen();
    text(0, 0, "BATT:        LEBEN:");
    for (i = 0; i < 4; i++) put(8 + i, 0, T_BATT_EMPTY);
    text(0, 1, "STROM:   %");
    for (i = 0; i < 20; i++) put(i, 17, T_WALL);
    set_sprite_tile(0, T_GB_TL); set_sprite_tile(1, T_GB_TR);
    set_sprite_tile(2, T_GB_BL); set_sprite_tile(3, T_GB_BR);
    for (i = 0; i < NOBJ; i++) { oactive[i] = 0; move_sprite(4 + i, 0, 0); }
    draw_player(px);
    SHOW_SPRITES;

    while (lives && energy) {
        keys = joypad();
        if ((keys & J_START) && pause_game()) return;
        if (glow && !--glow) {
            set_sprite_tile(0, T_GB_TL);
            set_sprite_tile(1, T_GB_TR);
        }
        old_px = px;
        if ((keys & J_LEFT) && px > 2) px -= 2;
        if ((keys & J_RIGHT) && px < 142) px += 2;
        if (px != old_px) draw_player(px);
        if (++etick >= rain_drain[d]) {
            etick = 0;
            energy--;
            energy_dirty = 1;
        }

        if (++spawn >= spawnrate) {
            spawn = 0;
            for (i = 0; i < NOBJ; i++) if (!oactive[i]) {
                oactive[i] = 1;
                oy[i] = 24;
                ox[i] = 4 + (rand() % 148);
                otype[i] = (rand() % rain_hw[d]) == 0;
                move_sprite(4 + i, ox[i] + 8, oy[i] + 16);
                set_sprite_tile(4 + i, otype[i] ? T_HOMEW : T_BATT);
                break;
            }
        }

        if (++ftick >= fdelay) ftick = 0;
        for (i = 0; i < NOBJ; i++) if (oactive[i]) {
            if (ftick == 0) oy[i]++;
            if (oy[i] > 112 && oy[i] < 136 && ox[i] + 8 > px && ox[i] < px + 16) {
                oactive[i] = 0; move_sprite(4 + i, 0, 0);
                counters_dirty = 1;
                notice = 24;
                lost_life = otype[i];
                if (otype[i]) {
                    if (lives) lives--;
                    boom();
                    play(M_WRONG, 0xD3);
                } else {
                    if (batt < 255) batt++;
                    play(M_BATT, 0xD2);
                    if (filled_slots == 4) {
                        uint8_t slot;
                        for (slot = 0; slot < 4; slot++) put(8 + slot, 0, T_BATT_EMPTY);
                        filled_slots = 0;
                    }
                    put(8 + filled_slots, 0, T_BATT);
                    if (++filled_slots == 4) {
                        set_sprite_tile(0, T_GB_TL_LIT);
                        set_sprite_tile(1, T_GB_TR_LIT);
                        glow = 36;
                    }
                    energy = (energy > 75) ? 100 : energy + 25;
                    energy_dirty = 1;
                    if (batt < 255 && (batt % 5) == 0) {
                        if (spawnrate > 28) spawnrate = spawnrate >= 34 ? spawnrate - 6 : 28;
                        if (fdelay > 1 && (batt % 10) == 0) {
                            fdelay--;
                            play(M_LEVEL, 0xD2);
                        }
                    }
                }
            } else if (oy[i] >= 136) {
                oactive[i] = 0; move_sprite(4 + i, 0, 0);
            } else if (!ftick) {
                move_sprite(4 + i, ox[i] + 8, oy[i] + 16);
            }
        }

        if (counters_dirty) {
            gotoxy(5, 0); printf("%3u", (unsigned)batt);
            gotoxy(19, 0); putchar('0' + lives);
            counters_dirty = 0;
        }
        if (energy_dirty) {
            gotoxy(6, 1); printf("%3u", (unsigned)energy);
            energy_dirty = 0;
        }
        if (notice == 24) text(12, 1, lost_life ? "-1 LEBEN" : "+25 MAX ");
        if (notice && !--notice) text(12, 1, "        ");
        wait_vbl_done();
    }

    HIDE_SPRITES;
    play(M_OVER, 0xD3);
    clear_screen();
    if (!energy) {
        text(2, 2, "BATTERIE LEER!");
        text(1, 5, "DER ORIGINAL-GAME");
        text(1, 6, "BOY BRAUCHT 4 AA.");
    } else {
        text(1, 2, "ZU VIELE HEFTE!");
        text(1, 5, "ERST LERNEN, DANN");
        text(1, 6, "SPIELEN. SAGT MAMA.");
    }
    gotoxy(1, 10); printf("BATTERIEN: %u", (unsigned)batt);
    text(1, 17, "A: ZUM MENUE");
    wait_press(J_START | J_A | J_B);
}

/* ================================================================
 * SPIEL 3: ZEITREISE-QUIZ
 * ================================================================ */

typedef struct {
    const char *q1, *q2, *q3;
    const char *a, *b;
    uint8_t right;             /* 0 = A, 1 = B */
    const char *f1, *f2, *f3;  /* Fakt nach der Antwort */
} Question;

static const Question quiz[] = {
    { "WANN KAM DER", "GAMEBOY NACH", "EUROPA?",
      "2010", "1990", 1,
      "1989 IN JAPAN,", "1990 IN EUROPA.", "" },
    { "WIE VIELE BATTERIEN", "BRAUCHT EIN", "GAMEBOY?",
      "2 STUECK", "4 STUECK", 1,
      "4 STUECK AA. LEER?", "DANN SPIEL VORBEI!", "" },
    { "WAS IST AELTER?", "", "",
      "UNSERE SCHULE", "DER GAMEBOY", 0,
      "SCHULE: SEIT 1985,", "GAMEBOY: SEIT 1989!", "" },
    { "DIESER GAMEBOY IST", "WELCHE QUELLENART?", "",
      "SCHRIFTLICHE", "GEGENSTAENDLICHE", 1,
      "EIN GEGENSTAND ZUM", "ANFASSEN = GEGEN-", "STAENDLICHE QUELLE." },
    { "EIN ALTER BRIEF VON", "OMA IST EINE ...", "",
      "SCHRIFTQUELLE", "BILDQUELLE", 0,
      "TEXT VON FRUEHER", "= SCHRIFTQUELLE.", "" },
    { "WELCHE FARBE HATTE", "DER BILDSCHIRM?", "",
      "GRUEN-GRAU", "KNALLBUNT", 0,
      "UND OHNE LICHT!", "NUR MIT LAMPE.", "" },
    { "WILHELM OSTWALD", "BEKAM 1909 DEN ...", "",
      "OSCAR", "NOBELPREIS", 1,
      "FUER CHEMIE. UNSER", "NAMENSGEBER!", "" },
    { "TETRIS WURDE", "ERFUNDEN IN DER ...", "",
      "SOWJETUNION", "USA", 0,
      "1984 IN MOSKAU.", "", "" },
    { "27 GAMEBOYS, JEDER", "BRAUCHT 4 BATTERIEN", "WIE VIELE ZUSAMMEN?",
      "112", "108", 1,
      "27 MAL 4 = 108.", "TRICK: 25 MAL 4=100", "PLUS 2 MAL 4 = 8." },
    { "WER DIE VERGANGEN-", "HEIT NICHT KENNT,", "KANN DIE GEGENWART",
      "NICHT VERSTEHEN", "BESSER VERSTEHEN", 0,
      "...UND DIE ZUKUNFT", "NICHT GESTALTEN.", "HELMUT KOHL, 1995" },
};
#define NQ (sizeof(quiz) / sizeof(quiz[0]))

/* Auswahl: Anzahl der Fragen und Countdown */
static const char * const quiz_mode[4] = {
    "KURZ: 5 FRAGEN", "KURZ+COUNTDOWN", "LANG: 10 FRAGEN", "LANG+COUNTDOWN"
};
/* Kurzfassung: Gameboy als Quelle, Schule, Ostwald, Mathe, Kohl-Zitat */
static const uint8_t quiz_short[5] = { 3, 2, 6, 8, 9 };
#define QTIME 15   /* Sekunden pro Frage im Countdown-Modus */

static void tick_click(uint8_t urgent) {
    if (urgent) play(M_MOVE, 0xC1);
    else { NR41_REG = 0x3F; NR42_REG = 0x61; NR43_REG = 0x10; NR44_REG = 0x80; }
}

/* 0 = A, 1 = B, 2 = Zeit um, 0xFF = zurueck ins Menue */
static uint8_t quiz_wait_answer(uint8_t timed) {
    uint8_t keys, frames = 0, secs = QTIME;
    if (timed) { gotoxy(17, 0); printf("%2u", (unsigned)secs); }
    wait_release();
    for (;;) {
        keys = joypad();
        if (keys & J_START) {
            if (pause_game()) return 0xFF;
            wait_release();
        }
        if (keys & J_A) return 0;
        if (keys & J_B) return 1;
        wait_vbl_done();
        if (timed && ++frames == 60) {
            frames = 0;
            secs--;
            gotoxy(17, 0); printf("%2u", (unsigned)secs);
            if (!secs) return 2;
            tick_click(secs <= 3);
        }
    }
}

static void game_quiz(void) {
    uint8_t i, sel, right = 0, timed, ok, k;
    uint8_t mode, count;
    mode = choose("ZEITREISE-QUIZ", "SPIELMODUS:", quiz_mode, 4, "COUNTDOWN: 15 SEK.", "PRO FRAGE");
    if (mode == 0xFF) return;
    timed = mode & 1;
    count = mode < 2 ? 5 : NQ;
    for (i = 0; i < count; i++) {
        const Question *q = &quiz[count == 5 ? quiz_short[i] : i];
        clear_screen();
        gotoxy(1, 0); printf("FRAGE %u/%u", (unsigned)(i + 1), (unsigned)count);
        text(1, 2, q->q1); text(1, 3, q->q2); text(1, 4, q->q3);
        text(2, 7, "A"); text(4, 7, q->a);
        text(2, 9, "B"); text(4, 9, q->b);
        text_c(17, "DRUECKE A ODER B");
        sel = quiz_wait_answer(timed);
        if (sel == 0xFF) return;
        clear_row(17);
        if (sel < 2) put(0, sel == 0 ? 7 : 9, T_ARROW);
        ok = sel == q->right;
        if (ok) { right++; play(M_RIGHT, 0xF2); }
        else play(M_WRONG, 0xF4);
        if (!ok) { gotoxy(1, 12); printf("RICHTIG IST: %c", q->right ? 'B' : 'A'); }
        text(1, 13, q->f1);
        text(1, 14, q->f2);
        text(1, 15, q->f3);
        /* Ergebnis blinkt dreimal */
        for (k = 0; k < 7; k++) {
            if (k & 1) clear_row(11);
            else text(1, 11, ok ? "RICHTIG!" : (sel == 2 ? "ZEIT UM!" : "LEIDER FALSCH!"));
            wait_frames(10);
        }
        text_c(17, i + 1 < count ? "A: NAECHSTE FRAGE" : "A: ERGEBNIS");
        for (;;) {
            k = wait_press(J_A | J_START);
            if (k & J_A) break;
            if (pause_game()) return;
        }
    }

    clear_screen();
    play(M_WIN, 0xF2);
    text_c(2, "QUIZ GESCHAFFT!");
    gotoxy(3, 5); printf("%u VON %u RICHTIG", (unsigned)right, (unsigned)count);
    text_c(8, "DEIN TITEL:");
    if (right == count)              text_c(10, "GESCHICHTS-PROFI!");
    else if (right * 10 >= count * 7) text_c(10, "ZEITREISENDE(R)");
    else if (right * 10 >= count * 4) text_c(10, "QUELLEN-LEHRLING");
    else { text_c(10, "ZURUECK IN DIE"); text_c(11, "STEINZEIT!"); }
    text_c(17, "A: ZUM MENUE");
    wait_press(J_START | J_A);
}

/* ================================================================
 * TITEL UND MENUE
 * ================================================================ */

#include "sorter.h"

/* Titelbild: bleibt stehen, bis START gedrueckt wird */
static void intro(void) {
    uint8_t x, y, blink = 0;
    clear_screen();
    set_bkg_data(LOGO_BASE, LOGO_W * 2, logodata);
    for (y = 0; y < 2; y++)
        for (x = 0; x < LOGO_W; x++)
            put((20 - LOGO_W) / 2 + x, 5 + y, LOGO_BASE + y * LOGO_W + x);
    for (y = 72; y > 0; y--) { SCY_REG = y; wait_vbl_done(); }
    SCY_REG = 0;
    play(M_BOOT, 0xD2);
    wait_frames(30);
    text_c(9, "OSTWALD EDITION");
    wait_frames(20);
    text_c(11, "KLASSE 5/3");
    wait_frames(20);
    text_c(16, "WOG LEIPZIG");
    wait_release();
    for (;;) {
        if (blink == 0) text_c(14, "DRUECKE START");
        if (blink == 40) clear_row(14);
        if (++blink == 70) blink = 0;
        if (joypad() & (J_START | J_A)) break;
        wait_vbl_done();
    }
    play(M_SELECT, 0xE2);
    wait_release();
}

/* Modul-Auswahl: immer ein Spiel pro Bildschirm, links/rechts blaettern */
#define NGAMES 4
static const char * const game_name[NGAMES] = {
    "ZEITREISE-QUIZ", "QUELLEN-SNAKE", "BATTERIE-REGEN", "QUELLEN-SORTIERER"
};
static const char * const game_d1[NGAMES] = {
    "10 FRAGEN -", "SAMMLE ALLE 4", "FANG BATTERIEN,", "3 GLEICHE QUELLEN"
};
static const char * const game_d2[NGAMES] = {
    "RATET ALLE MIT!", "QUELLENARTEN!", "WEICH HEFTEN AUS!", "VERSCHWINDEN!"
};
static uint8_t menu_sel;

static void menu_show(uint8_t g) {
    uint8_t i, ic = ICON_BASE + g * 4;
    put(9, 5, ic); put(10, 5, ic + 1); put(9, 6, ic + 2); put(10, 6, ic + 3);
    clear_row(12); clear_row(14); clear_row(15);
    text_c(12, game_name[g]);
    text_c(14, game_d1[g]);
    text_c(15, game_d2[g]);
    for (i = 0; i < NGAMES; i++) put(8 + i, 17, i == g ? T_DOT_ON : T_DOT_OFF);
}

static uint8_t menu(void) {
    uint8_t keys, prev = 0xFF, pressed, x, y;
    clear_screen();
    set_bkg_data(ART_BASE, ART_COUNT, artdata);
    text_c(1, "OSTWALD EDITION");
    for (y = 0; y < CART_H; y++)
        for (x = 0; x < CART_W; x++)
            put(7 + x, 3 + y, cart_map[y * CART_W + x]);
    put(3, 6, T_LARROW);
    put(16, 6, T_RARROW);
    menu_show(menu_sel);
    for (;;) {
        wait_vbl_done();
        keys = joypad();
        pressed = keys & ~prev;
        prev = keys;
        if (pressed & J_LEFT)  { menu_sel = menu_sel ? menu_sel - 1 : NGAMES - 1; play(M_MOVE, 0xA1); menu_show(menu_sel); }
        if (pressed & J_RIGHT) { menu_sel = menu_sel + 1 < NGAMES ? menu_sel + 1 : 0; play(M_MOVE, 0xA1); menu_show(menu_sel); }
        if (pressed & (J_A | J_START)) break;
    }
    initrand(DIV_REG | ((uint16_t)menu_sel << 8));
    play(M_SELECT, 0xE2);
    wait_release();
    return menu_sel;
}

void main(void) {
    NR52_REG = 0x80; NR50_REG = 0x77; NR51_REG = 0xFF;   /* Ton an, beide Lautsprecher */
    disable_interrupts();
    add_VBL(snd_tick);
    enable_interrupts();
    DISPLAY_ON;
    printf(" ");                     /* Standardschrift laden */
    {   /* Standardschrift durch eigene, schlankere 5x7-Schrift ersetzen */
        uint8_t c;
        for (c = FONT_FIRST; c < FONT_FIRST + FONT_COUNT; c++) {
            gotoxy(0, 0); putchar(c);
            glyph_tile[c - FONT_FIRST] = get_bkg_tile_xy(0, 0);
            set_bkg_data(glyph_tile[c - FONT_FIRST], 1, &font5x7[(uint16_t)(c - FONT_FIRST) * 16]);
        }
        cls();
    }
    set_bkg_data(TILE_BASE, TILE_COUNT, tiledata);
    set_sprite_data(TILE_BASE, TILE_COUNT, tiledata);
    SHOW_BKG;
    pause_init();
    intro();
    while (1) {
        uint8_t g = menu();
        if (g == 0) game_quiz();
        else if (g == 1) game_snake();
        else if (g == 2) game_rain();
        else game_sort();
    }
}
