#include "gui.h"

#include "dialogs.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HHMS_GUI_VERSION HHMS_VERSION
#define CONTROL_MAX 48

static const Color C_BG = {14, 16, 18, 255};
static const Color C_SIDE = {22, 24, 28, 255};
static const Color C_STATUS = {10, 12, 14, 255};
static const Color C_LINE = {72, 76, 84, 255};
static const Color C_WALL = {74, 78, 84, 255};
static const Color C_FLOOR = {30, 32, 36, 255};
static const Color C_OPEN = {58, 88, 108, 255};
static const Color C_WATER = {36, 74, 98, 255};
static const Color C_SAFE = {45, 126, 72, 255};
static const Color C_MINE = {172, 52, 48, 255};
static const Color C_FLAG = {210, 67, 55, 255};
static const Color C_CONFLICT = {191, 63, 158, 255};
static const Color C_HOVER = {242, 204, 86, 255};
static const Color C_LINK = {82, 194, 231, 255};
static const Color C_ORIGIN = {229, 178, 69, 255};
static const Color C_TEXT = {236, 231, 220, 255};
static const Color C_DIM = {159, 165, 174, 255};
static const Color C_BTN = {42, 46, 52, 255};
static const Color C_BTN_HOVER = {59, 65, 73, 255};
static const Color C_SELECTED = {113, 78, 31, 255};
static const Color C_DISABLED = {31, 34, 38, 255};
static const Color C_DISABLED_TEXT = {92, 97, 104, 255};
static const Color C_CONFIRMED = {64, 125, 190, 54};
static const Color C_ESTIMATED = {206, 165, 65, 220};

static const char *SUPPORT_LABELS[HHMS_SUP_COUNT] = {
    "Wood", "Stone", "Beam", "Monument", "Timber tunnel",
    "Reinforced tunnel", "Stone arch"
};
static const char *ORIENTATION_LABELS[4] = {"North", "East", "South", "West"};
static const char *SUPPORT_CODES[HHMS_SUP_COUNT] = {"W", "S", "B", "M", "T", "R", "A"};

typedef enum {
    CTL_NEW = 1, CTL_OPEN, CTL_SAVE, CTL_SAVE_AS, CTL_UNDO, CTL_REDO,
    CTL_FIT, CTL_ORIGIN, CTL_PIN, CTL_HELP,
    CTL_COUNT_0 = 100,
    CTL_BRUSH_OPEN = 120, CTL_BRUSH_WATER, CTL_BRUSH_FLAG, CTL_BRUSH_ERASE,
    CTL_SUPPORT_0 = 140
} ControlId;

typedef struct {
    int id;
    Rectangle rect;
    const char *label;
    int selected;
    int enabled;
} Control;

typedef struct {
    Rectangle grid;
    Rectangle side;
    Rectangle status;
    Control controls[CONTROL_MAX];
    int count;
    float content_height;
    float scale;
} GuiLayout;

static float clampf(float value, float lo, float hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

static int primary_down(void)
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
           IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
}

static int shift_down(void)
{
    return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
}

static int point_in(Rectangle rectangle, Vector2 point)
{
    return CheckCollisionPointRec(point, rectangle);
}

static void add_control(GuiLayout *layout, int id, Rectangle rect, const char *label,
                        int selected, int enabled)
{
    if (layout->count >= CONTROL_MAX)
        return;
    Control *control = &layout->controls[layout->count++];
    control->id = id;
    control->rect = rect;
    control->label = label;
    control->selected = selected;
    control->enabled = enabled;
}

static GuiLayout make_layout(const HhmsGui *gui)
{
    GuiLayout layout;
    memset(&layout, 0, sizeof(layout));
    float scale = clampf(gui->app->view.ui_scale, 0.75f, 1.5f);
    float sidebar = clampf(302.0f * scale, 236.0f, GetScreenWidth() * 0.46f);
    float status = clampf(58.0f * scale, 54.0f, 82.0f);
    layout.scale = scale;
    layout.grid = (Rectangle){0, 0, (float)GetScreenWidth() - sidebar,
                              (float)GetScreenHeight() - status};
    layout.side = (Rectangle){layout.grid.width, 0, sidebar, (float)GetScreenHeight()};
    layout.status = (Rectangle){0, layout.grid.height, layout.grid.width, status};

    float pad = 12.0f * scale;
    float gap = 6.0f * scale;
    float x = layout.side.x + pad;
    float width = layout.side.width - 2.0f * pad;
    float half = (width - gap) * 0.5f;
    float button_h = 27.0f * scale;
    float y = 86.0f * scale - gui->sidebar_scroll;
#define ADD2(id1, label1, sel1, en1, id2, label2, sel2, en2) \
    do { \
        add_control(&layout, id1, (Rectangle){x, y, half, button_h}, label1, sel1, en1); \
        add_control(&layout, id2, (Rectangle){x + half + gap, y, half, button_h}, label2, sel2, en2); \
        y += button_h + gap; \
    } while (0)
    ADD2(CTL_NEW, "New", 0, 1, CTL_OPEN, "Open...", 0, 1);
    ADD2(CTL_SAVE, "Save", 0, 1, CTL_SAVE_AS, "Save As...", 0, 1);
    ADD2(CTL_UNDO, "Undo", 0, hhms_can_undo(&gui->app->map),
         CTL_REDO, "Redo", 0, hhms_can_redo(&gui->app->map));
    ADD2(CTL_FIT, "Fit map", 0, 1, CTL_ORIGIN, "Origin", 0, 1);
    ADD2(CTL_PIN, gui->topmost ? "Pinned" : "Unpinned", gui->topmost, 1,
         CTL_HELP, "Help", gui->help, 1);
    y += 24.0f * scale;
    for (int i = 0; i <= 8; i++) {
        int column = i % 3;
        int row = i / 3;
        float w = (width - gap * 2.0f) / 3.0f;
        Rectangle r = {x + column * (w + gap), y + row * (button_h + gap), w, button_h};
        static const char *labels[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8"};
        add_control(&layout, CTL_COUNT_0 + i, r, labels[i],
                    gui->brush == HHMS_BRUSH_COUNT && gui->count == i, 1);
    }
    y += 3.0f * (button_h + gap) + 24.0f * scale;
    ADD2(CTL_BRUSH_OPEN, "Open floor (G)", gui->brush == HHMS_BRUSH_OPEN, 1,
         CTL_BRUSH_WATER, "Water (V)", gui->brush == HHMS_BRUSH_WATER, 1);
    ADD2(CTL_BRUSH_FLAG, "Authored cave (F)", gui->brush == HHMS_BRUSH_FLAG, 1,
         CTL_BRUSH_ERASE, "Erase (X)", gui->brush == HHMS_BRUSH_ERASE, 1);
    y += 24.0f * scale;
    for (int i = 0; i < HHMS_SUP_COUNT; i++) {
        Rectangle r = {x, y, width, button_h};
        add_control(&layout, CTL_SUPPORT_0 + i, r, SUPPORT_LABELS[i],
                    gui->brush == HHMS_BRUSH_SUPPORT &&
                    gui->support_kind == (HhmsSupportKind)i, 1);
        y += button_h + gap;
    }
    layout.content_height = y + gui->sidebar_scroll + 126.0f * scale;
#undef ADD2
    return layout;
}

static void tile_center(const HhmsGui *gui, Rectangle grid, int x, int y,
                        float *sx, float *sy)
{
    *sx = grid.x + grid.width * 0.5f + ((float)x - (float)gui->app->view.camx) * gui->app->view.cell;
    *sy = grid.y + grid.height * 0.5f + ((float)y - (float)gui->app->view.camy) * gui->app->view.cell;
}

static void map_to_screen(const HhmsGui *gui, Rectangle grid, double x, double y,
                          float *sx, float *sy)
{
    *sx = grid.x + grid.width * 0.5f + (float)(x - gui->app->view.camx) * gui->app->view.cell;
    *sy = grid.y + grid.height * 0.5f + (float)(y - gui->app->view.camy) * gui->app->view.cell;
}

static void screen_to_map(const HhmsGui *gui, Rectangle grid, Vector2 point,
                          double *x, double *y)
{
    *x = gui->app->view.camx + (point.x - (grid.x + grid.width * 0.5f)) / gui->app->view.cell;
    *y = gui->app->view.camy + (point.y - (grid.y + grid.height * 0.5f)) / gui->app->view.cell;
}

static int screen_to_tile(const HhmsGui *gui, Rectangle grid, Vector2 point,
                          int *x, int *y)
{
    if (!point_in(grid, point))
        return 0;
    double map_x, map_y;
    screen_to_map(gui, grid, point, &map_x, &map_y);
    *x = (int)floor(map_x + 0.5);
    *y = (int)floor(map_y + 0.5);
    return 1;
}

static void update_title(HhmsGui *gui)
{
    char title[sizeof(gui->title)];
    snprintf(title, sizeof(title), "%s%s — HH Minesweeper %s",
             hhms_app_filename(gui->app), hhms_app_dirty(gui->app) ? " *" : "",
             HHMS_GUI_VERSION);
    if (strcmp(title, gui->title) != 0) {
        snprintf(gui->title, sizeof(gui->title), "%s", title);
        SetWindowTitle(gui->title);
    }
}
static int default_font_filename_safe(const char *value)
{
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p < 32 || *p > 126)
            return 0;
    }
    return 1;
}


