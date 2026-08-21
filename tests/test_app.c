#define _XOPEN_SOURCE 700

#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define rmdir _rmdir
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static int g_fail;
static int g_pass;

static void expect(int condition, const char *behavior)
{
    if (condition) {
        g_pass++;
    } else {
        g_fail++;
        fprintf(stderr, "FAIL %s\n", behavior);
    }
}

typedef struct {
    char path[512];
} TempDirectory;

static int temp_directory_create(TempDirectory *directory)
{
#ifdef _WIN32
    char base[MAX_PATH];
    char unique[MAX_PATH];
    DWORD length = GetTempPathA((DWORD)sizeof(base), base);
    if (length == 0 || length >= sizeof(base) ||
        !GetTempFileNameA(base, "hha", 0, unique))
        return 0;
    DeleteFileA(unique);
    if (_mkdir(unique) != 0)
        return 0;
    snprintf(directory->path, sizeof(directory->path), "%s", unique);
#else
    if (snprintf(directory->path, sizeof(directory->path),
                 "/tmp/hhms-app-tests-XXXXXX") >= (int)sizeof(directory->path))
        return 0;
    int descriptor = mkstemp(directory->path);
    if (descriptor < 0)
        return 0;
    close(descriptor);
    if (remove(directory->path) != 0 || mkdir(directory->path, 0700) != 0)
        return 0;
#endif
    return 1;
}

static void temp_path(const TempDirectory *directory, const char *name,
                      char *path, size_t capacity)
{
    snprintf(path, capacity, "%s/%s", directory->path, name);
}

static int write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    if (!file)
        return 0;
    size_t length = strlen(text);
    int ok = fwrite(text, 1, length, file) == length && fclose(file) == 0;
    return ok;
}

static int app_edit_tile(HhmsApp *app, int x, int y, HhmsKind kind, int count)
{
    if (!hhms_app_begin_edit(app))
        return 0;
    if (hhms_set_tile(&app->map, x, y, kind, count) != HHMS_EDIT_OK) {
        hhms_app_cancel_edit(app);
        return 0;
    }
    return hhms_app_commit_edit(app);
}

static int app_edit_support(HhmsApp *app, double x, double y,
                            HhmsSupportKind kind, uint32_t *id)
{
    if (!hhms_app_begin_edit(app))
        return 0;
    if (hhms_add_support(&app->map, x, y, kind, HHMS_ORIENT_NORTH, id) !=
        HHMS_EDIT_OK) {
        hhms_app_cancel_edit(app);
        return 0;
    }
    return hhms_app_commit_edit(app);
}

static int view_equal(HhmsView a, HhmsView b)
{
    return a.camx == b.camx && a.camy == b.camy && a.cell == b.cell &&
           a.ui_scale == b.ui_scale && a.has_view == b.has_view;
}

static void test_new_untitled_document(void)
{
    static HhmsApp app;
    hhms_app_init(&app);
    expect(app.path == NULL, "new application document has no established save path");
    expect(strcmp(hhms_app_filename(&app), "Untitled.hhmap") == 0,
           "new application document displays the untitled filename");
    expect(!hhms_app_dirty(&app), "new untitled document starts clean");
    expect(app.map.ntiles == 0 && app.map.nsupports == 0,
           "new untitled document starts with empty authored map state");
    expect(app.analysis_valid && !app.analysis.contradiction,
           "new untitled document starts with current consistent analysis");
    hhms_app_destroy(&app);
}

