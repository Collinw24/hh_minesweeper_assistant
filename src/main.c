#include "hhms.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SIDE 268
#define STATUS_H 28

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

static const Color C_BG      = {16, 14, 13, 255};
static const Color C_SIDE    = {28, 24, 21, 255};
static const Color C_LINE    = {72, 64, 54, 255};
static const Color C_WALL    = {58, 52, 44, 255};
static const Color C_FLOOR   = {34, 30, 27, 255};
static const Color C_OPEN    = {92, 78, 60, 255};
static const Color C_SAFE    = {42, 96, 54, 255};
static const Color C_MINE    = {150, 42, 36, 255};
static const Color C_FLAG    = {176, 52, 40, 255};
static const Color C_CONF    = {186, 46, 150, 255};
static const Color C_HOVER   = {232, 196, 96, 255};
static const Color C_ORIGIN  = {220, 176, 72, 255};
static const Color C_TEXT    = {228, 216, 196, 255};
static const Color C_DIM     = {150, 138, 120, 255};
static const Color C_BTN     = {48, 42, 36, 255};
static const Color C_BTNH    = {70, 60, 48, 255};
static const Color C_ON      = {120, 78, 28, 255};

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
    unsigned char r = (unsigned char)(80 + p * 160);
    unsigned char g = (unsigned char)(90 - p * 50);
    unsigned char b = (unsigned char)(40);
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
    set_msg(a, "Only count mined dust (0-8); mark Natural Galleries Floor (G).");
}

static void solve_now(App *a)
{
    hhms_solve(&a->map);
    if (a->map.contradiction)
        set_msg(a, "Contradiction: a count cannot be right.");
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
    int tw = MeasureText(label, 16);
    DrawText(label, (int)(r.x + (r.width - (float)tw) * 0.5f), (int)(r.y + 7), 16, C_TEXT);
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
        set_msg(a, "Loaded.");
    } else {
        set_msg(a, "Load failed.");
    }
}

static void do_new(App *a)
{
    hhms_init(&a->map);
    a->dirty = 0;
    a->confirm_new = 0;
    set_msg(a, "New map started.");
}

static void request_new(App *a)
{
    if (a->dirty && a->map.ntiles > 0) {
        a->confirm_new = 1;
        set_msg(a, "Unsaved changes: press N again (or click Confirm) to clear.");
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
        else if (t->p_mine > 0.f)
            fill = heat(t->p_mine);
    }
    DrawRectangleRec(r, fill);

    if (hhms_covered(&a->map, x, y))
        DrawRectangleRec(r, (Color){50, 90, 160, 50});

    if (x == 0 && y == 0)
        DrawRectangleLinesEx(r, 2, C_ORIGIN);

    if (t && t->kind == HHMS_CLEAR) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", t->count);
        int fs = a->cell > 24 ? 20 : 14;
        int tw = MeasureText(buf, fs);
        DrawText(buf, (int)(sx + (a->cell - (float)tw) * 0.5f), (int)(sy + (a->cell - (float)fs) * 0.5f), fs, number_color(t->count));
    } else if (t && (t->kind == HHMS_MINE || t->mark == HHMS_MARK_MINE)) {
        int fs = a->cell > 24 ? 18 : 12;
        int tw = MeasureText("X", fs);
        DrawText("X", (int)(sx + (a->cell - (float)tw) * 0.5f), (int)(sy + (a->cell - (float)fs) * 0.5f), fs, RAYWHITE);
    } else if (t && t->mark == HHMS_MARK_NONE && t->p_mine > 0.f && t->p_mine < 1.f && a->cell >= 22) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", (int)(t->p_mine * 100.f + 0.5f));
        int tw = MeasureText(buf, 12);
        DrawText(buf, (int)(sx + (a->cell - (float)tw) * 0.5f), (int)(sy + a->cell * 0.5f - 6), 12, C_TEXT);
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
    /* grid lines */
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