static void edit_error(HhmsGui *gui, HhmsEditResult result)
{
    if (result == HHMS_EDIT_MAP_FULL)
        hhms_app_set_message(gui->app, "Map limit reached (8192 authored tiles). Erase a tile before adding another.");
    else if (result == HHMS_EDIT_SUPPORTS_FULL)
        hhms_app_set_message(gui->app, "Support limit reached (256). Remove a support before adding another.");
    else if (result == HHMS_EDIT_HISTORY_FULL)
        hhms_app_set_message(gui->app, "This stroke is too large for one Undo; it was cancelled.");
    else if (result == HHMS_EDIT_INVALID)
        hhms_app_set_message(gui->app, "That terrain or observation is invalid here; erase the existing state first.");
}

static HhmsEditResult apply_brush(HhmsGui *gui, int x, int y, HhmsBrush brush)
{
    const HhmsTile *tile;
    switch (brush) {
    case HHMS_BRUSH_COUNT:
        return hhms_set_tile(&gui->app->map, x, y, HHMS_CLEAR, gui->count);
    case HHMS_BRUSH_OPEN:
        return hhms_set_tile(&gui->app->map, x, y, HHMS_OPEN, 0);
    case HHMS_BRUSH_WATER:
        return hhms_set_terrain(&gui->app->map, x, y, HHMS_TERRAIN_WATER);
    case HHMS_BRUSH_FLAG:
        tile = hhms_get(&gui->app->map, x, y);
        return tile && tile->kind == HHMS_MINE
            ? hhms_set_tile(&gui->app->map, x, y, HHMS_UNKNOWN, 0)
            : hhms_set_tile(&gui->app->map, x, y, HHMS_MINE, 0);
    case HHMS_BRUSH_ERASE:
        return hhms_erase_tile(&gui->app->map, x, y);
    default:
        return HHMS_EDIT_INVALID;
    }
}

static void finish_single_edit(HhmsGui *gui, HhmsEditResult result)
{
    if (result == HHMS_EDIT_OK || result == HHMS_EDIT_NO_CHANGE)
        hhms_app_commit_edit(gui->app);
    else {
        hhms_app_cancel_edit(gui->app);
        edit_error(gui, result);
    }
}
static void finish_painting(HhmsGui *gui)
{
    if (!gui->painting)
        return;
    hhms_app_commit_edit(gui->app);
    gui->painting = 0;
}


static void single_tile_edit(HhmsGui *gui, int x, int y, HhmsBrush brush)
{
    if (!hhms_app_begin_edit(gui->app))
        return;
    finish_single_edit(gui, apply_brush(gui, x, y, brush));
    gui->remove_warning_id = 0;
}