static void test_save_as_noop_and_saved_revision(const TempDirectory *directory)
{
    static HhmsApp app;
    char path[640];
    temp_path(directory, "saved.hhmap", path, sizeof(path));
    hhms_app_init(&app);
    expect(app_edit_tile(&app, 2, 3, HHMS_CLEAR, 1),
           "authored tile edit commits before Save As");
    expect(hhms_app_dirty(&app), "authored tile edit makes untitled document dirty");
    expect(hhms_app_save_as(&app, path) == HHMS_APP_OK,
           "Save As establishes a document path");
    expect(app.path && strcmp(app.path, path) == 0,
           "Save As stores the exact selected path");
    expect(strcmp(hhms_app_filename(&app), "saved.hhmap") == 0,
           "Save As exposes the selected path's basename");
    expect(!hhms_app_dirty(&app), "successful Save As establishes a clean saved revision");
    uint64_t saved = app.saved_state;

    expect(hhms_app_begin_edit(&app), "no-op edit transaction begins");
    expect(hhms_set_tile(&app.map, 2, 3, HHMS_CLEAR, 1) == HHMS_EDIT_NO_CHANGE,
           "repeating the exact authored tile is a no-op");
    expect(!hhms_app_commit_edit(&app), "no-op edit transaction creates no revision");
    expect(app.saved_state == saved && hhms_state_id(&app.map) == saved,
           "repeated no-op preserves the exact saved-state id");
    expect(!hhms_app_dirty(&app), "repeated no-op remains clean");

    expect(app_edit_tile(&app, 4, 3, HHMS_MINE, 0),
           "post-save tile edit commits a new revision");
    expect(hhms_app_dirty(&app), "post-save tile edit makes document dirty");
    expect(hhms_app_undo(&app), "undo returns from post-save revision");
    expect(hhms_state_id(&app.map) == saved && !hhms_app_dirty(&app),
           "undo to the saved state id clears dirty");
    expect(hhms_app_redo(&app), "redo reapplies the post-save revision");
    expect(hhms_app_dirty(&app), "redo away from the saved state dirties the document");
    uint64_t redone = hhms_state_id(&app.map);
    expect(hhms_app_undo(&app) && !hhms_app_dirty(&app),
           "second undo returns exactly to the clean saved revision");
    expect(app_edit_tile(&app, 5, 3, HHMS_OPEN, 0),
           "new edit commits after undoing to the saved revision");
    expect(!hhms_can_redo(&app.map) && hhms_state_id(&app.map) != saved &&
           hhms_state_id(&app.map) != redone && hhms_app_dirty(&app),
           "redo branching cannot reuse a saved id or falsely report the new branch clean");

    hhms_app_destroy(&app);
    remove(path);
}

static void test_history_cap_never_false_cleans(const TempDirectory *directory)
{
    static HhmsApp app;
    char path[640];
    temp_path(directory, "history-cap.hhmap", path, sizeof(path));
    hhms_app_init(&app);
    expect(app_edit_tile(&app, -1, 50, HHMS_OPEN, 0),
           "history-cap test commits the revision that will be saved");
    expect(hhms_app_save_as(&app, path) == HHMS_APP_OK,
           "history-cap test establishes an early saved revision");
    uint64_t saved = app.saved_state;
    int all_committed = 1;
    for (int i = 0; i < HHMS_MAX_HISTORY + 8; i++) {
        if (!app_edit_tile(&app, i, 50, HHMS_OPEN, 0)) {
            all_committed = 0;
            break;
        }
    }
    expect(all_committed && hhms_app_dirty(&app),
           "edits beyond history capacity remain dirty after the saved revision is evicted");
    int undos = 0;
    while (hhms_app_undo(&app))
        undos++;
    expect(undos == HHMS_MAX_HISTORY,
           "history cap retains exactly the bounded number of undo transactions");
    expect(hhms_state_id(&app.map) != saved && hhms_app_dirty(&app),
           "undoing retained history cannot falsely clean after saved revision eviction");
    while (hhms_app_redo(&app)) {
    }
    expect(hhms_app_dirty(&app),
           "redo after history eviction remains dirty relative to the unreachable savepoint");
    hhms_app_destroy(&app);
    remove(path);
}

