#include "hhms.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SIDE 280
#define STATUS_H 36

typedef enum {
    BRUSH_COUNT = 0,
    BRUSH_OPEN,
    BRUSH_FLAG,
    BRUSH_ERASE,
    BRUSH_SUPPORT
} Brush;

typedef struct {
    HhmsMap map;
    float camx, camy;
    float cell;
    int topmost;
    Brush brush;
    int count;
    HhmsSupportKind sup;
    char file[256];
    int edit_file;
    int dirty;
    int hover_on;
    int hx, hy;
    int paint;
    int px, py;
    int help;
    int confirm_new;
    char msg[256];
} App;

static const Color C_BG      = {14, 16, 18, 255};
static const Color C_SIDE    = {22, 24, 28, 255};
static const Color C_LINE    = {58, 62, 70, 255};
static const Color C_WALL    = {64, 68, 74, 255};
static const Color C_FLOOR   = {28, 30, 34, 255};
static const Color C_OPEN    = {56, 82, 104, 255};
static const Color C_SAFE    = {36, 122, 64, 255};
static const Color C_MINE    = {168, 40, 38, 255};
static const Color C_FLAG    = {196, 48, 42, 255};
static const Color C_CONF    = {186, 46, 150, 255};
static const Color C_HOVER   = {240, 204, 88, 255};
static const Color C_LINK    = {80, 190, 230, 255};
static const Color C_ORIGIN  = {220, 176, 72, 255};
static const Color C_TEXT    = {228, 220, 208, 255};
static const Color C_DIM     = {138, 144, 154, 255};
static const Color C_BTN     = {40, 44, 50, 255};
static const Color C_BTNH    = {58, 64, 72, 255};
static const Color C_ON      = {118, 82, 32, 255};

static const int NDX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
static const int NDY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

static Color number_color(int n)
{
    switch (n) {
    case 1: return (Color){80, 150, 255, 255};
    case 2: return (Color){90, 200, 110, 255};
    case 3: return (Color){230, 80, 70, 255};
    case 4: return (Color){160, 120, 255, 255};
    case 5: return (Color){210, 140, 60, 255};
    case 6: return (Color){70, 200, 200, 255};
    case 7: return (Color){230, 220, 210, 255};
    case 8: return (Color){170, 170, 170, 255};
    default: return C_DIM;
    }
}

static Color heat(float p)
{
    if (p < 0.f)
        return C_WALL;
    unsigned char r = (unsigned char)(70 + p * 170);
    unsigned char g = (unsigned char)(110 - p * 70);
    unsigned char b = (unsigned char)(48);
    return (Color){r, g, b, 255};
}

static const char *filename_display(const char *path)
{
    if (!path || !path[0])
        return "mine.hhmap";
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\');
    const char *name = path;
    if (slash && slash + 1 > name)
        name = slash + 1;
    if (bslash && bslash + 1 > name)
        name = bslash + 1;
    return (*name != '\0') ? name : path;
}

static void set_msg(App *a, const char *s)
{
    strncpy(a->msg, s, sizeof(a->msg) - 1);
    a->msg[sizeof(a->msg) - 1] = 0;
}

static int still_rock(const HhmsMap *m, int x, int y)
{
    const HhmsTile *t = hhms_get(m, x, y);
    if (!t)
        return 1;
    if (t->kind == HHMS_CLEAR || t->kind == HHMS_OPEN || t->kind == HHMS_MINE)
        return 0;
    if (t->mark == HHMS_MARK_SAFE || t->mark == HHMS_MARK_MINE)
        return 0;
    return 1;
}

static int is_adj8(int x0, int y0, int x1, int y1)
{
    int dx = x0 - x1;
    int dy = y0 - y1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx <= 1 && dy <= 1 && (dx || dy);
}