static void raster_segment(HhmsGui *gui, int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    int first = 1;
    for (;;) {
        if (!first) {
            HhmsEditResult result = apply_brush(gui, x0, y0, gui->brush);
            if (result != HHMS_EDIT_OK && result != HHMS_EDIT_NO_CHANGE) {
                hhms_app_cancel_edit(gui->app);
                edit_error(gui, result);
                gui->painting = 0;
                return;
            }
        }
        first = 0;
        if (x0 == x1 && y0 == y1)
            break;
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

static void support_edit(HhmsGui *gui, Rectangle grid, Vector2 point)
{
    double x, y;
    screen_to_map(gui, grid, point, &x, &y);
    int index = hhms_support_index_near(&gui->app->map, x, y,
                                        fmax(0.18, 9.0 / gui->app->view.cell));
    if (index >= 0) {
        HhmsSupport support = gui->app->map.supports[index];
        gui->selected_support_id = support.id;
        if (support.kind == gui->support_kind) {
            int exposed = hhms_support_exposed_caves(&gui->app->map,
                                                      gui->app->analysis_valid ? &gui->app->analysis : NULL,
                                                      support.id);
            if (exposed > 0 && gui->remove_warning_id != support.id) {
                char message[192];
                snprintf(message, sizeof(message),
                         "Removal would leave %d known cave%s without modeled radial coverage. Click this support again to remove it.",
                         exposed, exposed == 1 ? "" : "s");
                hhms_app_set_message(gui->app, message);
                gui->remove_warning_id = support.id;
                return;
            }
            if (!hhms_app_begin_edit(gui->app))
                return;
            finish_single_edit(gui, hhms_del_support(&gui->app->map, support.id));
            gui->selected_support_id = 0;
            gui->remove_warning_id = 0;
            return;
        }
        if (!hhms_app_begin_edit(gui->app))
            return;
        finish_single_edit(gui, hhms_update_support(&gui->app->map, support.id,
                                                    support.x, support.y,
                                                    gui->support_kind, gui->orientation));
        gui->selected_support_id = support.id;
        gui->remove_warning_id = 0;
        return;
    }
    if (!hhms_app_begin_edit(gui->app))
        return;
    uint32_t id = 0;
    finish_single_edit(gui, hhms_add_support(&gui->app->map, x, y, gui->support_kind,
                                             gui->orientation, &id));
    gui->selected_support_id = id;
    gui->remove_warning_id = 0;
}

static void reset_document_ui(HhmsGui *gui)
{
    gui->cursor_x = 0;
    gui->cursor_y = 0;
    gui->cursor_set = 1;
    gui->remove_warning_id = 0;
    gui->selected_support_id = 0;
}

static void request_new(HhmsGui *gui)
{
    finish_painting(gui);
    if (hhms_app_request_new(gui->app) == HHMS_APP_OK)
        reset_document_ui(gui);
}

static void request_save(HhmsGui *gui, int force_path)
{
    finish_painting(gui);
    HhmsAppResult result = force_path ? HHMS_APP_NEEDS_SAVE_PATH : hhms_app_save(gui->app);
    if (result != HHMS_APP_NEEDS_SAVE_PATH)
        return;
    char *path = NULL;
    char error[HHMS_APP_MESSAGE_MAX];
    HhmsDialogResult dialog = hhms_dialog_save(&path, gui->topmost,
                                                hhms_app_filename(gui->app), error, sizeof(error));
    if (dialog == HHMS_DIALOG_OK) {
        hhms_app_save_as(gui->app, path);
        hhms_dialog_free(path);
    } else if (dialog == HHMS_DIALOG_ERROR) {
        hhms_app_set_message(gui->app, error);
    }
}

static void request_open(HhmsGui *gui)
{
    finish_painting(gui);
    char *path = NULL;
    char error[HHMS_APP_MESSAGE_MAX];
    HhmsDialogResult result = hhms_dialog_open(&path, gui->topmost, error, sizeof(error));
    if (result == HHMS_DIALOG_OK) {
        hhms_gui_request_load(gui, path);
        hhms_dialog_free(path);
    } else if (result == HHMS_DIALOG_ERROR) {
        hhms_app_set_message(gui->app, error);
    }
}

void hhms_gui_request_load(HhmsGui *gui, const char *path)
{
    finish_painting(gui);
    HhmsAppResult result = hhms_app_request_load(gui->app, path);
    if (result == HHMS_APP_OK && gui->app->fit_after_load) {
        GuiLayout layout = make_layout(gui);
        hhms_app_fit_view(gui->app, layout.grid.width, layout.grid.height, 0);
        hhms_app_accept_loaded_fit(gui->app);
    }
    if (result == HHMS_APP_OK) {
        reset_document_ui(gui);
    }
}

static void resolve_unsaved(HhmsGui *gui, HhmsUnsavedChoice choice)
{
    HhmsPendingAction pending = gui->app->pending;
    if (choice == HHMS_UNSAVED_SAVE && !gui->app->path) {
        char *path = NULL;
        char error[HHMS_APP_MESSAGE_MAX];
        HhmsDialogResult dialog = hhms_dialog_save(&path, gui->topmost,
                                                    hhms_app_filename(gui->app), error, sizeof(error));
        if (dialog == HHMS_DIALOG_CANCEL)
            return;
        if (dialog == HHMS_DIALOG_ERROR) {
            hhms_app_set_message(gui->app, error);
            return;
        }
        HhmsAppResult result = hhms_app_resolve_unsaved(gui->app, choice, path);
        hhms_dialog_free(path);
        if (result != HHMS_APP_QUIT)
            gui->close_latched = 0;
        if (result == HHMS_APP_OK && (pending == HHMS_PENDING_NEW || pending == HHMS_PENDING_LOAD))
            reset_document_ui(gui);
        return;
    }
    HhmsAppResult result = hhms_app_resolve_unsaved(gui->app, choice, NULL);
    if (result != HHMS_APP_QUIT)
        gui->close_latched = 0;
    if (result == HHMS_APP_OK && (pending == HHMS_PENDING_NEW || pending == HHMS_PENDING_LOAD))
        reset_document_ui(gui);
}

static Rectangle modal_button(int index)
{
    float width = fminf(520.0f, GetScreenWidth() - 40.0f);
    float x = (GetScreenWidth() - width) * 0.5f;
    float y = GetScreenHeight() * 0.5f + 34.0f;
    float gap = 8.0f;
    float button = (width - 36.0f - 2.0f * gap) / 3.0f;
    return (Rectangle){x + 18.0f + index * (button + gap), y, button, 32.0f};
}

static void process_modal(HhmsGui *gui)
{
    if (IsKeyPressed(KEY_TAB)) {
        int direction = shift_down() ? -1 : 1;
        gui->modal_focus = (gui->modal_focus + direction + 3) % 3;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        resolve_unsaved(gui, HHMS_UNSAVED_CANCEL);
        return;
    }
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < 3; i++) {
        if (point_in(modal_button(i), mouse) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            gui->modal_focus = i;
            resolve_unsaved(gui, i == 0 ? HHMS_UNSAVED_SAVE :
                            (i == 1 ? HHMS_UNSAVED_DISCARD : HHMS_UNSAVED_CANCEL));
            return;
        }
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        resolve_unsaved(gui, gui->modal_focus == 0 ? HHMS_UNSAVED_SAVE :
                        (gui->modal_focus == 1 ? HHMS_UNSAVED_DISCARD : HHMS_UNSAVED_CANCEL));
}

static void activate_control(HhmsGui *gui, int id, GuiLayout layout)
{
    finish_painting(gui);
    gui->remove_warning_id = 0;
    if (id >= CTL_COUNT_0 && id <= CTL_COUNT_0 + 8) {
        gui->brush = HHMS_BRUSH_COUNT;
        gui->count = id - CTL_COUNT_0;
        return;
    }
    if (id >= CTL_SUPPORT_0 && id < CTL_SUPPORT_0 + HHMS_SUP_COUNT) {
        gui->brush = HHMS_BRUSH_SUPPORT;
        gui->support_kind = (HhmsSupportKind)(id - CTL_SUPPORT_0);
        gui->remove_warning_id = 0;
        return;
    }
    switch (id) {
    case CTL_NEW: request_new(gui); break;
    case CTL_OPEN: request_open(gui); break;
    case CTL_SAVE: request_save(gui, 0); break;
    case CTL_SAVE_AS: request_save(gui, 1); break;
    case CTL_UNDO: hhms_app_undo(gui->app); break;
    case CTL_REDO: hhms_app_redo(gui->app); break;
    case CTL_FIT: hhms_app_fit_view(gui->app, layout.grid.width, layout.grid.height, 1); break;
    case CTL_ORIGIN: hhms_app_reset_view(gui->app); break;
    case CTL_PIN:
        gui->topmost = !gui->topmost;
        if (gui->topmost) SetWindowState(FLAG_WINDOW_TOPMOST);
        else ClearWindowState(FLAG_WINDOW_TOPMOST);
        break;
    case CTL_HELP: gui->help = 1; break;
    case CTL_BRUSH_OPEN: gui->brush = HHMS_BRUSH_OPEN; break;
    case CTL_BRUSH_WATER: gui->brush = HHMS_BRUSH_WATER; break;
    case CTL_BRUSH_FLAG: gui->brush = HHMS_BRUSH_FLAG; break;
    case CTL_BRUSH_ERASE: gui->brush = HHMS_BRUSH_ERASE; break;
    default: break;
    }
}


static void keyboard_target(const HhmsGui *gui, int *x, int *y)
{
    if (gui->hover_board) {
        *x = gui->hover_x;
        *y = gui->hover_y;
    } else {
        *x = gui->cursor_x;
        *y = gui->cursor_y;
    }
}

static int process_shortcuts(HhmsGui *gui, GuiLayout layout)
{
    int primary = primary_down();
    int shift = shift_down();
    if (primary && (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))) {
        finish_painting(gui);
        gui->app->view.ui_scale = clampf(gui->app->view.ui_scale + 0.1f, 0.75f, 1.5f);
        return 1;
    }
    if (primary && (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))) {
        finish_painting(gui);
        gui->app->view.ui_scale = clampf(gui->app->view.ui_scale - 0.1f, 0.75f, 1.5f);
        return 1;
    }
    if (primary && IsKeyPressed(KEY_S)) { request_save(gui, 0); return 1; }
    if (primary && IsKeyPressed(KEY_O)) { request_open(gui); return 1; }
    if (primary && IsKeyPressed(KEY_N)) { request_new(gui); return 1; }
    if ((primary && shift && IsKeyPressed(KEY_Z)) || IsKeyPressed(KEY_Y)) {
        finish_painting(gui);
        hhms_app_redo(gui->app); return 1;
    }
    if ((primary && IsKeyPressed(KEY_Z)) || IsKeyPressed(KEY_U)) {
        finish_painting(gui);
        hhms_app_undo(gui->app); return 1;
    }
    if (shift && IsKeyPressed(KEY_R)) {
        finish_painting(gui);
        hhms_app_fit_view(gui->app, layout.grid.width, layout.grid.height, 1); return 1;
    }
    if (IsKeyPressed(KEY_R)) {
        finish_painting(gui);
        hhms_app_reset_view(gui->app);
        return 1;
    }
    if (IsKeyPressed(KEY_S)) { request_save(gui, 0); return 1; }
    if (IsKeyPressed(KEY_L)) { request_open(gui); return 1; }
    if (IsKeyPressed(KEY_N)) { request_new(gui); return 1; }
    if (IsKeyPressed(KEY_T)) { activate_control(gui, CTL_PIN, layout); return 1; }
    if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_SLASH)) {
        finish_painting(gui);
        gui->help = 1;
        return 1;
    }

    if (IsKeyPressed(KEY_COMMA) || IsKeyPressed(KEY_PERIOD)) {
        finish_painting(gui);
        int direction = IsKeyPressed(KEY_PERIOD) ? 1 : -1;
        gui->orientation = (HhmsOrientation)(((int)gui->orientation + direction + 4) % 4);
        gui->remove_warning_id = 0;
        return 1;
    }

    int target_x, target_y;
    keyboard_target(gui, &target_x, &target_y);
    for (int digit = 0; digit <= 8; digit++) {
        int pressed = IsKeyPressed(KEY_ZERO + digit) || IsKeyPressed(KEY_KP_0 + digit);
        if (pressed) {
            finish_painting(gui);
            gui->brush = HHMS_BRUSH_COUNT;
            gui->count = digit;
            single_tile_edit(gui, target_x, target_y, gui->brush);
            return 1;
        }
    }
    if (IsKeyPressed(KEY_G) || IsKeyPressed(KEY_V) || IsKeyPressed(KEY_F) ||
        IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) {
        finish_painting(gui);
        if (IsKeyPressed(KEY_G)) gui->brush = HHMS_BRUSH_OPEN;
        else if (IsKeyPressed(KEY_V)) gui->brush = HHMS_BRUSH_WATER;
        else if (IsKeyPressed(KEY_F)) gui->brush = HHMS_BRUSH_FLAG;
        else gui->brush = HHMS_BRUSH_ERASE;
        single_tile_edit(gui, target_x, target_y, gui->brush);
        return 1;
    }
    if (IsKeyPressed(KEY_LEFT_BRACKET) || IsKeyPressed(KEY_RIGHT_BRACKET)) {
        finish_painting(gui);
        int direction = IsKeyPressed(KEY_RIGHT_BRACKET) ? 1 : -1;
        gui->support_kind = (HhmsSupportKind)(((int)gui->support_kind + direction + HHMS_SUP_COUNT) % HHMS_SUP_COUNT);
        gui->brush = HHMS_BRUSH_SUPPORT;
        gui->remove_warning_id = 0;
        return 1;
    }
    return 0;
}