static void test_view_dirty_baseline(const TempDirectory *directory)
{
    static HhmsApp app;
    char path[640];
    temp_path(directory, "view.hhmap", path, sizeof(path));
    hhms_app_init(&app);
    expect(app_edit_tile(&app, 0, 0, HHMS_OPEN, 0),
           "view dirty test commits initial authored map state");
    expect(hhms_app_save_as(&app, path) == HHMS_APP_OK,
           "view dirty test establishes a saved document");
    app.view.camx += 3.5;
    app.view.cell += 2.0f;
    expect(hhms_app_dirty(&app), "camera navigation makes the document dirty");

    expect(app_edit_tile(&app, 1, 1, HHMS_MINE, 0),
           "map edit commits while view also differs from saved baseline");
    expect(hhms_app_undo(&app), "map edit can be undone while view remains changed");
    expect(hhms_state_id(&app.map) == app.saved_state,
           "map undo reaches the saved map revision despite view change");
    expect(hhms_app_dirty(&app),
           "map undo cannot falsely clear dirty while viewport differs from saved baseline");
    expect(hhms_app_save(&app) == HHMS_APP_OK,
           "successful save accepts changed map and view baselines");
    expect(!hhms_app_dirty(&app), "successful save clears both map and viewport dirty state");

    hhms_app_destroy(&app);
    remove(path);
}

static HhmsAppResult request_indexed_action(HhmsApp *app, int action,
                                            const char *load_path)
{
    if (action == 0)
        return hhms_app_request_new(app);
    if (action == 1)
        return hhms_app_request_load(app, load_path);
    return hhms_app_request_close(app);
}

static HhmsPendingAction pending_for_action(int action)
{
    if (action == 0)
        return HHMS_PENDING_NEW;
    if (action == 1)
        return HHMS_PENDING_LOAD;
    return HHMS_PENDING_CLOSE;
}

static void test_all_dirty_transitions_require_confirmation(const TempDirectory *directory)
{
    static HhmsMap target_map;
    HhmsView target_view;
    HhmsError error;
    char target_path[640];
    temp_path(directory, "load-target.hhmap", target_path, sizeof(target_path));
    hhms_init(&target_map);
    hhms_set_tile(&target_map, 99, 99, HHMS_OPEN, 0);
    hhms_view_init(&target_view);
    target_view.has_view = 1;
    expect(hhms_save(&target_map, &target_view, target_path, &error) == HHMS_IO_OK,
           "confirmation tests create a real load target");

    static HhmsApp app;
    char behavior[160];
    for (int change = 0; change < 2; change++) {
        for (int action = 0; action < 3; action++) {
            hhms_app_init(&app);
            int committed = change == 0
                ? app_edit_tile(&app, 7, 8, HHMS_MINE, 0)
                : app_edit_support(&app, 1.25, -0.5, HHMS_SUP_WOOD, NULL);
            snprintf(behavior, sizeof(behavior),
                     "%s-only change commits before %s request",
                     change == 0 ? "tile" : "support",
                     action == 0 ? "New" : action == 1 ? "Load" : "Close");
            expect(committed, behavior);
            HhmsAppResult result = request_indexed_action(&app, action, target_path);
            snprintf(behavior, sizeof(behavior),
                     "%s requires confirmation for a %s-only change",
                     action == 0 ? "New" : action == 1 ? "Load" : "Close",
                     change == 0 ? "tile" : "support");
            expect(result == HHMS_APP_NEEDS_CONFIRMATION, behavior);
            expect(app.pending == pending_for_action(action),
                   "confirmation preserves the exact pending document action");
            expect(change == 0 ? hhms_get(&app.map, 7, 8) != NULL
                               : app.map.nsupports == 1,
                   "confirmation leaves the dirty authored document intact");
            hhms_app_resolve_unsaved(&app, HHMS_UNSAVED_CANCEL, NULL);
            hhms_app_destroy(&app);
        }
    }
    remove(target_path);
}

