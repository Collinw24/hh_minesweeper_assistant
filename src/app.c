#include "app.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *duplicate_string(const char *value)
{
    if (!value)
        return NULL;
    size_t length = strlen(value) + 1;
    char *copy = (char *)malloc(length);
    if (copy)
        memcpy(copy, value, length);
    return copy;
}

static int view_equal(const HhmsView *a, const HhmsView *b)
{
    return a->camx == b->camx && a->camy == b->camy &&
           a->cell == b->cell && a->ui_scale == b->ui_scale &&
           a->has_view == b->has_view;
}
static void set_analysis_message(HhmsApp *app);

static int finish_active_edit(HhmsApp *app)
{
    if (!app->map.history.active)
        return 0;
    int changed = hhms_commit_edit(&app->map);
    hhms_app_solve(app);
    if (changed)
        set_analysis_message(app);
    return changed;
}


void hhms_app_set_message(HhmsApp *app, const char *message)
{
    if (!message)
        message = "";
    strncpy(app->message, message, sizeof(app->message) - 1);
    app->message[sizeof(app->message) - 1] = '\0';
}

static const char *base_name(const char *path)
{
    if (!path || !path[0])
        return "Untitled.hhmap";
    const char *name = path;
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    if (slash && slash + 1 > name)
        name = slash + 1;
    if (backslash && backslash + 1 > name)
        name = backslash + 1;
    return *name ? name : path;
}

const char *hhms_app_filename(const HhmsApp *app)
{
    return base_name(app->path);
}

void hhms_app_solve(HhmsApp *app)
{
    hhms_solve(&app->map, &app->analysis);
    app->analysis_valid = 1;
}

void hhms_app_init(HhmsApp *app)
{
    memset(app, 0, sizeof(*app));
    hhms_init(&app->map);
    hhms_analysis_init(&app->analysis);
    hhms_view_init(&app->view);
    app->view.has_view = 1;
    app->saved_view = app->view;
    app->saved_state = hhms_state_id(&app->map);
    hhms_app_solve(app);
    hhms_app_set_message(app, "Hover a clue to see the walls it still constrains.");
}

void hhms_app_destroy(HhmsApp *app)
{
    free(app->path);
    free(app->pending_path);
    app->path = NULL;
    app->pending_path = NULL;
}

int hhms_app_dirty(const HhmsApp *app)
{
    return (app->map.history.active &&
            app->map.history.nactive_changes > 0) ||
           hhms_state_id(&app->map) != app->saved_state ||
           !view_equal(&app->view, &app->saved_view);
}

static void set_io_message(HhmsApp *app, const char *operation, const char *path,
                           const HhmsError *error)
{
    const char *detail = error && error->detail[0]
        ? error->detail : hhms_io_result_name(error ? error->code : HHMS_IO_PARSE_FAILED);
    snprintf(app->message, sizeof(app->message), "%s %s failed: %s",
             operation, base_name(path), detail);
}

static HhmsAppResult save_to(HhmsApp *app, const char *path, int replace_path)
{
    finish_active_edit(app);
    if (!path || !path[0])
        return HHMS_APP_NEEDS_SAVE_PATH;
    char *copy = replace_path ? duplicate_string(path) : NULL;
    if (replace_path && !copy) {
        hhms_app_set_message(app, "Save failed: out of memory.");
        return HHMS_APP_ERROR;
    }
    HhmsError error;
    HhmsIoResult result = hhms_save(&app->map, &app->view, path, &error);
    if (result != HHMS_IO_OK) {
        free(copy);
        set_io_message(app, "Saving", path, &error);
        return HHMS_APP_ERROR;
    }
    if (replace_path) {
        free(app->path);
        app->path = copy;
    }
    app->saved_state = hhms_state_id(&app->map);
    app->saved_view = app->view;
    snprintf(app->message, sizeof(app->message), "Saved %s.", base_name(path));
    return HHMS_APP_OK;
}

HhmsAppResult hhms_app_save(HhmsApp *app)
{
    if (!app->path)
        return HHMS_APP_NEEDS_SAVE_PATH;
    return save_to(app, app->path, 0);
}