static void process_board(HhmsGui *gui, GuiLayout layout)
{
    Vector2 mouse = GetMousePosition();
    gui->hover_board = screen_to_tile(gui, layout.grid, mouse, &gui->hover_x, &gui->hover_y);
    if (gui->hover_board && !gui->cursor_set) {
        gui->cursor_x = gui->hover_x;
        gui->cursor_y = gui->hover_y;
        gui->cursor_set = 1;
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && point_in(layout.side, mouse)) {
        float maximum = fmaxf(0.0f, layout.content_height - layout.side.height + 18.0f * layout.scale);
        gui->sidebar_scroll = clampf(gui->sidebar_scroll - wheel * 42.0f * layout.scale, 0.0f, maximum);
    } else if (wheel != 0.0f && gui->hover_board) {
        double anchor_x, anchor_y;
        screen_to_map(gui, layout.grid, mouse, &anchor_x, &anchor_y);
        float old_cell = gui->app->view.cell;
        gui->app->view.cell = clampf(old_cell * powf(1.12f, wheel), 14.0f, 72.0f);
        gui->app->view.camx = anchor_x - (mouse.x - (layout.grid.x + layout.grid.width * 0.5f)) / gui->app->view.cell;
        gui->app->view.camy = anchor_y - (mouse.y - (layout.grid.y + layout.grid.height * 0.5f)) / gui->app->view.cell;
    }

    int pan_now = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ||
                  (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_SPACE));
    if (pan_now && point_in(layout.grid, mouse)) {
        gui->space_board_pending = 0;
        Vector2 delta = GetMouseDelta();
        gui->app->view.camx -= delta.x / gui->app->view.cell;
        gui->app->view.camy -= delta.y / gui->app->view.cell;
        gui->dragging_pan = 1;
        return;
    }
    gui->dragging_pan = 0;

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && gui->hover_board) {
        gui->focus = 0;
        gui->cursor_x = gui->hover_x;
        gui->cursor_y = gui->hover_y;
        gui->cursor_set = 1;
        single_tile_edit(gui, gui->hover_x, gui->hover_y, HHMS_BRUSH_FLAG);
        return;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && gui->hover_board) {
        gui->focus = 0;
        gui->cursor_x = gui->hover_x;
        gui->cursor_y = gui->hover_y;
        gui->cursor_set = 1;
        if (gui->brush == HHMS_BRUSH_SUPPORT || shift_down()) {
            support_edit(gui, layout.grid, mouse);
            gui->painting = 0;
        } else if (hhms_app_begin_edit(gui->app)) {
            HhmsEditResult result = apply_brush(gui, gui->hover_x, gui->hover_y, gui->brush);
            if (result == HHMS_EDIT_OK || result == HHMS_EDIT_NO_CHANGE) {
                gui->painting = 1;
                gui->paint_x = gui->hover_x;
                gui->paint_y = gui->hover_y;
                gui->remove_warning_id = 0;
            } else {
                edit_error(gui, result);
                hhms_app_cancel_edit(gui->app);
            }
        }
    }
    if (gui->painting && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && gui->hover_board &&
        (gui->paint_x != gui->hover_x || gui->paint_y != gui->hover_y)) {
        raster_segment(gui, gui->paint_x, gui->paint_y, gui->hover_x, gui->hover_y);
        gui->paint_x = gui->hover_x;
        gui->paint_y = gui->hover_y;
    }
    if (gui->painting && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        hhms_app_commit_edit(gui->app);
        gui->painting = 0;
    }
}

static int has_hhmap_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot)
        return 0;
    const char *wanted = ".hhmap";
    while (*dot && *wanted) {
        if (tolower((unsigned char)*dot) != *wanted)
            return 0;
        dot++;
        wanted++;
    }
    return *dot == '\0' && *wanted == '\0';
}

static void process_drop(HhmsGui *gui)
{
    if (!IsFileDropped())
        return;
    FilePathList dropped = LoadDroppedFiles();
    char *copy = NULL;
    if (dropped.count > 0 && dropped.paths[0]) {
        size_t length = strlen(dropped.paths[0]) + 1;
        copy = (char *)malloc(length);
        if (copy)
            memcpy(copy, dropped.paths[0], length);
    }
    UnloadDroppedFiles(dropped);
    if (!copy) {
        hhms_app_set_message(gui->app, "Dropped file could not be copied; try Open instead.");
        return;
    }
    if (!has_hhmap_extension(copy))
        hhms_app_set_message(gui->app, "Only .hhmap files can be loaded here.");
    else
        hhms_gui_request_load(gui, copy);
    free(copy);
}

void hhms_gui_init(HhmsGui *gui, HhmsApp *app, int show_first_run_help)
{
    memset(gui, 0, sizeof(*gui));
    gui->app = app;
    gui->brush = HHMS_BRUSH_COUNT;
    gui->support_kind = HHMS_SUP_WOOD;
    gui->orientation = HHMS_ORIENT_NORTH;
    gui->topmost = 1;
    gui->help = show_first_run_help;
    gui->cursor_set = 1;
    gui->title[0] = '\0';
    update_title(gui);
}