static void test_cancel_preserves_exact_document(const TempDirectory *directory)
{
    static HhmsApp app;
    static HhmsMap before_map;
    HhmsView before_view;
    char original_path[640];
    char load_path[640];
    temp_path(directory, "original.hhmap", original_path, sizeof(original_path));
    temp_path(directory, "cancel-target.hhmap", load_path, sizeof(load_path));

    static HhmsMap target;
    HhmsView target_view;
    HhmsError error;
    hhms_init(&target);
    hhms_set_tile(&target, 55, 55, HHMS_OPEN, 0);
    hhms_view_init(&target_view);
    target_view.has_view = 1;
    expect(hhms_save(&target, &target_view, load_path, &error) == HHMS_IO_OK,
           "cancel test creates a real alternate load target");

    hhms_app_init(&app);
    expect(app_edit_tile(&app, 1, 2, HHMS_CLEAR, 3),
           "cancel test commits initial document state");
    app.view.camx = 12.25;
    app.view.camy = -7.5;
    expect(hhms_app_save_as(&app, original_path) == HHMS_APP_OK,
           "cancel test establishes original path and saved view");
    expect(app_edit_support(&app, 3.0, 4.0, HHMS_SUP_STONE, NULL),
           "cancel test adds a dirty support-only revision");
    app.view.cell += 4.0f;
    before_map = app.map;
    before_view = app.view;
    uint64_t before_saved_state = app.saved_state;

    expect(hhms_app_request_load(&app, load_path) == HHMS_APP_NEEDS_CONFIRMATION,
           "dirty Load request enters pending confirmation");
    expect(hhms_app_resolve_unsaved(&app, HHMS_UNSAVED_CANCEL, NULL) ==
           HHMS_APP_NO_CHANGE,
           "Cancel resolves pending Load without executing it");
    expect(memcmp(&app.map, &before_map, sizeof(before_map)) == 0,
           "Cancel preserves exact authored map and history state");
    expect(view_equal(app.view, before_view), "Cancel preserves exact viewport state");
    expect(app.path && strcmp(app.path, original_path) == 0,
           "Cancel preserves exact original document path");
    expect(app.saved_state == before_saved_state && hhms_app_dirty(&app),
           "Cancel preserves saved revision baseline and dirty state");
    expect(app.pending == HHMS_PENDING_NONE && app.pending_path == NULL,
           "Cancel clears only the pending action state");

    hhms_app_destroy(&app);
    remove(original_path);
    remove(load_path);
}

static void test_pending_save_success_failure_and_path_requirement(
    const TempDirectory *directory)
{
    static HhmsApp app;
    char saved_path[640];
    char missing_path[700];
    temp_path(directory, "pending-save.hhmap", saved_path, sizeof(saved_path));
    temp_path(directory, "missing/subdir/save.hhmap", missing_path, sizeof(missing_path));

    hhms_app_init(&app);
    expect(hhms_app_save(&app) == HHMS_APP_NEEDS_SAVE_PATH,
           "saving an untitled document requires a save path");
    expect(app_edit_tile(&app, 6, 6, HHMS_MINE, 0),
           "pending-save success test commits a dirty edit");
    expect(hhms_app_request_new(&app) == HHMS_APP_NEEDS_CONFIRMATION,
           "dirty New waits for unsaved resolution");
    expect(hhms_app_resolve_unsaved(&app, HHMS_UNSAVED_SAVE, NULL) ==
           HHMS_APP_NEEDS_SAVE_PATH,
           "Save resolution without an established or supplied path requests one");
    expect(app.pending == HHMS_PENDING_NEW && hhms_get(&app.map, 6, 6),
           "save-path requirement preserves pending New and dirty document");
    expect(hhms_app_resolve_unsaved(&app, HHMS_UNSAVED_SAVE, saved_path) == HHMS_APP_OK,
           "successful Save then executes pending New");
    expect(app.path == NULL && app.map.ntiles == 0 && !hhms_app_dirty(&app),
           "pending New proceeds only after save succeeds and creates a clean untitled document");

    static HhmsMap saved;
    HhmsView saved_view;
    HhmsError error;
    hhms_init(&saved);
    hhms_view_init(&saved_view);
    expect(hhms_load(&saved, &saved_view, saved_path, &error) == HHMS_IO_OK &&
           hhms_get(&saved, 6, 6) != NULL,
           "pending action's successful save persists the pre-action document");
    hhms_app_destroy(&app);

    hhms_app_init(&app);
    expect(app_edit_support(&app, 0.5, 0.5, HHMS_SUP_BEAM, NULL),
           "pending-save failure test commits a support-only change");
    expect(hhms_app_request_close(&app) == HHMS_APP_NEEDS_CONFIRMATION,
           "dirty Close waits for unsaved resolution");
    uint64_t state = hhms_state_id(&app.map);
    HhmsView view = app.view;
    expect(hhms_app_resolve_unsaved(&app, HHMS_UNSAVED_SAVE, missing_path) ==
           HHMS_APP_ERROR,
           "failed Save prevents pending Close from proceeding");
    expect(!app.quit && app.pending == HHMS_PENDING_CLOSE,
           "failed Save keeps the application open with Close still pending");
    expect(hhms_state_id(&app.map) == state && app.map.nsupports == 1 &&
           view_equal(app.view, view) && hhms_app_dirty(&app),
           "failed pending Save preserves document state view and dirty baseline");
    expect(hhms_app_resolve_unsaved(&app, HHMS_UNSAVED_DISCARD, NULL) == HHMS_APP_QUIT &&
           app.quit,
           "Discard completes the still-pending Close action");
    hhms_app_destroy(&app);

    remove(saved_path);
}