HhmsAppResult hhms_app_save_as(HhmsApp *app, const char *path)
{
    return save_to(app, path, 1);
}

static HhmsAppResult load_now(HhmsApp *app, const char *path)
{
    char *copy = duplicate_string(path);
    if (!copy) {
        hhms_app_set_message(app, "Load failed: out of memory.");
        return HHMS_APP_ERROR;
    }
    HhmsError error;
    HhmsView loaded_view;
    hhms_view_init(&loaded_view);
    HhmsIoResult result = hhms_load(&app->map, &loaded_view, path, &error);
    if (result != HHMS_IO_OK) {
        free(copy);
        set_io_message(app, "Loading", path, &error);
        return HHMS_APP_ERROR;
    }
    free(app->path);
    app->path = copy;
    app->view = loaded_view;
    app->fit_after_load = !loaded_view.has_view;
    if (app->fit_after_load) {
        hhms_view_init(&app->view);
        app->view.has_view = 1;
    }
    app->saved_state = hhms_state_id(&app->map);
    app->saved_view = app->view;
    hhms_app_solve(app);
    snprintf(app->message, sizeof(app->message), "Loaded %s.", base_name(path));
    return HHMS_APP_OK;
}

static HhmsAppResult new_now(HhmsApp *app)
{
    hhms_init(&app->map);
    hhms_view_init(&app->view);
    app->view.has_view = 1;
    app->saved_view = app->view;
    app->saved_state = hhms_state_id(&app->map);
    free(app->path);
    app->path = NULL;
    app->fit_after_load = 0;
    hhms_app_solve(app);
    hhms_app_set_message(app, "New map. Align the origin, then enter a mined tile's dust.");
    return HHMS_APP_OK;
}

static HhmsAppResult execute_pending(HhmsApp *app)
{
    HhmsPendingAction action = app->pending;
    char *path = app->pending_path;
    app->pending = HHMS_PENDING_NONE;
    app->pending_path = NULL;
    HhmsAppResult result = HHMS_APP_OK;
    if (action == HHMS_PENDING_NEW) {
        result = new_now(app);
    } else if (action == HHMS_PENDING_LOAD) {
        result = load_now(app, path);
    } else if (action == HHMS_PENDING_CLOSE) {
        app->quit = 1;
        result = HHMS_APP_QUIT;
    }
    free(path);
    return result;
}

static HhmsAppResult request_action(HhmsApp *app, HhmsPendingAction action,
                                    const char *path)
{
    finish_active_edit(app);
    if (app->pending != HHMS_PENDING_NONE)
        return HHMS_APP_NEEDS_CONFIRMATION;
    if (path) {
        app->pending_path = duplicate_string(path);
        if (!app->pending_path) {
            hhms_app_set_message(app, "Action failed: out of memory.");
            return HHMS_APP_ERROR;
        }
    }
    app->pending = action;
    if (hhms_app_dirty(app)) {
        hhms_app_set_message(app, "Unsaved changes: Save, Discard, or Cancel.");
        return HHMS_APP_NEEDS_CONFIRMATION;
    }
    return execute_pending(app);
}

HhmsAppResult hhms_app_request_new(HhmsApp *app)
{
    return request_action(app, HHMS_PENDING_NEW, NULL);
}

HhmsAppResult hhms_app_request_load(HhmsApp *app, const char *path)
{
    if (!path || !path[0])
        return HHMS_APP_NO_CHANGE;
    return request_action(app, HHMS_PENDING_LOAD, path);
}

HhmsAppResult hhms_app_request_close(HhmsApp *app)
{
    return request_action(app, HHMS_PENDING_CLOSE, NULL);
}

HhmsAppResult hhms_app_resolve_unsaved(HhmsApp *app, HhmsUnsavedChoice choice,
                                       const char *save_path)
{
    if (app->pending == HHMS_PENDING_NONE)
        return HHMS_APP_NO_CHANGE;
    if (choice == HHMS_UNSAVED_CANCEL) {
        free(app->pending_path);
        app->pending_path = NULL;
        app->pending = HHMS_PENDING_NONE;
        hhms_app_set_message(app, "Action cancelled.");
        return HHMS_APP_NO_CHANGE;
    }
    if (choice == HHMS_UNSAVED_SAVE) {
        HhmsAppResult result = save_path && save_path[0]
            ? hhms_app_save_as(app, save_path) : hhms_app_save(app);
        if (result != HHMS_APP_OK)
            return result;
    }
    return execute_pending(app);
}