void hhms_gui_process(HhmsGui *gui)
{
    GuiLayout layout = make_layout(gui);
    if (gui->app->fit_after_load) {
        hhms_app_fit_view(gui->app, layout.grid.width, layout.grid.height, 0);
        hhms_app_accept_loaded_fit(gui->app);
    }
    if (WindowShouldClose() && !gui->close_latched) {
        finish_painting(gui);
        HhmsAppResult result = hhms_app_request_close(gui->app);
        if (result == HHMS_APP_NEEDS_CONFIRMATION) {
            gui->close_latched = 1;
            PollInputEvents();
        }
    }
    update_title(gui);
    if (gui->app->quit)
        return;
    if (gui->app->pending != HHMS_PENDING_NONE) {
        process_modal(gui);
        update_title(gui);
        return;
    }
    if (gui->help) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
            IsKeyPressed(KEY_H) || IsKeyPressed(KEY_SLASH) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            gui->help = 0;
        update_title(gui);
        return;
    }

    process_drop(gui);
    if (gui->app->pending != HHMS_PENDING_NONE)
        return;
    layout = make_layout(gui);
    float maximum_scroll = fmaxf(0.0f, layout.content_height - layout.side.height +
                                  18.0f * layout.scale);
    gui->sidebar_scroll = clampf(gui->sidebar_scroll, 0.0f, maximum_scroll);
    layout = make_layout(gui);
    if (gui->focus > 0 &&
        (gui->focus > layout.count || !layout.controls[gui->focus - 1].enabled))
        gui->focus = 0;
    if (IsKeyPressed(KEY_TAB)) {
        int direction = shift_down() ? -1 : 1;
        do {
            gui->focus = (gui->focus + direction + layout.count + 1) % (layout.count + 1);
        } while (gui->focus > 0 && !layout.controls[gui->focus - 1].enabled);
    }
    Vector2 mouse = GetMousePosition();
    gui->hover_board = screen_to_tile(gui, layout.grid, mouse,
                                      &gui->hover_x, &gui->hover_y);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < layout.count; i++) {
            if (layout.controls[i].enabled && point_in(layout.controls[i].rect, mouse) &&
                point_in(layout.side, mouse)) {
                gui->focus = i + 1;
                activate_control(gui, layout.controls[i].id, layout);
                update_title(gui);
                return;
            }
        }
    }
    if ((IsKeyPressed(KEY_ENTER) ||
         (IsKeyPressed(KEY_SPACE) && !point_in(layout.grid, mouse))) &&
        gui->focus > 0 && !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        !IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        int index = gui->focus - 1;
        if (index < layout.count && layout.controls[index].enabled)
            activate_control(gui, layout.controls[index].id, layout);
        update_title(gui);
        return;
    }
    if (process_shortcuts(gui, layout)) {
        update_title(gui);
        return;
    }

    if (gui->focus == 0) {
        if (IsKeyPressed(KEY_SPACE))
            gui->space_board_pending = 1;
        if (shift_down()) {
            double focus_pan = 12.0 / gui->app->view.cell;
            if (IsKeyDown(KEY_LEFT)) gui->app->view.camx -= focus_pan;
            if (IsKeyDown(KEY_RIGHT)) gui->app->view.camx += focus_pan;
            if (IsKeyDown(KEY_UP)) gui->app->view.camy -= focus_pan;
            if (IsKeyDown(KEY_DOWN)) gui->app->view.camy += focus_pan;
        } else {
            if (IsKeyPressed(KEY_LEFT)) gui->cursor_x--;
            if (IsKeyPressed(KEY_RIGHT)) gui->cursor_x++;
            if (IsKeyPressed(KEY_UP)) gui->cursor_y--;
            if (IsKeyPressed(KEY_DOWN)) gui->cursor_y++;
        }
        int activate_board = IsKeyPressed(KEY_ENTER) ||
                             (IsKeyReleased(KEY_SPACE) && gui->space_board_pending);
        if (IsKeyReleased(KEY_SPACE))
            gui->space_board_pending = 0;
        if (activate_board) {
            if (gui->brush == HHMS_BRUSH_SUPPORT) {
                float sx, sy;
                tile_center(gui, layout.grid, gui->cursor_x, gui->cursor_y, &sx, &sy);
                support_edit(gui, layout.grid, (Vector2){sx, sy});
            } else {
                single_tile_edit(gui, gui->cursor_x, gui->cursor_y, gui->brush);
            }
        }
    } else {
        gui->space_board_pending = 0;
        double pan = 12.0 / gui->app->view.cell;
        if (IsKeyDown(KEY_LEFT)) gui->app->view.camx -= pan;
        if (IsKeyDown(KEY_RIGHT)) gui->app->view.camx += pan;
        if (IsKeyDown(KEY_UP)) gui->app->view.camy -= pan;
        if (IsKeyDown(KEY_DOWN)) gui->app->view.camy += pan;
    }
    double pan = 12.0 / gui->app->view.cell;
    if (IsKeyDown(KEY_A)) gui->app->view.camx -= pan;
    if (IsKeyDown(KEY_D)) gui->app->view.camx += pan;
    if (IsKeyDown(KEY_W)) gui->app->view.camy -= pan;
    process_board(gui, layout);
    update_title(gui);
}

static Color number_color(int number)
{
    static const Color colors[9] = {
        {210, 210, 210, 255}, {108, 174, 255, 255}, {112, 215, 130, 255},
        {255, 126, 110, 255}, {190, 155, 255, 255}, {245, 178, 94, 255},
        {102, 220, 220, 255}, {244, 238, 226, 255}, {190, 190, 190, 255}
    };
    return colors[number >= 0 && number <= 8 ? number : 0];
}

static Color odds_color(uint64_t mine, uint64_t total)
{
    if (!total)
        return C_WALL;
    float p = (float)((double)mine / (double)total);
    return (Color){(unsigned char)(86 + p * 116), (unsigned char)(112 - p * 46), 54, 255};
}

static void draw_center_text(const char *text, Rectangle rect, int size, Color color)
{
    int width = MeasureText(text, size);
    DrawText(text, (int)(rect.x + (rect.width - width) * 0.5f),
             (int)(rect.y + (rect.height - size) * 0.5f), size, color);
}

static void draw_dashed_line(Vector2 a, Vector2 b, float dash, Color color)
{
    float dx = b.x - a.x, dy = b.y - a.y;
    float length = sqrtf(dx * dx + dy * dy);
    if (length < 0.1f)
        return;
    float ux = dx / length, uy = dy / length;
    for (float at = 0; at < length; at += dash * 2.0f) {
        float end = fminf(at + dash, length);
        DrawLineEx((Vector2){a.x + ux * at, a.y + uy * at},
                   (Vector2){a.x + ux * end, a.y + uy * end}, 2.0f, color);
    }
}

static void draw_support_footprints(const HhmsGui *gui, Rectangle grid)
{
    for (int i = 0; i < gui->app->map.nsupports; i++) {
        const HhmsSupport *support = &gui->app->map.supports[i];
        float sx, sy;
        map_to_screen(gui, grid, support->x, support->y, &sx, &sy);
        if (!hhms_support_is_directional(support->kind)) {
            float radius = (float)hhms_support_radius(support->kind) * gui->app->view.cell;
            DrawCircleV((Vector2){sx, sy}, radius, C_CONFIRMED);
            DrawCircleLines((int)sx, (int)sy, radius, (Color){95, 159, 219, 180});
        } else {
            double width, length;
            hhms_support_estimated_size(support->kind, &width, &length);
            int vertical = support->orientation == HHMS_ORIENT_NORTH || support->orientation == HHMS_ORIENT_SOUTH;
            float half_x = (float)(vertical ? width : length) * gui->app->view.cell * 0.5f;
            float half_y = (float)(vertical ? length : width) * gui->app->view.cell * 0.5f;
            Vector2 p[4] = {{sx-half_x,sy-half_y},{sx+half_x,sy-half_y},
                            {sx+half_x,sy+half_y},{sx-half_x,sy+half_y}};
            for (int edge = 0; edge < 4; edge++)
                draw_dashed_line(p[edge], p[(edge + 1) % 4], 6.0f, C_ESTIMATED);
        }
    }
}

static void draw_action_shape(HhmsAction action, int authored, HhmsCoverage coverage,
                              Rectangle rect, float cell)
{
    Vector2 center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
    float unit = fmaxf(2.0f, cell * 0.16f);
    if (action == HHMS_ACTION_DIG || action == HHMS_ACTION_SAFE_WATER) {
        DrawLineEx((Vector2){center.x-unit, center.y}, (Vector2){center.x-unit*0.2f, center.y+unit}, 2.4f, C_TEXT);
        DrawLineEx((Vector2){center.x-unit*0.2f, center.y+unit}, (Vector2){center.x+unit*1.2f, center.y-unit}, 2.4f, C_TEXT);
    } else if (action == HHMS_ACTION_CAVE || action == HHMS_ACTION_PROTECTED_CAVE) {
        if (authored) {
            DrawLineEx((Vector2){center.x-unit,center.y+unit}, (Vector2){center.x-unit,center.y-unit}, 2.0f, C_TEXT);
            DrawTriangle((Vector2){center.x-unit,center.y-unit},
                         (Vector2){center.x+unit,center.y-unit*0.45f},
                         (Vector2){center.x-unit,center.y}, C_TEXT);
        } else {
            DrawLineEx((Vector2){center.x-unit,center.y-unit}, (Vector2){center.x+unit,center.y+unit}, 2.4f, C_TEXT);
            DrawLineEx((Vector2){center.x+unit,center.y-unit}, (Vector2){center.x-unit,center.y+unit}, 2.4f, C_TEXT);
        }
        if (action == HHMS_ACTION_PROTECTED_CAVE || coverage.confirmed_count > 0)
            DrawCircleLines((int)center.x, (int)center.y, unit * 1.55f, C_TEXT);
    } else if (action == HHMS_ACTION_OPEN) {
        DrawCircleLines((int)center.x, (int)center.y, unit, C_TEXT);
        DrawCircleV(center, 1.5f, C_TEXT);
    } else if (action == HHMS_ACTION_ODDS_ROCK || action == HHMS_ACTION_ODDS_WATER) {
        DrawCircleLines((int)center.x, (int)center.y, unit, C_TEXT);
        DrawLineEx((Vector2){center.x,center.y-unit}, (Vector2){center.x,center.y+unit}, 1.5f, C_TEXT);
    } else if (action == HHMS_ACTION_CONFLICT) {
        for (float offset = -unit; offset <= unit; offset += fmaxf(3.0f, unit))
            DrawLineEx((Vector2){center.x-unit,center.y+offset},
                       (Vector2){center.x+unit,center.y+offset-unit}, 1.3f, C_TEXT);
        DrawRectangleLinesEx(rect, 2.0f, C_TEXT);
    }
}