static void test_bad_load_preserves_document(const TempDirectory *directory)
{
    static HhmsApp app;
    static HhmsMap before_map;
    HhmsView before_view;
    char good_path[640];
    char bad_path[640];
    temp_path(directory, "good.hhmap", good_path, sizeof(good_path));
    temp_path(directory, "bad.hhmap", bad_path, sizeof(bad_path));

    hhms_app_init(&app);
    expect(app_edit_tile(&app, -3, 9, HHMS_CLEAR, 2),
           "bad-load test commits source document state");
    app.view.camx = 44.5;
    app.view.camy = -18.25;
    expect(hhms_app_save_as(&app, good_path) == HHMS_APP_OK,
           "bad-load test establishes a clean source document");
    expect(write_text(bad_path, "{ invalid document"),
           "bad-load fixture is written outside the repository");
    before_map = app.map;
    before_view = app.view;
    uint64_t saved_state = app.saved_state;
    HhmsView saved_view = app.saved_view;

    expect(hhms_app_request_load(&app, bad_path) == HHMS_APP_ERROR,
           "malformed Load reports an application error");
    expect(memcmp(&app.map, &before_map, sizeof(before_map)) == 0,
           "failed Load preserves exact map state and history");
    expect(view_equal(app.view, before_view) && view_equal(app.saved_view, saved_view),
           "failed Load preserves current and saved viewport baselines");
    expect(app.path && strcmp(app.path, good_path) == 0 &&
           app.saved_state == saved_state && !hhms_app_dirty(&app),
           "failed Load preserves path saved revision and clean state");

    hhms_app_destroy(&app);
    remove(good_path);
    remove(bad_path);
}

static void test_odds_formatter(void)
{
    char buffer[160];
    hhms_format_odds(0, 5, 0, buffer, sizeof(buffer));
    expect(strcmp(buffer, "0%") == 0,
           "odds formatter reserves literal zero percent for mathematical proof");
    hhms_format_odds(5, 5, 0, buffer, sizeof(buffer));
    expect(strcmp(buffer, "100%") == 0,
           "odds formatter reserves literal one hundred percent for mathematical proof");
    hhms_format_odds(0, 5, 1, buffer, sizeof(buffer));
    expect(strcmp(buffer, "0% (proved safe)") == 0,
           "detailed zero odds identify a proved-safe result");
    hhms_format_odds(5, 5, 1, buffer, sizeof(buffer));
    expect(strcmp(buffer, "100% (proved cave)") == 0,
           "detailed full odds identify a proved-cave result");
    hhms_format_odds(1, 101, 0, buffer, sizeof(buffer));
    expect(strcmp(buffer, "<1%") == 0,
           "nonzero odds below one percent never round to proof-like zero");
    hhms_format_odds(100, 101, 0, buffer, sizeof(buffer));
    expect(strcmp(buffer, ">99%") == 0,
           "non-certain odds above ninety-nine percent never round to proof-like one hundred");
    hhms_format_odds(0, 0, 0, buffer, sizeof(buffer));
    expect(strcmp(buffer, "unresolved") == 0,
           "missing model counts format as unresolved rather than proof");
}