static void set_analysis_message(HhmsApp *app)
{
    if (app->analysis.contradiction) {
        hhms_app_set_message(app, "Counts disagree. Derived DIG, CAVE, and odds are hidden.");
    } else if (!app->analysis.complete) {
        hhms_app_set_message(app, "Analysis is limited: a connected frontier exceeds 20 cells.");
    } else {
        hhms_app_set_message(app, "Analysis current.");
    }
}

int hhms_app_begin_edit(HhmsApp *app)
{
    if (!hhms_begin_edit(&app->map))
        return 0;
    app->analysis_valid = 0;
    return 1;
}

int hhms_app_commit_edit(HhmsApp *app)
{
    int changed = hhms_commit_edit(&app->map);
    hhms_app_solve(app);
    if (changed)
        set_analysis_message(app);
    return changed;
}

void hhms_app_cancel_edit(HhmsApp *app)
{
    hhms_cancel_edit(&app->map);
    hhms_app_solve(app);
    hhms_app_set_message(app, "Edit cancelled.");
}

int hhms_app_undo(HhmsApp *app)
{
    finish_active_edit(app);
    if (!hhms_undo(&app->map))
        return 0;
    hhms_app_solve(app);
    hhms_app_set_message(app, "Undo.");
    return 1;
}

int hhms_app_redo(HhmsApp *app)
{
    finish_active_edit(app);
    if (!hhms_redo(&app->map))
        return 0;
    hhms_app_solve(app);
    hhms_app_set_message(app, "Redo.");
    return 1;
}

void hhms_app_reset_view(HhmsApp *app)
{
    float ui_scale = app->view.ui_scale > 0.0f ? app->view.ui_scale : 1.0f;
    hhms_view_init(&app->view);
    app->view.ui_scale = ui_scale;
    app->view.has_view = 1;
}

void hhms_app_fit_view(HhmsApp *app, float grid_width, float grid_height,
                       int mark_dirty)
{
    float ui_scale = app->view.ui_scale > 0.0f ? app->view.ui_scale : 1.0f;
    double min_x, min_y, max_x, max_y;
    if (!hhms_bounds(&app->map, &min_x, &min_y, &max_x, &max_y)) {
        hhms_app_reset_view(app);
    } else {
        double span_x = max_x - min_x + 3.0;
        double span_y = max_y - min_y + 3.0;
        double by_width = grid_width > 0.0f ? grid_width / span_x : 32.0;
        double by_height = grid_height > 0.0f ? grid_height / span_y : 32.0;
        double cell = fmin(by_width, by_height);
        if (cell < 14.0) cell = 14.0;
        if (cell > 72.0) cell = 72.0;
        app->view.camx = (min_x + max_x) * 0.5;
        app->view.camy = (min_y + max_y) * 0.5;
        app->view.cell = (float)cell;
        app->view.ui_scale = ui_scale;
        app->view.has_view = 1;
    }
    if (!mark_dirty)
        app->saved_view = app->view;
}

void hhms_app_accept_loaded_fit(HhmsApp *app)
{
    app->saved_view = app->view;
    app->fit_after_load = 0;
}

const char *hhms_reason_name(HhmsReason reason)
{
    switch (reason) {
    case HHMS_REASON_SIMPLE: return "one clue";
    case HHMS_REASON_SUBSET: return "overlapping clues";
    case HHMS_REASON_ENUMERATION: return "all legal layouts";
    default: return "no deduction";
    }
}