static void draw_water_shape(Rectangle rect, float cell)
{
    float x0 = rect.x + cell * 0.18f;
    float x1 = rect.x + rect.width - cell * 0.18f;
    for (int i = -1; i <= 1; i++) {
        float y = rect.y + rect.height * 0.5f + i * fmaxf(2.0f, cell * 0.13f);
        DrawLineEx((Vector2){x0,y}, (Vector2){(x0+x1)*0.5f,y-2.0f}, 1.2f, C_TEXT);
        DrawLineEx((Vector2){(x0+x1)*0.5f,y-2.0f}, (Vector2){x1,y}, 1.2f, C_TEXT);
    }
}

static void draw_tile(const HhmsGui *gui, Rectangle grid, int x, int y)
{
    float center_x, center_y;
    tile_center(gui, grid, x, y, &center_x, &center_y);
    float cell = gui->app->view.cell;
    Rectangle rect = {center_x - cell * 0.5f + 1.0f, center_y - cell * 0.5f + 1.0f,
                      cell - 2.0f, cell - 2.0f};
    const HhmsTile *tile = hhms_get(&gui->app->map, x, y);
    const HhmsAnalysis *analysis = gui->app->analysis_valid ? &gui->app->analysis : NULL;
    const HhmsAnalysisCell *derived = analysis ? hhms_analysis_get(analysis, x, y) : NULL;
    if (!tile && !derived) {
        DrawRectangleRec(rect, C_WALL);
        int linked_range = gui->hover_board && abs(x - gui->hover_x) <= 2 &&
                           abs(y - gui->hover_y) <= 2;
        HhmsLinkRole empty_link = linked_range
            ? hhms_link_role(&gui->app->map, analysis, gui->hover_x, gui->hover_y, x, y)
            : HHMS_LINK_NONE;
        if (empty_link != HHMS_LINK_NONE)
            DrawRectangleLinesEx((Rectangle){rect.x+2,rect.y+2,rect.width-4,rect.height-4},
                                 2.0f, empty_link == HHMS_LINK_CLUE ? C_HOVER : C_LINK);
        if (x == 0 && y == 0)
            DrawRectangleLinesEx(rect, 2.0f, C_ORIGIN);
        return;
    }
    HhmsCellInfo info = hhms_cell_info(&gui->app->map, analysis, x, y);
    int authored_cave = info.kind == HHMS_MINE;
    Color fill = tile && tile->terrain == HHMS_TERRAIN_WATER ? C_WATER : C_WALL;
    if (info.action == HHMS_ACTION_CONFLICT) fill = C_CONFLICT;
    else if (info.action == HHMS_ACTION_CLUE) fill = C_FLOOR;
    else if (info.action == HHMS_ACTION_OPEN) fill = C_OPEN;
    else if (info.action == HHMS_ACTION_DIG || info.action == HHMS_ACTION_SAFE_WATER) fill = C_SAFE;
    else if (info.action == HHMS_ACTION_CAVE || info.action == HHMS_ACTION_PROTECTED_CAVE)
        fill = authored_cave ? C_FLAG : C_MINE;
    else if (info.action == HHMS_ACTION_ODDS_ROCK || info.action == HHMS_ACTION_ODDS_WATER)
        fill = odds_color(info.mine_models, info.total_models);
    DrawRectangleRec(rect, fill);

    if (tile && tile->terrain == HHMS_TERRAIN_WATER)
        draw_water_shape(rect, cell);
    if (info.action == HHMS_ACTION_CLUE && tile) {
        char number[4];
        snprintf(number, sizeof(number), "%d", tile->count);
        int size = cell >= 24.0f ? (int)clampf(cell * 0.55f, 14.0f, 28.0f) : 12;
        Color color = number_color(tile->count);
        int width = MeasureText(number, size);
        int tx = (int)(center_x - width * 0.5f), ty = (int)(center_y - size * 0.5f);
        DrawText(number, tx + 1, ty + 1, size, BLACK);
        DrawText(number, tx, ty, size, color);
    } else {
        draw_action_shape(info.action, authored_cave, info.coverage, rect, cell);
        if ((info.action == HHMS_ACTION_ODDS_ROCK || info.action == HHMS_ACTION_ODDS_WATER) && cell >= 28.0f) {
            char odds[24];
            hhms_format_odds(info.mine_models, info.total_models, 0, odds, sizeof(odds));
            draw_center_text(odds, (Rectangle){rect.x, rect.y + rect.height * 0.52f, rect.width, rect.height * 0.42f},
                             cell >= 44 ? 13 : 10, C_TEXT);
        } else if (cell >= 42.0f && (info.action == HHMS_ACTION_DIG || info.action == HHMS_ACTION_CAVE ||
                                    info.action == HHMS_ACTION_PROTECTED_CAVE || info.action == HHMS_ACTION_OPEN)) {
            const char *label = info.action == HHMS_ACTION_DIG ? "DIG" :
                (info.action == HHMS_ACTION_OPEN ? "open" : (authored_cave ? "flag" : "CAVE"));
            DrawText(label, (int)rect.x + 3, (int)(rect.y + rect.height - 13), 10, C_TEXT);
        }
    }
    int linked_range = gui->hover_board && abs(x - gui->hover_x) <= 2 &&
                       abs(y - gui->hover_y) <= 2;
    HhmsLinkRole link = linked_range
        ? hhms_link_role(&gui->app->map, analysis, gui->hover_x, gui->hover_y, x, y)
        : HHMS_LINK_NONE;
    if (link != HHMS_LINK_NONE)
        DrawRectangleLinesEx((Rectangle){rect.x+2,rect.y+2,rect.width-4,rect.height-4},
                             2.0f, link == HHMS_LINK_CLUE ? C_HOVER : C_LINK);
    if (x == 0 && y == 0)
        DrawRectangleLinesEx(rect, 2.0f, C_ORIGIN);
}

static void draw_support_markers(const HhmsGui *gui, Rectangle grid)
{
    for (int i = 0; i < gui->app->map.nsupports; i++) {
        const HhmsSupport *support = &gui->app->map.supports[i];
        float x, y;
        map_to_screen(gui, grid, support->x, support->y, &x, &y);
        float size = clampf(gui->app->view.cell * 0.18f, 4.0f, 10.0f);
        DrawPoly((Vector2){x,y}, 4, size, 45.0f, C_HOVER);
        DrawPolyLines((Vector2){x,y}, 4, size, 45.0f, C_TEXT);
        DrawText(SUPPORT_CODES[support->kind], (int)(x + size + 2.0f),
                 (int)(y - size - 1.0f), 10, C_TEXT);
        if (support->id == gui->selected_support_id)
            DrawCircleLines((int)x, (int)y, size + 4.0f, C_HOVER);
        if (hhms_support_is_directional(support->kind)) {
            Vector2 direction = {0,-size*1.8f};
            if (support->orientation == HHMS_ORIENT_EAST) direction = (Vector2){size*1.8f,0};
            else if (support->orientation == HHMS_ORIENT_SOUTH) direction = (Vector2){0,size*1.8f};
            else if (support->orientation == HHMS_ORIENT_WEST) direction = (Vector2){-size*1.8f,0};
            DrawLineEx((Vector2){x,y}, (Vector2){x+direction.x,y+direction.y}, 2.0f, C_TEXT);
        }
    }
}