static void test_cell_formatter(void)
{
    static HhmsApp app;
    char buffer[256];

    hhms_app_init(&app);
    hhms_set_tile(&app.map, 4, 4, HHMS_MINE, 0);
    hhms_app_solve(&app);
    hhms_app_format_cell(&app, 4, 4, buffer, sizeof(buffer));
    expect(strstr(buffer, "CAVE") && strstr(buffer, "flagged by you"),
           "cell formatter distinguishes an authored flag from a derived proof");
    hhms_app_destroy(&app);

    hhms_app_init(&app);
    hhms_set_tile(&app.map, 0, 0, HHMS_CLEAR, 8);
    hhms_app_solve(&app);
    hhms_app_format_cell(&app, 1, 0, buffer, sizeof(buffer));
    expect(strstr(buffer, "CAVE") && strstr(buffer, "proved from counts"),
           "cell formatter identifies a solver-proved cave");
    hhms_add_support(&app.map, 1.0, 0.0, HHMS_SUP_WOOD,
                     HHMS_ORIENT_NORTH, NULL);
    hhms_app_solve(&app);
    hhms_app_format_cell(&app, 1, 0, buffer, sizeof(buffer));
    expect(strstr(buffer, "PROTECTED CAVE") && strstr(buffer, "wood covers it"),
           "cell formatter identifies confirmed radial cave protection");
    hhms_app_destroy(&app);

    hhms_app_init(&app);
    hhms_set_tile(&app.map, 0, 0, HHMS_CLEAR, 0);
    hhms_set_terrain(&app.map, 1, 0, HHMS_TERRAIN_WATER);
    hhms_app_solve(&app);
    hhms_app_format_cell(&app, 1, 0, buffer, sizeof(buffer));
    expect(strstr(buffer, "water") && strstr(buffer, "proved free") &&
           !strstr(buffer, "DIG"),
           "cell formatter distinguishes proved-safe water from mineable DIG");
    hhms_app_destroy(&app);

    hhms_app_init(&app);
    hhms_set_tile(&app.map, 0, 0, HHMS_CLEAR, 0);
    hhms_set_tile(&app.map, 1, 0, HHMS_MINE, 0);
    hhms_app_solve(&app);
    hhms_app_format_cell(&app, 0, 0, buffer, sizeof(buffer));
    expect(strstr(buffer, "CONFLICT") && strstr(buffer, "disagrees"),
           "cell formatter distinguishes a contradiction conflict");
    hhms_app_destroy(&app);

    hhms_app_init(&app);
    for (int x = 0; x < 8; x++)
        hhms_set_tile(&app.map, x, 0, HHMS_CLEAR, 1);
    hhms_set_tile(&app.map, -1, -1, HHMS_OPEN, 0);
    hhms_app_solve(&app);
    expect(!app.analysis.complete &&
           (app.analysis.limits & HHMS_LIMIT_ENUM_COMPONENT) != 0,
           "cell-format limit fixture exceeds the exact frontier bound");
    hhms_app_format_cell(&app, 0, -1, buffer, sizeof(buffer));
    expect(strstr(buffer, "limit") != NULL,
           "cell formatter distinguishes analysis-limit unknowns from ordinary unknown walls");
    hhms_app_destroy(&app);
}

static void test_v2_load_restores_clean_view(const TempDirectory *directory)
{
    static HhmsMap map;
    HhmsView view;
    HhmsError error;
    char path[640];
    temp_path(directory, "loaded-view.hhmap", path, sizeof(path));
    hhms_init(&map);
    hhms_set_tile(&map, 3, -7, HHMS_MINE, 0);
    hhms_view_init(&view);
    view.camx = 17.75;
    view.camy = -22.125;
    view.cell = 46.5f;
    view.ui_scale = 1.5f;
    view.has_view = 1;
    expect(hhms_save(&map, &view, path, &error) == HHMS_IO_OK,
           "load-view test saves a real version-two document");

    static HhmsApp app;
    hhms_app_init(&app);
    expect(hhms_app_request_load(&app, path) == HHMS_APP_OK,
           "clean application loads version-two document without confirmation");
    expect(view_equal(app.view, view), "version-two Load restores the exact saved viewport");
    expect(view_equal(app.saved_view, view) && !hhms_app_dirty(&app),
           "version-two Load establishes the restored viewport as a clean baseline");
    expect(app.path && strcmp(app.path, path) == 0 && hhms_get(&app.map, 3, -7),
           "version-two Load establishes loaded path and authored document state");
    hhms_app_destroy(&app);
    remove(path);
}

