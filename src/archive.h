/* Ostwald-Archiv: Quellenpaare schliessen die Luecken eines Regals. */
#define SHELF_W 8
#define SHELF_H 13
#define SHELF_X 1
#define SHELF_Y 3

static const char * const rooms[] = {
    "KLASSENZIMMER", "BIBLIOTHEK", "LABOR", "DEPOT", "MUSEUM"
};
static const uint8_t gaps[5][5] = {
    {3}, {1, 5}, {4, 0, 6}, {2, 6, 1, 4}, {5, 1, 4, 0, 6}
};
static const uint8_t fall_rate[] = { 3, 4, 5, 7, 9 }; /* Sechzehntelpixel pro Bild */
static uint8_t shelf[SHELF_H][SHELF_W];
static uint8_t pair_x, pair_y, vertical, subpixel, pair_icons[2];

static void shelf_row(uint8_t y) {
    set_bkg_tiles(SHELF_X, SHELF_Y + y, SHELF_W, 1, shelf[y]);
}

static uint8_t pair_fits(int8_t x, uint8_t y, uint8_t upright) {
    uint8_t end_y = y + upright;
    int8_t end_x = x + !upright;
    if (x < 0 || end_x >= SHELF_W || end_y >= SHELF_H) return 0;
    return !shelf[y][(uint8_t)x] && !shelf[end_y][(uint8_t)end_x];
}

static uint8_t can_slide(int8_t x, uint8_t upright) {
    if (!pair_fits(x, pair_y, upright)) return 0;
    return !subpixel || pair_fits(x, pair_y + 1, upright);
}

static void draw_pair(void) {
    uint8_t x = (SHELF_X + pair_x) * 8 + 8;
    uint8_t y = (SHELF_Y + pair_y) * 8 + 16 + (subpixel >> 4);
    move_sprite(0, x, y);
    move_sprite(1, x + (vertical ? 0 : 8), y + (vertical ? 8 : 0));
}

static uint8_t spawn_pair(void) {
    pair_x = 3; pair_y = 0; vertical = 0; subpixel = 0;
    if (!pair_fits(pair_x, pair_y, 0)) return 0;
    pair_icons[0] = food_tile[rand() & 3];
    pair_icons[1] = food_tile[rand() & 3];
    set_sprite_tile(0, pair_icons[0]);
    set_sprite_tile(1, pair_icons[1]);
    draw_pair();
    SHOW_SPRITES;
    return 1;
}

static uint8_t file_pair(void) {
    uint8_t y, x, dest = SHELF_H, removed = 0;
    shelf[pair_y][pair_x] = pair_icons[0];
    shelf[pair_y + vertical][pair_x + !vertical] = pair_icons[1];
    shelf_row(pair_y);
    if (vertical) shelf_row(pair_y + 1);
    for (y = SHELF_H; y-- > 0;) {
        for (x = 0; x < SHELF_W && shelf[y][x]; x++) {}
        if (x == SHELF_W) { removed++; continue; }
        dest--;
        if (dest != y) {
            memcpy(shelf[dest], shelf[y], SHELF_W);
            shelf_row(dest);
        }
    }
    while (dest) {
        --dest;
        memset(shelf[dest], 0, SHELF_W);
        shelf_row(dest);
    }
    return removed;
}

static void open_room(uint8_t room) {
    uint8_t x, y, row, gap;
    clear_screen();
    memset(shelf, 0, sizeof shelf);
    text(0, 0, "OSTWALD-ARCHIV");
    gotoxy(0, 1); printf("%u %s", (unsigned)(room + 1), rooms[room]);
    for (y = 0; y <= room; y++) {
        row = SHELF_H - 1 - y;
        gap = gaps[room][y];
        for (x = 0; x < SHELF_W; x++)
            if (x != gap && x != gap + 1) shelf[row][x] = food_tile[(x + y + room) & 3];
        shelf_row(row);
    }
    for (y = SHELF_Y; y < SHELF_Y + SHELF_H; y++) {
        put(0, y, T_WALL); put(SHELF_W + 1, y, T_WALL);
    }
    for (x = 0; x <= SHELF_W + 1; x++) put(x, SHELF_Y + SHELF_H, T_WALL);
    text(11, 4, "REIHEN");
    text(11, 8, "A DREHEN");
    text(11, 10, "B FALLEN");
    text(11, 14, "START:");
    text(11, 15, "MENUE");
}