static void draw_axes_and_grid(const HhmsGui *gui, Rectangle grid,
                               int x0, int y0, int x1, int y1)
{
    float cell = gui->app->view.cell;
    Color grid_line = cell < 20 ? (Color){54,58,64,255} : C_LINE;
    for (int x = x0; x <= x1; x++) {
        float sx, sy;
        tile_center(gui, grid, x, 0, &sx, &sy);
        float edge = sx - cell * 0.5f;
        DrawLine((int)edge, (int)grid.y, (int)edge,
                 (int)(grid.y + grid.height), grid_line);
    }
    for (int y = y0; y <= y1; y++) {
        float sx, sy;
        tile_center(gui, grid, 0, y, &sx, &sy);
        float edge = sy - cell * 0.5f;
        DrawLine((int)grid.x, (int)edge,
                 (int)(grid.x + grid.width), (int)edge, grid_line);
    }
    float ox, oy;
    tile_center(gui, grid, 0, 0, &ox, &oy);
    float origin_x = ox - cell * 0.5f;
    float origin_y = oy - cell * 0.5f;
    DrawLineEx((Vector2){grid.x, origin_y},
               (Vector2){grid.x + grid.width, origin_y}, 2.0f, C_ORIGIN);
    DrawLineEx((Vector2){origin_x, grid.y},
               (Vector2){origin_x, grid.y + grid.height}, 2.0f, C_ORIGIN);

    int step = cell >= 34 ? 1 : (cell >= 20 ? 5 : 10);
    int font = cell >= 24 ? 11 : 9;
    for (int x = x0; x <= x1; x++) {
        if (x % step != 0)
            continue;
        float sx, ignored;
        tile_center(gui, grid, x, 0, &sx, &ignored);
        char text[24];
        snprintf(text, sizeof(text), "%d", x);
        DrawText(text, (int)(sx - MeasureText(text, font) * 0.5f),
                 (int)grid.y + 3, font, C_ORIGIN);
    }
    for (int y = y0; y <= y1; y++) {
        if (y % step != 0)
            continue;
        float ignored, sy;
        tile_center(gui, grid, 0, y, &ignored, &sy);
        char text[24];
        snprintf(text, sizeof(text), "%d", y);
        DrawText(text, (int)grid.x + 3, (int)(sy - font * 0.5f),
                 font, C_ORIGIN);
    }
}

static void draw_board(const HhmsGui *gui, Rectangle grid)
{
    DrawRectangleRec(grid, C_BG);
    double left = gui->app->view.camx - grid.width * 0.5 / gui->app->view.cell;
    double right = gui->app->view.camx + grid.width * 0.5 / gui->app->view.cell;
    double top = gui->app->view.camy - grid.height * 0.5 / gui->app->view.cell;
    double bottom = gui->app->view.camy + grid.height * 0.5 / gui->app->view.cell;
    int x0 = (int)floor(left - 0.5) - 1, x1 = (int)ceil(right + 0.5) + 1;
    int y0 = (int)floor(top - 0.5) - 1, y1 = (int)ceil(bottom + 0.5) + 1;
    BeginScissorMode((int)grid.x, (int)grid.y, (int)grid.width, (int)grid.height);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            draw_tile(gui, grid, x, y);
    draw_support_footprints(gui, grid);
    draw_axes_and_grid(gui, grid, x0, y0, x1, y1);
    draw_support_markers(gui, grid);
    int cx = gui->cursor_x, cy = gui->cursor_y;
    float sx, sy;
    tile_center(gui, grid, cx, cy, &sx, &sy);
    Rectangle cursor = {sx-gui->app->view.cell*0.5f+2, sy-gui->app->view.cell*0.5f+2,
                        gui->app->view.cell-4, gui->app->view.cell-4};
    DrawRectangleLinesEx(cursor, gui->focus == 0 ? 3.0f : 1.5f,
                         gui->focus == 0 ? C_HOVER : C_DIM);
    if (gui->hover_board && (gui->hover_x != cx || gui->hover_y != cy)) {
        tile_center(gui, grid, gui->hover_x, gui->hover_y, &sx, &sy);
        DrawRectangleLinesEx((Rectangle){sx-gui->app->view.cell*0.5f+1,sy-gui->app->view.cell*0.5f+1,
                                        gui->app->view.cell-2,gui->app->view.cell-2}, 2.0f, C_HOVER);
    }
    EndScissorMode();
}

static void draw_button(const Control *control, int focused, Vector2 mouse, float scale)
{
    int hot = control->enabled && point_in(control->rect, mouse);
    int active = hot && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Color fill = !control->enabled ? C_DISABLED :
        (active || control->selected ? C_SELECTED : (hot ? C_BTN_HOVER : C_BTN));
    Color border = focused ? C_HOVER : (control->selected ? C_HOVER : C_LINE);
    DrawRectangleRec(control->rect, fill);
    DrawRectangleLinesEx(control->rect, focused ? 2.0f : 1.0f, border);
    int font = (int)clampf(13.0f * scale, 11.0f, 17.0f);
    draw_center_text(control->label, control->rect, font,
                     control->enabled ? C_TEXT : C_DISABLED_TEXT);
}

static void draw_sidebar(const HhmsGui *gui, const GuiLayout *layout)
{
    DrawRectangleRec(layout->side, C_SIDE);
    DrawLine((int)layout->side.x, 0, (int)layout->side.x, GetScreenHeight(), C_LINE);
    float scale = layout->scale;
    float x = layout->side.x + 12.0f * scale;
    DrawText("HH Minesweeper", (int)x, (int)(10*scale), (int)(19*scale), C_HOVER);
    DrawText("v" HHMS_GUI_VERSION "  manual planning companion", (int)x, (int)(33*scale), (int)(11*scale), C_DIM);
    char filename[512];
    const char *map_name = hhms_app_filename(gui->app);
    if (default_font_filename_safe(map_name)) {
        snprintf(filename, sizeof(filename), "%s%s", map_name,
                 hhms_app_dirty(gui->app) ? "  * modified" : "");
    } else {
        snprintf(filename, sizeof(filename), "Unicode filename%s - see window title",
                 hhms_app_dirty(gui->app) ? "  * modified" : "");
    }
    DrawText(filename, (int)x, (int)(55*scale), (int)(13*scale), C_TEXT);

    BeginScissorMode((int)layout->side.x, (int)(78*scale), (int)layout->side.width,
                     GetScreenHeight() - (int)(78*scale));
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < layout->count; i++)
        draw_button(&layout->controls[i], gui->focus == i + 1, mouse, scale);

    float pad = 12.0f * scale;
    float label_x = layout->side.x + pad;
    float controls_start = 86.0f * scale - gui->sidebar_scroll;
    DrawText("File, history, and view", (int)label_x, (int)(controls_start - 17*scale), (int)(11*scale), C_DIM);
    float brush_label_y = controls_start + 5.0f * (27.0f*scale + 6.0f*scale) + 4.0f*scale;
    DrawText("Dust observed after mining (0-8)", (int)label_x, (int)brush_label_y, (int)(11*scale), C_DIM);
    float action_label_y = brush_label_y + 3.0f*(27.0f*scale+6.0f*scale) + 31.0f*scale;
    DrawText("Authored tiles", (int)label_x, (int)action_label_y, (int)(11*scale), C_DIM);
    float support_y = action_label_y + 2.0f*(27.0f*scale+6.0f*scale) + 32.0f*scale;
    char selected[192];
    if (hhms_support_is_directional(gui->support_kind)) {
        double width, length;
        hhms_support_estimated_size(gui->support_kind, &width, &length);
        snprintf(selected, sizeof(selected), "Support: %s, %s, est %.0fx%.0f",
                 SUPPORT_LABELS[gui->support_kind], ORIENTATION_LABELS[gui->orientation], width, length);
    } else {
        snprintf(selected, sizeof(selected), "Support: %s, modeled r%.2g",
                 SUPPORT_LABELS[gui->support_kind], hhms_support_radius(gui->support_kind));
    }
    DrawText(selected, (int)label_x, (int)support_y, (int)(10*scale), C_DIM);
    float legend_y = layout->content_height - gui->sidebar_scroll - 112.0f*scale;
    DrawText("Board key", (int)label_x, (int)legend_y, (int)(12*scale), C_DIM); legend_y += 17*scale;
    DrawText("check DIG | flag authored | X proved CAVE", (int)label_x, (int)legend_y, (int)(11*scale), C_TEXT); legend_y += 16*scale;
    DrawText("ring open | waves water | circle odds | hatch conflict", (int)label_x, (int)legend_y, (int)(11*scale), C_TEXT); legend_y += 16*scale;
    DrawText("Blue wash = modeled radial coverage", (int)label_x, (int)legend_y, (int)(11*scale), C_TEXT); legend_y += 16*scale;
    DrawText("Dashed gold = visual tunnel estimate only", (int)label_x, (int)legend_y, (int)(11*scale), C_TEXT);
    EndScissorMode();

    if (layout->content_height > layout->side.height) {
        float track_y = 82.0f*scale, track_h = layout->side.height - track_y - 8.0f;
        float thumb_h = fmaxf(34.0f, track_h * layout->side.height / layout->content_height);
        float maximum = fmaxf(1.0f, layout->content_height - layout->side.height + 18.0f*scale);
        float thumb_y = track_y + (track_h-thumb_h) * gui->sidebar_scroll / maximum;
        DrawRectangle((int)(layout->side.x+layout->side.width-4), (int)thumb_y, 3, (int)thumb_h, C_DIM);
    }
}