static void test_active_edit_commands_finalize(const TempDirectory *directory)
{
    static HhmsApp app;
    char path[640];
    temp_path(directory, "active-edit.hhmap", path, sizeof(path));
    hhms_app_init(&app);
    expect(hhms_app_save_as(&app, path) == HHMS_APP_OK,
           "active-edit test establishes a clean document path");

    expect(hhms_app_begin_edit(&app) &&
           hhms_set_tile(&app.map, 3, 4, HHMS_OPEN, 0) == HHMS_EDIT_OK,
           "active stroke mutates authored state before commit");
    expect(app.map.history.active && hhms_app_dirty(&app),
           "non-empty active stroke is immediately dirty");
    expect(hhms_app_request_new(&app) == HHMS_APP_NEEDS_CONFIRMATION,
           "New finalizes and guards an active stroke");
    expect(!app.map.history.active && hhms_get(&app.map, 3, 4) &&
           hhms_can_undo(&app.map),
           "New request commits the active stroke into history before prompting");
    expect(hhms_app_resolve_unsaved(&app, HHMS_UNSAVED_CANCEL, NULL) ==
           HHMS_APP_NO_CHANGE && hhms_get(&app.map, 3, 4),
           "cancelling guarded New preserves the finalized stroke");
    expect(hhms_app_undo(&app) && !hhms_app_dirty(&app),
           "finalized active stroke undoes to the saved baseline");

    expect(hhms_app_begin_edit(&app) &&
           hhms_set_tile(&app.map, 6, 7, HHMS_MINE, 0) == HHMS_EDIT_OK,
           "active save stroke begins");
    expect(hhms_app_save(&app) == HHMS_APP_OK &&
           !app.map.history.active && !hhms_app_dirty(&app),
           "Save commits an active stroke before establishing the saved baseline");

    expect(hhms_app_begin_edit(&app) &&
           hhms_set_tile(&app.map, 8, 9, HHMS_OPEN, 0) == HHMS_EDIT_OK,
           "active close stroke begins");
    expect(hhms_app_request_close(&app) == HHMS_APP_NEEDS_CONFIRMATION &&
           !app.map.history.active && app.pending == HHMS_PENDING_CLOSE,
           "Close finalizes and guards an active stroke");
    expect(hhms_app_resolve_unsaved(&app, HHMS_UNSAVED_CANCEL, NULL) ==
           HHMS_APP_NO_CHANGE && !app.quit,
           "cancelling guarded Close keeps the finalized document open");

    hhms_app_destroy(&app);
    remove(path);
}

int main(void)
{
    TempDirectory directory;
    if (!temp_directory_create(&directory)) {
        expect(0, "isolated temporary app-test directory can be created");
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return 1;
    }
    expect(1, "app tests use an isolated temporary directory outside the repository");

    test_new_untitled_document();
    test_save_as_noop_and_saved_revision(&directory);
    test_history_cap_never_false_cleans(&directory);
    test_view_dirty_baseline(&directory);
    test_all_dirty_transitions_require_confirmation(&directory);
    test_cancel_preserves_exact_document(&directory);
    test_pending_save_success_failure_and_path_requirement(&directory);
    test_bad_load_preserves_document(&directory);
    test_odds_formatter();
    test_cell_formatter();
    test_active_edit_commands_finalize(&directory);
    test_v2_load_restores_clean_view(&directory);

    expect(rmdir(directory.path) == 0,
           "temporary app-test directory is empty and cleaned up");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