/* 0 none, 1 linked rock, 2 the hovered number that explains them */
static int link_role(const App *a, int x, int y)
{
    if (!a->hover_on)
        return 0;
    if (x == a->hx && y == a->hy)
        return 0;
    const HhmsTile *h = hhms_get(&a->map, a->hx, a->hy);
    if (h && h->kind == HHMS_CLEAR) {
        if (is_adj8(a->hx, a->hy, x, y) && still_rock(&a->map, x, y))
            return 1;
        return 0;
    }
    if (!still_rock(&a->map, a->hx, a->hy))
        return 0;
    if (h && (h->kind == HHMS_CLEAR || h->kind == HHMS_OPEN || h->kind == HHMS_MINE))
        return 0;
    if (h && (h->mark == HHMS_MARK_SAFE || h->mark == HHMS_MARK_MINE))
        return 0;
    /* hover unknown: light numbers that still count it, and other rocks those numbers still count */
    for (int i = 0; i < 8; i++) {
        int cx = a->hx + NDX[i];
        int cy = a->hy + NDY[i];
        const HhmsTile *c = hhms_get(&a->map, cx, cy);
        if (!c || c->kind != HHMS_CLEAR)
            continue;
        if (x == cx && y == cy)
            return 2;
        if (is_adj8(cx, cy, x, y) && still_rock(&a->map, x, y))
            return 1;
    }
    return 0;
}

static int remaining_for_clear(const HhmsMap *m, int x, int y)
{
    int n = 0;
    for (int i = 0; i < 8; i++) {
        if (still_rock(m, x + NDX[i], y + NDY[i]))
            n++;
    }
    return n;
}

static void app_init(App *a)
{
    memset(a, 0, sizeof(*a));
    hhms_init(&a->map);
    a->camx = 0;
    a->camy = 0;
    a->cell = 32;
    a->topmost = 1;
    a->brush = BRUSH_COUNT;
    a->count = 0;
    a->sup = HHMS_SUP_WOOD;
    a->help = 1;
    strncpy(a->file, "mine.hhmap", sizeof(a->file) - 1);
    set_msg(a, "Hover a number to see which walls it still counts.");
}

static void solve_now(App *a)
{
    hhms_solve(&a->map);
    if (a->map.contradiction)
        set_msg(a, "Those numbers cannot all be true.");
}

static Rectangle grid_rect(void)
{
    return (Rectangle){0, 0, (float)(GetScreenWidth() - SIDE), (float)(GetScreenHeight() - STATUS_H)};
}

static Rectangle side_rect(void)
{
    return (Rectangle){(float)(GetScreenWidth() - SIDE), 0, (float)SIDE, (float)GetScreenHeight()};
}

static void tile_to_screen(const App *a, int x, int y, float *sx, float *sy)
{
    Rectangle g = grid_rect();
    *sx = g.x + g.width * 0.5f + ((float)x - a->camx) * a->cell;
    *sy = g.y + g.height * 0.5f + ((float)y - a->camy) * a->cell;
}

static int screen_to_tile(const App *a, float sx, float sy, int *x, int *y)
{
    Rectangle g = grid_rect();
    if (!CheckCollisionPointRec((Vector2){sx, sy}, g))
        return 0;
    float tx = a->camx + (sx - (g.x + g.width * 0.5f)) / a->cell;
    float ty = a->camy + (sy - (g.y + g.height * 0.5f)) / a->cell;
    *x = (int)floorf(tx);
    *y = (int)floorf(ty);
    return 1;
}

static int hit(Rectangle r)
{
    return CheckCollisionPointRec(GetMousePosition(), r);
}