static void draw_sidebar(App *a)
{
    Rectangle s = side_rect();
    DrawRectangleRec(s, C_SIDE);
    DrawLine((int)s.x, 0, (int)s.x, GetScreenHeight(), C_LINE);

    int x = (int)s.x + 12;
    int y = 10;
    DrawText("HH Minesweeper", x, y, 18, C_HOVER);
    y += 22;
    DrawText("Mined dust: 0-8   Gallery floor: G", x, y, 12, C_DIM);
    y += 20;

    Rectangle file_r = {(float)x, (float)y, s.width - 24, 24};
    DrawRectangleRec(file_r, a->edit_file ? C_BTNH : C_BTN);
    DrawRectangleLinesEx(file_r, 1, a->edit_file ? C_HOVER : C_LINE);
    const char *fname = a->edit_file ? a->file : filename_display(a->file);
    DrawText(fname, x + 6, y + 5, 14, C_TEXT);
    if (hit(file_r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        a->edit_file = 1;
    y += 30;

    if (btn((Rectangle){(float)x, (float)y, 118, 26}, "Save (S)", 0))
        do_save(a);
    if (btn((Rectangle){(float)x + 124, (float)y, 118, 26}, "Load (L)", 0))
        do_load(a);
    y += 30;

    if (a->confirm_new) {
        if (btn((Rectangle){(float)x, (float)y, 118, 26}, "Confirm New", 1))
            do_new(a);
        if (btn((Rectangle){(float)x + 124, (float)y, 118, 26}, "Cancel", 0)) {
            a->confirm_new = 0;
            set_msg(a, "New map cancelled.");
        }
    } else {
        if (btn((Rectangle){(float)x, (float)y, 118, 26}, "New (N)", 0))
            request_new(a);
        if (btn((Rectangle){(float)x + 124, (float)y, 118, 26}, a->topmost ? "Pinned (T)" : "Unpinned (T)", a->topmost)) {
            a->topmost = !a->topmost;
            if (a->topmost)
                SetWindowState(FLAG_WINDOW_TOPMOST);
            else
                ClearWindowState(FLAG_WINDOW_TOPMOST);
        }
    }
    y += 34;

    DrawText("Dust Count", x, y, 12, C_DIM);
    y += 16;
    for (int i = 0; i <= 8; i++) {
        char num[4];
        char sub[16];
        snprintf(num, sizeof(num), "%d", i);
        if (i == 0)
            snprintf(sub, sizeof(sub), "0 kg");
        else
            snprintf(sub, sizeof(sub), "0.0%d kg", i);

        float bx = (float)x + (float)((i % 3) * 82);
        float by = (float)y + (float)((i / 3) * 32);
        Rectangle r = {bx, by, 78, 28};
        int on = (a->brush == BRUSH_COUNT && a->count == i);
        int hot = hit(r);
        DrawRectangleRec(r, on ? C_ON : (hot ? C_BTNH : C_BTN));
        DrawRectangleLinesEx(r, 1, on ? C_HOVER : C_LINE);
        int nw = MeasureText(num, 14);
        DrawText(num, (int)(r.x + 8), (int)(r.y + 6), 14, number_color(i));
        DrawText(sub, (int)(r.x + 8 + nw + 6), (int)(r.y + 8), 10, C_TEXT);
        if (hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            a->brush = BRUSH_COUNT;
            a->count = i;
        }
    }
    y += 100;

    if (btn((Rectangle){(float)x, (float)y, 118, 26}, "Floor (G)", a->brush == BRUSH_OPEN))
        a->brush = BRUSH_OPEN;
    if (btn((Rectangle){(float)x + 124, (float)y, 118, 26}, "Flag (F)", a->brush == BRUSH_FLAG))
        a->brush = BRUSH_FLAG;
    y += 30;
    if (btn((Rectangle){(float)x, (float)y, 118, 26}, "Erase (X)", a->brush == BRUSH_ERASE))
        a->brush = BRUSH_ERASE;
    if (btn((Rectangle){(float)x + 124, (float)y, 118, 26}, "Undo (U)", 0)) {
        if (hhms_undo(&a->map)) {
            a->dirty = 1;
            solve_now(a);
        }
    }
    y += 32;

    DrawText("Support (Shift-Click)", x, y, 12, C_DIM);
    y += 16;
    const char *sn[4] = {"Wood", "Stone", "Beam", "Mon."};
    for (int i = 0; i < 4; i++) {
        if (btn((Rectangle){(float)x + (float)(i * 62), (float)y, 58, 26}, sn[i], a->brush == BRUSH_SUPPORT && (int)a->sup == i)) {
            a->brush = BRUSH_SUPPORT;
            a->sup = (HhmsSupportKind)i;
        }
    }
    y += 32;

    DrawText("Legend", x, y, 12, C_DIM);
    y += 16;
    DrawRectangle(x, y, 12, 12, C_OPEN);
    DrawText("floor", x + 16, y, 11, C_TEXT);
    DrawRectangle(x + 72, y, 12, 12, C_FLOOR);
    DrawText("cleared #", x + 88, y, 11, C_TEXT);
    DrawRectangle(x + 164, y, 12, 12, C_MINE);
    DrawText("cave-in", x + 180, y, 11, C_TEXT);
    y += 16;
    DrawRectangle(x, y, 12, 12, C_SAFE);
    DrawText("safe", x + 16, y, 11, C_TEXT);
    DrawRectangle(x + 72, y, 12, 12, heat(0.45f));
    DrawText("odds", x + 88, y, 11, C_TEXT);
    DrawRectangle(x + 164, y, 12, 12, C_CONF);
    DrawText("conflict", x + 180, y, 11, C_TEXT);
    y += 16;
    DrawRectangle(x, y, 12, 12, (Color){50, 90, 160, 180});
    DrawText("supported", x + 16, y, 11, C_TEXT);
    y += 20;

    if (a->map.contradiction) {
        DrawText("COUNTS DISAGREE", x, y, 13, C_CONF);
        y += 18;
    }

    DrawText("H help   T pin   U undo", x, y, 11, C_DIM);
    y += 14;
    DrawText("0-8 count   G floor   F flag   X erase", x, y, 11, C_DIM);
    y += 14;
    DrawText("shift-click support   mid-drag / arrows", x, y, 11, C_DIM);

    if (a->help) {
        Rectangle box = {30, 30, (float)(GetScreenWidth() - SIDE - 60), (float)(GetScreenHeight() - STATUS_H - 60)};
        if (box.width > 560) box.width = 560;
        if (box.height > 360) box.height = 360;
        DrawRectangleRec(box, (Color){20, 18, 16, 245});
        DrawRectangleLinesEx(box, 2, C_HOVER);

        int hx = (int)box.x + 20;
        int hy = (int)box.y + 18;
        DrawText("Quick Start Guide (H to toggle)", hx, hy, 18, C_HOVER);
        hy += 28;

        const char *lines[] = {
            "1. Only type 0-8 on tiles you mined",
            "   and whose dust you picked up.",
            "2. Natural Gallery rooms have no dust:",
            "   they must be marked Floor (G), never 0.",
            "3. The first tile into a wall can cave in with no warning.",
            "4. Water can hide cave-ins.",
            "5. Green tiles are safe next; red tiles are cave-ins.",
            "6. Right-click flags cave-ins; Shift-click places supports.",
            "7. This is a live notebook; it never connects to H&H.",
        };
        for (int i = 0; i < 9; i++) {
            DrawText(lines[i], hx, hy, 13, C_TEXT);
            hy += 24;
        }

        Rectangle dismiss_r = {box.x + box.width - 170, box.y + box.height - 40, 150, 26};
        if (btn(dismiss_r, "Got It (Esc/H)", 0))
            a->help = 0;
    }
}

static void draw_status(const App *a)
{
    int y = GetScreenHeight() - STATUS_H;
    DrawRectangle(0, y, GetScreenWidth(), STATUS_H, (Color){12, 10, 9, 255});
    char buf[256];
    if (a->hover_on) {
        const HhmsTile *t = hhms_get(&a->map, a->hx, a->hy);
        const char *cov = hhms_covered(&a->map, a->hx, a->hy) ? "supported" : "unsupported";
        char kind_buf[32];
        const char *kind_str = "unmined";
        if (t) {
            if (t->mark == HHMS_MARK_CONFLICT)
                kind_str = "conflict";
            else if (t->kind == HHMS_OPEN)
                kind_str = "floor";
            else if (t->kind == HHMS_CLEAR) {
                snprintf(kind_buf, sizeof(kind_buf), "cleared (dust %d)", t->count);
                kind_str = kind_buf;
            } else if (t->kind == HHMS_MINE || t->mark == HHMS_MARK_MINE)
                kind_str = "cave-in";
            else if (t->mark == HHMS_MARK_SAFE)
                kind_str = "safe";
            else if (t->p_mine > 0.f)
                kind_str = "odds";
        }
        if (a->hx == 0 && a->hy == 0) {
            if (t && t->p_mine >= 0.f && t->kind != HHMS_OPEN && t->kind != HHMS_CLEAR && t->kind != HHMS_MINE)
                snprintf(buf, sizeof(buf), "Tile (0,0) [Origin]  %s  p=%.0f%%  %s | %s", kind_str, t->p_mine * 100.f, cov, a->msg);
            else
                snprintf(buf, sizeof(buf), "Tile (0,0) [Origin]  %s  %s | %s", kind_str, cov, a->msg);
        } else {
            if (t && t->p_mine >= 0.f && t->kind != HHMS_OPEN && t->kind != HHMS_CLEAR && t->kind != HHMS_MINE)
                snprintf(buf, sizeof(buf), "Tile (%d,%d)  %s  p=%.0f%%  %s | %s", a->hx, a->hy, kind_str, t->p_mine * 100.f, cov, a->msg);
            else
                snprintf(buf, sizeof(buf), "Tile (%d,%d)  %s  %s | %s", a->hx, a->hy, kind_str, cov, a->msg);
        }
    } else {
        snprintf(buf, sizeof(buf), "%s", a->msg);
    }
    DrawText(buf, 10, y + 6, 14, C_TEXT);
    if (a->dirty)
        DrawText("*", GetScreenWidth() - SIDE - 20, y + 6, 16, C_HOVER);
}

int main(int argc, char **argv)
{
    static App app;
    app_init(&app);
    if (argc > 1) {
        strncpy(app.file, argv[1], sizeof(app.file) - 1);
        app.file[sizeof(app.file) - 1] = 0;
        do_load(&app);
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_TOPMOST | FLAG_VSYNC_HINT);
    InitWindow(1100, 720, "HH Minesweeper");
    SetWindowMinSize(820, 560);
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