static void draw_text_fit(const char *text, int x, int y, int font,
                          int max_width, Color color)
{
    if (MeasureText(text, font) <= max_width) {
        DrawText(text, x, y, font, color);
        return;
    }
    char clipped[512];
    snprintf(clipped, sizeof(clipped), "%s", text);
    size_t length = strlen(clipped);
    while (length > 3 && MeasureText(clipped, font) > max_width) {
        length--;
        clipped[length] = '\0';
    }
    if (length > 3) {
        clipped[length - 3] = '.';
        clipped[length - 2] = '.';
        clipped[length - 1] = '.';
    }
    DrawText(clipped, x, y, font, color);
}

static void draw_status(const HhmsGui *gui, Rectangle status)
{
    DrawRectangleRec(status, C_STATUS);
    BeginScissorMode((int)status.x, (int)status.y,
                     (int)status.width, (int)status.height);
    int x = (int)status.x + 10;
    int y = (int)status.y + 6;
    int cell_x = gui->hover_board ? gui->hover_x : gui->cursor_x;
    int cell_y = gui->hover_board ? gui->hover_y : gui->cursor_y;
    char cell[384];
    hhms_app_format_cell(gui->app, cell_x, cell_y, cell, sizeof(cell));
    draw_text_fit(cell, x, y, 13, (int)status.width - 20, C_TEXT);
    const char *message = gui->app->message;
    if (!gui->app->analysis_valid)
        message = "Painting: analysis is paused; one Undo will cover this entire stroke.";
    else if (!gui->app->analysis.contradiction &&
             (gui->app->analysis.limits & HHMS_LIMIT_ANALYSIS_CELLS))
        message = "Analysis limited: the 8192 derived-cell cap was reached; results may be incomplete.";
    else if (!gui->app->analysis.contradiction &&
             (gui->app->analysis.limits & HHMS_LIMIT_ENUM_COMPONENT))
        message = "Analysis limited: a connected frontier exceeds 20 cells; odds are incomplete.";
    draw_text_fit(message, x, y + 20, 12, (int)status.width - 20,
                  gui->app->analysis_valid && gui->app->analysis.contradiction
                      ? C_CONFLICT : C_DIM);
    int support_index = gui->selected_support_id
        ? hhms_support_index_by_id(&gui->app->map, gui->selected_support_id)
        : hhms_support_index_near(&gui->app->map, cell_x, cell_y, 0.55);
    if (support_index >= 0) {
        const HhmsSupport *support = &gui->app->map.supports[support_index];
        int exposed = hhms_support_exposed_caves(&gui->app->map,
                                                  gui->app->analysis_valid ? &gui->app->analysis : NULL,
                                                  support->id);
        char info[224];
        if (hhms_support_is_directional(support->kind)) {
            double width_estimate, length_estimate;
            hhms_support_estimated_size(support->kind, &width_estimate, &length_estimate);
            snprintf(info, sizeof(info), "%s @ %.2f,%.2f %s est %.0fx%.0f expose %d",
                     SUPPORT_LABELS[support->kind], support->x, support->y,
                     ORIENTATION_LABELS[support->orientation], width_estimate, length_estimate, exposed);
        } else {
            snprintf(info, sizeof(info), "%s @ %.2f,%.2f %s r%.2f overlap %d expose %d",
                     SUPPORT_LABELS[support->kind], support->x, support->y,
                     ORIENTATION_LABELS[support->orientation], hhms_support_radius(support->kind),
                     hhms_coverage(&gui->app->map, cell_x, cell_y, 0).confirmed_count, exposed);
        }
        draw_text_fit(info, x, y + 37, 11, (int)status.width - 20, C_HOVER);
    }
    EndScissorMode();
}

static void draw_help(const HhmsGui *gui, Rectangle grid)
{
    (void)gui;
    (void)grid;
    Rectangle box = {18, 18, (float)GetScreenWidth() - 36, (float)GetScreenHeight() - 36};
    if (box.width > 800) { box.x += (box.width-800)*0.5f; box.width = 800; }
    DrawRectangleRec(box, (Color){16,18,22,250});
    DrawRectangleLinesEx(box, 2.0f, C_HOVER);
    int x = (int)box.x + 18, y = (int)box.y + 15;
    DrawText("HH Minesweeper " HHMS_GUI_VERSION " - first steps", x, y, 19, C_HOVER); y += 28;
    const char *lines[] = {
        "Set the map origin first: (0,0) is the gold axis crossing; Origin returns there.",
        "Mine a wall, then type its dust count 0-8. Open floor has no dust clue.",
        "Water can contain a cave-in state but is not mineable; proved-safe water remains water.",
        "DIG (check) is proved safe. A flag is authored; CAVE (X) is proved from counts.",
        "A percent is cave-in share across legal layouts, never a safety guarantee.",
        "Hover a clue or odds tile for its exact remaining count or legal-layout fraction.",
        "Enumeration is capped at a 20-cell connected frontier; limit warnings stay visible.",
        "Drag to paint; each drag is one Undo. Right-click flags; X corrects by erasing.",
        "Pan with middle-drag or Space-drag; wheel zoom stays anchored under the pointer.",
        "Fit Map is Shift+R; Origin is R. Tab reaches the board and every enabled control.",
        "Save opens a native Save As for Untitled maps; choose any local .hhmap location.",
        "Modeled radial assumptions: wood 9.09, stone 11.36, beam 13.64, monument 30.",
        "Tunnel outlines estimate 1x4, 2x8, or 3x15 only; they never confirm safety.",
        "Blue radial wash is modeled coverage; dashed gold footprints are estimates.",
        "Conflicts fail closed: DIG, CAVE, and odds disappear until observations agree."
    };
    int line_height = box.height < 430 ? 17 : 21;
    int font = box.width < 570 ? 11 : 13;
    for (int i = 0; i < (int)(sizeof(lines)/sizeof(lines[0])); i++) {
        DrawText(lines[i], x, y, font, C_TEXT);
        y += line_height;
    }
    Rectangle close = {box.x + box.width - 190, box.y + box.height - 38, 172, 28};
    Control close_control = {0, close, "Close help (Esc)", 0, 1};
    draw_button(&close_control, 1, GetMousePosition(), 1.0f);
}

static void draw_confirmation(const HhmsGui *gui)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0,0,0,150});
    float width = fminf(520.0f, GetScreenWidth() - 40.0f);
    Rectangle box = {(GetScreenWidth()-width)*0.5f, GetScreenHeight()*0.5f-92.0f, width, 184.0f};
    DrawRectangleRec(box, (Color){22,24,28,255});
    DrawRectangleLinesEx(box, 2.0f, C_HOVER);
    DrawText("Unsaved changes", (int)box.x+18, (int)box.y+18, 20, C_HOVER);
    char action[256];
    const char *verb = gui->app->pending == HHMS_PENDING_NEW ? "create a new map" :
        (gui->app->pending == HHMS_PENDING_LOAD ? "load another map" : "close the window");
    snprintf(action, sizeof(action), "Save this map before you %s?", verb);
    DrawText(action, (int)box.x+18, (int)box.y+54, 13, C_TEXT);
    DrawText("Discard cannot be undone. Cancel keeps this map open.", (int)box.x+18, (int)box.y+78, 12, C_DIM);
    const char *labels[3] = {"Save", "Discard", "Cancel"};
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < 3; i++) {
        Control control = {0, modal_button(i), labels[i], i == 2, 1};
        draw_button(&control, gui->modal_focus == i, mouse, 1.0f);
    }
}

void hhms_gui_draw(const HhmsGui *gui)
{
    GuiLayout layout = make_layout(gui);
    ClearBackground(C_BG);
    draw_board(gui, layout.grid);
    draw_sidebar(gui, &layout);
    draw_status(gui, layout.status);
    if (gui->help)
        draw_help(gui, layout.grid);
    if (gui->app->pending != HHMS_PENDING_NONE)
        draw_confirmation(gui);
}