static int btn(Rectangle r, const char *label, int on)
{
    int hot = hit(r);
    DrawRectangleRec(r, on ? C_ON : (hot ? C_BTNH : C_BTN));
    DrawRectangleLinesEx(r, 1, on ? C_HOVER : C_LINE);
    int tw = MeasureText(label, 15);
    DrawText(label, (int)(r.x + (r.width - (float)tw) * 0.5f), (int)(r.y + 6), 15, C_TEXT);
    return hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void apply_at(App *a, int x, int y)
{
    int rc = 0;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || a->brush == BRUSH_SUPPORT) {
        int i = hhms_support_index(&a->map, x, y);
        if (i >= 0 && a->map.supports[i].kind == a->sup)
            rc = hhms_del_support_at(&a->map, x, y, 1);
        else
            rc = hhms_add_support(&a->map, x, y, a->sup, 1);
    } else if (a->brush == BRUSH_OPEN) {
        rc = hhms_set_tile(&a->map, x, y, HHMS_OPEN, 0, 1);
    } else if (a->brush == BRUSH_FLAG) {
        const HhmsTile *t = hhms_get(&a->map, x, y);
        if (t && t->kind == HHMS_MINE)
            rc = hhms_erase_tile(&a->map, x, y, 1);
        else
            rc = hhms_set_tile(&a->map, x, y, HHMS_MINE, 0, 1);
    } else if (a->brush == BRUSH_ERASE) {
        rc = hhms_erase_tile(&a->map, x, y, 1);
    } else {
        rc = hhms_set_tile(&a->map, x, y, HHMS_CLEAR, a->count, 1);
    }
    if (rc == 0) {
        a->dirty = 1;
        solve_now(a);
    } else {
        set_msg(a, "Map is full.");
    }
}

static void do_save(App *a)
{
    if (hhms_save(&a->map, a->file) == 0) {
        a->dirty = 0;
        set_msg(a, "Saved.");
    } else {
        set_msg(a, "Save failed.");
    }
}

static void do_load(App *a)
{
    if (hhms_load(&a->map, a->file) == 0) {
        a->dirty = 0;
        a->map.nundo = 0;
        solve_now(a);
        set_msg(a, "Loaded. Hover a number to see what it still counts.");
    } else {
        set_msg(a, "Load failed.");
    }
}

static void do_new(App *a)
{
    hhms_init(&a->map);
    a->dirty = 0;
    a->confirm_new = 0;
    set_msg(a, "New map. Hover a tile and type the dust count.");
}

static void request_new(App *a)
{
    if (a->dirty && a->map.ntiles > 0) {
        a->confirm_new = 1;
        set_msg(a, "Unsaved changes: press N again to clear.");
    } else {
        do_new(a);
    }
}

static void handle_file_keys(App *a)
{
    int ch = GetCharPressed();
    while (ch > 0) {
        size_t n = strlen(a->file);
        if (ch >= 32 && ch < 127 && n + 1 < sizeof(a->file)) {
            a->file[n] = (char)ch;
            a->file[n + 1] = 0;
        }
        ch = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        size_t n = strlen(a->file);
        if (n > 0)
            a->file[n - 1] = 0;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))
        a->edit_file = 0;
}