static uint8_t repeat;
static int8_t last_direction;

static void steer_pair(uint8_t keys) {
    int8_t direction;
    direction = (keys & J_LEFT) ? -1 : ((keys & J_RIGHT) ? 1 : 0);
    if (direction && (direction != last_direction || !repeat)) {
        if (can_slide(pair_x + direction, vertical)) pair_x += direction;
        repeat = direction == last_direction ? 3 : 11;
    } else if (repeat) repeat--;
    last_direction = direction;

}

static void rotate_pair(void) {
    uint8_t next = vertical ^ 1u;
    if (can_slide(pair_x, next)) {
        vertical = next;
        return;
    }
    if (pair_x == 0) return;
    if (can_slide(pair_x - 1, next)) {
        pair_x--;
        vertical = next;
    }
}

static uint8_t advance_pair(uint8_t keys, uint8_t pressed, uint8_t room) {
    uint8_t speed, landed;
    landed = 0;
    if (pressed & J_B) {
        while (pair_fits(pair_x, pair_y + 1, vertical)) pair_y++;
        landed = 1;
    } else {
        speed = (keys & J_DOWN) ? 32 : fall_rate[room];
        if (pair_fits(pair_x, pair_y + 1, vertical)) {
            subpixel += speed;
            if (subpixel >= 128) { subpixel -= 128; pair_y++; }
            if (subpixel && !pair_fits(pair_x, pair_y + 1, vertical)) subpixel = 0;
        } else landed = 1;
    }
    return landed;
}

static uint8_t play_room(uint8_t room) {
    uint8_t prev, cleared, keys, pressed, removed;
    open_room(room);
    cleared = prev = repeat = 0;
    last_direction = 0;
    gotoxy(11, 5); printf("0/%u", (unsigned)(room + 1));
    spawn_pair();
    for (;;) {
        wait_vbl_done();
        keys = joypad();
        pressed = keys & ~prev;
        prev = keys;
        if (pressed & J_START) return 0;
        steer_pair(keys);
        if (pressed & J_A) rotate_pair();
        if (!advance_pair(keys, pressed, room)) { draw_pair(); continue; }
        HIDE_SPRITES;
        removed = file_pair();
        if (removed) {
            cleared += removed;
            gotoxy(11, 5); printf("%u/%u", (unsigned)cleared, (unsigned)(room + 1));
            play(M_COIN, 0xC2);
        }
        if (cleared >= room + 1) return 1;
        if (!spawn_pair()) {
            play(M_OVER, 0xC3);
            clear_screen();
            text(2, 5, "DAS REGAL IST VOLL");
            text(2, 10, "A: ZUM MENUE");
            wait_press(J_A | J_START);
            return 0;
        }
    }
}

static void game_archive(void) {
    uint8_t room, i;
    room = choose("OSTWALD-ARCHIV", rooms, 5, "FUELLE GANZE REIHEN", "MIT QUELLENPAAREN");
    if (room == 0xFF) return;
    for (i = 0; i < 8; i++) move_sprite(i, 0, 0);
    for (; room < 5; room++) {
        if (!play_room(room)) return;
        clear_screen();
        play(room == 4 ? M_WIN : M_LEVEL, 0xC2);
        text(1, 5, rooms[room]);
        text(1, 7, "REGAL SORTIERT!");
        text(1, 12, room == 4 ? "MUSEUM EROEFFNET!" : "A: NAECHSTES REGAL");
        text(1, 16, room == 4 ? "A: ZUM MENUE" : "B: ZUM MENUE");
        if (wait_press(J_A | J_B | J_START) & (J_START | J_B)) return;
    }
}