void hhms_format_odds(uint64_t mine_models, uint64_t total_models,
                      int detailed, char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0)
        return;
    if (total_models == 0) {
        snprintf(buffer, capacity, "unresolved");
        return;
    }
    if (mine_models == 0) {
        snprintf(buffer, capacity, detailed ? "0%% (proved safe)" : "0%%");
        return;
    }
    if (mine_models == total_models) {
        snprintf(buffer, capacity, detailed ? "100%% (proved cave)" : "100%%");
        return;
    }
    double probability = (double)mine_models / (double)total_models;
    char percent[24];
    if (probability < 0.01)
        snprintf(percent, sizeof(percent), "<1%%");
    else if (probability > 0.99)
        snprintf(percent, sizeof(percent), ">99%%");
    else
        snprintf(percent, sizeof(percent), "%.0f%%", probability * 100.0);
    if (detailed) {
        snprintf(buffer, capacity, "%s (%llu of %llu legal layouts)", percent,
                 (unsigned long long)mine_models,
                 (unsigned long long)total_models);
    } else {
        snprintf(buffer, capacity, "%s", percent);
    }
}

static const char *support_name_by_id(const HhmsApp *app, uint32_t id)
{
    int index = hhms_support_index_by_id(&app->map, id);
    return index >= 0 ? hhms_support_name(app->map.supports[index].kind) : "support";
}

void hhms_app_format_cell(const HhmsApp *app, int x, int y,
                          char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0)
        return;
    HhmsCellInfo info = hhms_cell_info(&app->map,
                                       app->analysis_valid ? &app->analysis : NULL,
                                       x, y);
    const HhmsTile *tile = hhms_get(&app->map, x, y);
    switch (info.action) {
    case HHMS_ACTION_CONFLICT:
        snprintf(buffer, capacity, "(%d,%d) CONFLICT: this observation disagrees with the counts", x, y);
        break;
    case HHMS_ACTION_CLUE: {
        HhmsConstraintSummary summary = hhms_constraint_summary(
            &app->map, app->analysis_valid ? &app->analysis : NULL, x, y);
        snprintf(buffer, capacity,
                 "(%d,%d) dust %d: needs %d cave%s among %d unresolved wall%s",
                 x, y, tile ? tile->count : 0, summary.remaining_mines,
                 summary.remaining_mines == 1 ? "" : "s", summary.remaining_cells,
                 summary.remaining_cells == 1 ? "" : "s");
        break;
    }
    case HHMS_ACTION_OPEN:
        snprintf(buffer, capacity, "(%d,%d) open floor; no dust clue", x, y);
        break;
    case HHMS_ACTION_DIG:
        snprintf(buffer, capacity, "(%d,%d) DIG: proved safe from %s", x, y,
                 hhms_reason_name(info.reason));
        break;
    case HHMS_ACTION_SAFE_WATER:
        snprintf(buffer, capacity, "(%d,%d) water: proved free of cave-ins; not mineable", x, y);
        break;
    case HHMS_ACTION_CAVE:
        snprintf(buffer, capacity, "(%d,%d) CAVE: %s", x, y,
                 info.kind == HHMS_MINE ? "flagged by you" : "proved from counts");
        break;
    case HHMS_ACTION_PROTECTED_CAVE:
        snprintf(buffer, capacity,
                 "(%d,%d) PROTECTED CAVE: %s; %s covers it in the model, mining damages support",
                 x, y, info.kind == HHMS_MINE ? "flagged by you" : "proved from counts",
                 support_name_by_id(app, info.coverage.primary_confirmed));
        break;
    case HHMS_ACTION_ODDS_ROCK:
    case HHMS_ACTION_ODDS_WATER: {
        char odds[128];
        hhms_format_odds(info.mine_models, info.total_models, 1, odds, sizeof(odds));
        snprintf(buffer, capacity, "(%d,%d) %s cave-in share; not proved%s", x, y,
                 odds, info.action == HHMS_ACTION_ODDS_WATER ? "; water" : "");
        break;
    }
    case HHMS_ACTION_UNKNOWN_WATER:
        snprintf(buffer, capacity, "(%d,%d) water; cave-in state unknown", x, y);
        break;
    default:
        snprintf(buffer, capacity, "(%d,%d) unknown rock", x, y);
        break;
    }
    if (app->analysis_valid && !app->analysis.complete) {
        size_t used = strlen(buffer);
        if (used < capacity) {
            snprintf(buffer + used, capacity - used,
                     " | solver limit: exact odds unavailable for a large frontier");
        }
    }
}