static void handle_keys(App *a)
{
    if (a->edit_file) {
        handle_file_keys(a);
        return;
    }

    if (a->help && (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
        a->help = 0;
        return;
    }

    if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_KP_0)) { a->confirm_new = 0; a->brush = BRUSH_COUNT; a->count = 0; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) { a->confirm_new = 0; a->brush = BRUSH_COUNT; a->count = 1; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) { a->confirm_new = 0; a->brush = BRUSH_COUNT; a->count = 2; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) { a->confirm_new = 0; a->brush = BRUSH_COUNT; a->count = 3; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_FOUR) || IsKeyPressed(KEY_KP_4)) { a->confirm_new = 0; a->brush = BRUSH_COUNT; a->count = 4; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_FIVE) || IsKeyPressed(KEY_KP_5)) { a->confirm_new = 0; a->brush = BRUSH_COUNT; a->count = 5; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_SIX) || IsKeyPressed(KEY_KP_6)) { a->confirm_new = 0; a->brush = BRUSH_COUNT; a->count = 6; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_SEVEN) || IsKeyPressed(KEY_KP_7)) { a->confirm_new = 0; a->brush = BRUSH_COUNT; a->count = 7; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_EIGHT) || IsKeyPressed(KEY_KP_8)) { a->confirm_new = 0; a->brush = BRUSH_COUNT; a->count = 8; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_G)) { a->confirm_new = 0; a->brush = BRUSH_OPEN; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_F)) { a->confirm_new = 0; a->brush = BRUSH_FLAG; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) { a->confirm_new = 0; a->brush = BRUSH_ERASE; if (a->hover_on) apply_at(a, a->hx, a->hy); }
    if (IsKeyPressed(KEY_LEFT_BRACKET))
        a->sup = (HhmsSupportKind)(((int)a->sup + 3) % 4);
    if (IsKeyPressed(KEY_RIGHT_BRACKET))
        a->sup = (HhmsSupportKind)(((int)a->sup + 1) % 4);
    if (IsKeyPressed(KEY_U) && hhms_undo(&a->map)) {
        a->confirm_new = 0;
        a->dirty = 1;
        solve_now(a);
        set_msg(a, "Undo.");
    }
    if (IsKeyPressed(KEY_S)) {
        a->confirm_new = 0;
        do_save(a);
    }
    if (IsKeyPressed(KEY_L)) {
        a->confirm_new = 0;
        do_load(a);
    }
    if (IsKeyPressed(KEY_N)) {
        if (a->confirm_new)
            do_new(a);
        else
            request_new(a);
    }
    if (IsKeyPressed(KEY_T)) {
        a->topmost = !a->topmost;
        if (a->topmost)
            SetWindowState(FLAG_WINDOW_TOPMOST);
        else
            ClearWindowState(FLAG_WINDOW_TOPMOST);
    }
    if (IsKeyPressed(KEY_R)) {
        a->camx = 0;
        a->camy = 0;
        a->cell = 32;
    }
    if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_SLASH))
        a->help = !a->help;

    if (a->confirm_new && IsKeyPressed(KEY_ESCAPE)) {
        a->confirm_new = 0;
        set_msg(a, "New map cancelled.");
    }

    float pan = 12.f / a->cell;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  a->camx -= pan;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) a->camx += pan;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    a->camy -= pan;
    if (IsKeyDown(KEY_DOWN)) a->camy += pan;
}

static void handle_mouse(App *a)
{
    Vector2 m = GetMousePosition();
    a->hover_on = screen_to_tile(a, m.x, m.y, &a->hx, &a->hy);

    float wheel = GetMouseWheelMove();
    if (wheel != 0 && a->hover_on) {
        a->cell *= (wheel > 0) ? 1.1f : (1.f / 1.1f);
        if (a->cell < 14) a->cell = 14;
        if (a->cell > 72) a->cell = 72;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_SPACE))) {
        Vector2 d = GetMouseDelta();
        a->camx -= d.x / a->cell;
        a->camy -= d.y / a->cell;
        return;
    }

    if (a->help)
        return;

    if (hit(side_rect()))
        return;
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && a->hover_on) {
        Brush old = a->brush;
        a->brush = BRUSH_FLAG;
        apply_at(a, a->hx, a->hy);
        a->brush = old;
        a->paint = 0;
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && a->hover_on) {
        apply_at(a, a->hx, a->hy);
        a->paint = 1;
        a->px = a->hx;
        a->py = a->hy;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        a->paint = 0;
    if (a->paint && a->hover_on && (a->hx != a->px || a->hy != a->py)) {
        apply_at(a, a->hx, a->hy);
        a->px = a->hx;
        a->py = a->hy;
    }
}

static void draw_label(float sx, float sy, float cell, const char *s, Color c)
{
    int fs = cell > 30 ? 14 : 11;
    if (cell < 22)
        return;
    int tw = MeasureText(s, fs);
    DrawText(s, (int)(sx + (cell - (float)tw) * 0.5f), (int)(sy + (cell - (float)fs) * 0.5f), fs, c);
}

static void draw_tile(const App *a, int x, int y)
{
    float sx, sy;
    tile_to_screen(a, x, y, &sx, &sy);
    Rectangle r = {sx + 1, sy + 1, a->cell - 2, a->cell - 2};
    if (r.x + r.width < 0 || r.y + r.height < 0)
        return;
    Rectangle g = grid_rect();
    if (r.x > g.width || r.y > g.height)
        return;

    const HhmsTile *t = hhms_get(&a->map, x, y);
    Color fill = C_WALL;
    int show_pct = 0;
    if (t) {
        if (t->mark == HHMS_MARK_CONFLICT)
            fill = C_CONF;
        else if (t->kind == HHMS_OPEN)
            fill = C_OPEN;
        else if (t->kind == HHMS_CLEAR)
            fill = C_FLOOR;
        else if (t->kind == HHMS_MINE)
            fill = C_FLAG;
        else if (t->mark == HHMS_MARK_SAFE)
            fill = C_SAFE;
        else if (t->mark == HHMS_MARK_MINE)
            fill = C_MINE;
        else if (t->p_mine > 0.f && t->p_mine < 1.f) {
            fill = heat(t->p_mine);
            show_pct = 1;
        }
    }
    DrawRectangleRec(r, fill);

    if (hhms_covered(&a->map, x, y))
        DrawRectangleRec(r, (Color){50, 90, 160, 46});

    int link = link_role(a, x, y);
    if (link == 1)
        DrawRectangleLinesEx((Rectangle){sx + 2, sy + 2, a->cell - 4, a->cell - 4}, 2, C_LINK);
    else if (link == 2)
        DrawRectangleLinesEx((Rectangle){sx + 2, sy + 2, a->cell - 4, a->cell - 4}, 2, C_HOVER);

    if (x == 0 && y == 0)
        DrawRectangleLinesEx(r, 2, C_ORIGIN);

    if (t && t->kind == HHMS_CLEAR) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", t->count);
        int fs = a->cell > 24 ? 22 : 14;
        int tw = MeasureText(buf, fs);
        DrawText(buf, (int)(sx + (a->cell - (float)tw) * 0.5f), (int)(sy + (a->cell - (float)fs) * 0.5f), fs, number_color(t->count));
    } else if (t && (t->kind == HHMS_MINE || t->mark == HHMS_MARK_MINE)) {
        draw_label(sx, sy, a->cell, "CAVE", RAYWHITE);
    } else if (t && t->mark == HHMS_MARK_SAFE) {
        draw_label(sx, sy, a->cell, "DIG", RAYWHITE);
    } else if (t && t->kind == HHMS_OPEN) {
        draw_label(sx, sy, a->cell, "open", (Color){200, 220, 235, 255});
    } else if (show_pct && a->cell >= 22) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d%%", (int)(t->p_mine * 100.f + 0.5f));
        int fs = a->cell > 28 ? 14 : 12;
        int tw = MeasureText(buf, fs);
        DrawText(buf, (int)(sx + (a->cell - (float)tw) * 0.5f), (int)(sy + a->cell * 0.5f - (float)fs * 0.5f), fs, C_TEXT);
    }

    int si = hhms_support_index(&a->map, x, y);
    if (si >= 0) {
        const char *n = hhms_support_name(a->map.supports[si].kind);
        char buf[2] = {n[0], 0};
        if (n[0] >= 'a' && n[0] <= 'z')
            buf[0] = (char)(n[0] - 32);
        DrawText(buf, (int)(sx + 3), (int)(sy + 2), 12, C_HOVER);
    }
}

static void draw_grid(const App *a)
{
    Rectangle g = grid_rect();
    DrawRectangleRec(g, C_BG);
    int x0, y0, x1, y1;
    screen_to_tile(a, g.x, g.y, &x0, &y0);
    if (!screen_to_tile(a, g.x + g.width - 1, g.y + g.height - 1, &x1, &y1)) {
        x1 = x0 + 40;
        y1 = y0 + 30;
    }
    x0 -= 1; y0 -= 1; x1 += 1; y1 += 1;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++)
            draw_tile(a, x, y);
    }
    BeginScissorMode((int)g.x, (int)g.y, (int)g.width, (int)g.height);
    for (int x = x0; x <= x1 + 1; x++) {
        float sx, sy;
        tile_to_screen(a, x, y0, &sx, &sy);
        DrawLine((int)sx, (int)g.y, (int)sx, (int)(g.y + g.height), C_LINE);
    }
    for (int y = y0; y <= y1 + 1; y++) {
        float sx, sy;
        tile_to_screen(a, x0, y, &sx, &sy);
        DrawLine((int)g.x, (int)sy, (int)(g.x + g.width), (int)sy, C_LINE);
    }
    if (a->hover_on) {
        float sx, sy;
        tile_to_screen(a, a->hx, a->hy, &sx, &sy);
        DrawRectangleLinesEx((Rectangle){sx, sy, a->cell, a->cell}, 2, C_HOVER);
    }
    EndScissorMode();
}

static void draw_swatch(int x, int y, Color c, const char *label)
{
    DrawRectangle(x, y, 12, 12, c);
    DrawText(label, x + 16, y - 1, 12, C_TEXT);
}

static void draw_sidebar(App *a)
{
    Rectangle s = side_rect();
    DrawRectangleRec(s, C_SIDE);
    DrawLine((int)s.x, 0, (int)s.x, GetScreenHeight(), C_LINE);

    int x = (int)s.x + 12;
    int y = 10;
    DrawText("HH Minesweeper", x, y, 18, C_HOVER);
    y += 22;
    DrawText("Green DIG   Red CAVE   % chance", x, y, 12, C_DIM);
    y += 18;

    Rectangle file_r = {(float)x, (float)y, s.width - 24, 24};
    DrawRectangleRec(file_r, a->edit_file ? C_BTNH : C_BTN);
    DrawRectangleLinesEx(file_r, 1, a->edit_file ? C_HOVER : C_LINE);
    const char *fname = a->edit_file ? a->file : filename_display(a->file);
    DrawText(fname, x + 6, y + 5, 14, C_TEXT);
    if (hit(file_r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        a->edit_file = 1;
    y += 30;

    if (btn((Rectangle){(float)x, (float)y, 124, 26}, "Save (S)", 0))
        do_save(a);
    if (btn((Rectangle){(float)x + 130, (float)y, 124, 26}, "Load (L)", 0))
        do_load(a);
    y += 30;

    if (a->confirm_new) {
        if (btn((Rectangle){(float)x, (float)y, 124, 26}, "Confirm New", 1))
            do_new(a);
        if (btn((Rectangle){(float)x + 130, (float)y, 124, 26}, "Cancel", 0)) {
            a->confirm_new = 0;
            set_msg(a, "New map cancelled.");
        }
    } else {
        if (btn((Rectangle){(float)x, (float)y, 124, 26}, "New (N)", 0))
            request_new(a);
        if (btn((Rectangle){(float)x + 130, (float)y, 124, 26}, a->topmost ? "Pinned (T)" : "Unpinned", a->topmost)) {
            a->topmost = !a->topmost;
            if (a->topmost)
                SetWindowState(FLAG_WINDOW_TOPMOST);
            else
                ClearWindowState(FLAG_WINDOW_TOPMOST);
        }
    }
    y += 34;

    DrawText("You mined this (dust)", x, y, 12, C_DIM);
    y += 16;
    for (int i = 0; i <= 8; i++) {
        char num[4];
        char sub[16];
        snprintf(num, sizeof(num), "%d", i);
        if (i == 0)
            snprintf(sub, sizeof(sub), "no dust");
        else
            snprintf(sub, sizeof(sub), "0.0%d kg", i);

        float bx = (float)x + (float)((i % 3) * 84);
        float by = (float)y + (float)((i / 3) * 32);
        Rectangle r = {bx, by, 80, 28};
        int on = (a->brush == BRUSH_COUNT && a->count == i);
        int hot = hit(r);
        DrawRectangleRec(r, on ? C_ON : (hot ? C_BTNH : C_BTN));
        DrawRectangleLinesEx(r, 1, on ? C_HOVER : C_LINE);
        DrawText(num, (int)(r.x + 6), (int)(r.y + 6), 14, number_color(i));
        DrawText(sub, (int)(r.x + 22), (int)(r.y + 8), 10, C_TEXT);
        if (hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            a->brush = BRUSH_COUNT;
            a->count = i;
        }
    }
    y += 100;

    if (btn((Rectangle){(float)x, (float)y, 124, 26}, "Open floor (G)", a->brush == BRUSH_OPEN))
        a->brush = BRUSH_OPEN;
    if (btn((Rectangle){(float)x + 130, (float)y, 124, 26}, "Flag cave (F)", a->brush == BRUSH_FLAG))
        a->brush = BRUSH_FLAG;
    y += 30;
    if (btn((Rectangle){(float)x, (float)y, 124, 26}, "Erase (X)", a->brush == BRUSH_ERASE))
        a->brush = BRUSH_ERASE;
    if (btn((Rectangle){(float)x + 130, (float)y, 124, 26}, "Undo (U)", 0)) {
        if (hhms_undo(&a->map)) {
            a->dirty = 1;
            solve_now(a);
        }
    }
    y += 32;

    DrawText("Support (Shift-click)", x, y, 12, C_DIM);
    y += 16;
    const char *sn[4] = {"Wood", "Stone", "Beam", "Mon."};
    for (int i = 0; i < 4; i++) {
        if (btn((Rectangle){(float)x + (float)(i * 64), (float)y, 60, 24}, sn[i], a->brush == BRUSH_SUPPORT && (int)a->sup == i)) {
            a->brush = BRUSH_SUPPORT;
            a->sup = (HhmsSupportKind)i;
        }
    }
    y += 30;

    DrawText("Read the board", x, y, 12, C_DIM);
    y += 16;
    draw_swatch(x, y, C_SAFE, "DIG  mine this");
    y += 16;
    draw_swatch(x, y, C_MINE, "CAVE  will collapse");
    y += 16;
    draw_swatch(x, y, heat(0.4f), "%  cave-in chance");
    y += 16;
    draw_swatch(x, y, C_OPEN, "open  floor, no number");
    y += 16;
    draw_swatch(x, y, C_FLOOR, "0-8  you mined this");
    y += 16;
    draw_swatch(x, y, C_WALL, "gray  still rock");
    y += 18;

    if (a->map.contradiction)
        DrawText("COUNTS DISAGREE", x, y, 14, C_CONF);
    else
        DrawText("Hover a 1 to see its leftover walls.", x, y, 12, C_DIM);

    if (a->help) {
        Rectangle box = {28, 28, (float)(GetScreenWidth() - SIDE - 56), (float)(GetScreenHeight() - STATUS_H - 56)};
        if (box.width > 540) box.width = 540;
        if (box.height > 340) box.height = 340;
        DrawRectangleRec(box, (Color){16, 18, 22, 248});
        DrawRectangleLinesEx(box, 2, C_HOVER);

        int hx = (int)box.x + 18;
        int hy = (int)box.y + 16;
        DrawText("What to do (H hides this)", hx, hy, 18, C_HOVER);
        hy += 28;
        const char *lines[] = {
            "Type 0-8 only on a wall you just mined.",
            "G marks open floor with no dust (galleries).",
            "Green DIG = mine it. Red CAVE = do not.",
            "A percent is cave-in chance, not safety.",
            "Hover a number: cyan marks walls it still counts.",
            "Hover a percent: cyan marks the other walls",
            "  that share that number. That is why it is not 100%.",
            "First wall into fresh rock can cave with no warning.",
        };
        for (int i = 0; i < 8; i++) {
            DrawText(lines[i], hx, hy, 14, C_TEXT);
            hy += 22;
        }
        Rectangle dismiss_r = {box.x + box.width - 168, box.y + box.height - 38, 150, 26};
        if (btn(dismiss_r, "Got it (Esc)", 0))
            a->help = 0;
    }
}

static void draw_status(const App *a)
{
    int y = GetScreenHeight() - STATUS_H;
    DrawRectangle(0, y, GetScreenWidth(), STATUS_H, (Color){10, 12, 14, 255});
    char buf[320];
    if (a->hover_on) {
        const HhmsTile *t = hhms_get(&a->map, a->hx, a->hy);
        const char *cov = hhms_covered(&a->map, a->hx, a->hy) ? "under support" : "no support";
        if (t && t->kind == HHMS_CLEAR) {
            int left = remaining_for_clear(&a->map, a->hx, a->hy);
            snprintf(buf, sizeof(buf), "(%d,%d)  dust %d  %d wall%s still count  %s  |  %s",
                     a->hx, a->hy, t->count, left, left == 1 ? "" : "s", cov, a->msg);
        } else if (t && t->kind == HHMS_OPEN) {
            snprintf(buf, sizeof(buf), "(%d,%d)  open floor, not a number  %s  |  %s", a->hx, a->hy, cov, a->msg);
        } else if (t && (t->kind == HHMS_MINE || t->mark == HHMS_MARK_MINE)) {
            snprintf(buf, sizeof(buf), "(%d,%d)  CAVE  do not mine  %s  |  %s", a->hx, a->hy, cov, a->msg);
        } else if (t && t->mark == HHMS_MARK_SAFE) {
            snprintf(buf, sizeof(buf), "(%d,%d)  DIG  this wall is safe  %s  |  %s", a->hx, a->hy, cov, a->msg);
        } else if (t && t->p_mine > 0.f && t->p_mine < 1.f) {
            snprintf(buf, sizeof(buf), "(%d,%d)  cave-in chance %.0f%%  not proved  %s  |  %s",
                     a->hx, a->hy, t->p_mine * 100.f, cov, a->msg);
        } else if (t && t->mark == HHMS_MARK_CONFLICT) {
            snprintf(buf, sizeof(buf), "(%d,%d)  conflict  %s  |  %s", a->hx, a->hy, cov, a->msg);
        } else {
            snprintf(buf, sizeof(buf), "(%d,%d)  unknown rock  %s  |  %s", a->hx, a->hy, cov, a->msg);
        }
    } else {
        snprintf(buf, sizeof(buf), "%s", a->msg);
    }
    DrawText(buf, 10, y + 10, 14, C_TEXT);
    if (a->dirty)
        DrawText("*", GetScreenWidth() - SIDE - 18, y + 8, 16, C_HOVER);
}

int main(int argc, char **argv)
{
    static App app;
    app_init(&app);
    if (argc > 1) {
        strncpy(app.file, argv[1], sizeof(app.file) - 1);
        app.file[sizeof(app.file) - 1] = 0;
        do_load(&app);
        app.help = 0;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_TOPMOST | FLAG_VSYNC_HINT);
    InitWindow(1100, 720, "HH Minesweeper");
    SetWindowMinSize(840, 580);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        handle_keys(&app);
        handle_mouse(&app);

        BeginDrawing();
        ClearBackground(C_BG);
        draw_grid(&app);
        draw_sidebar(&app);
        draw_status(&app);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
