#ifndef HHMS_APP_H
#define HHMS_APP_H

#include "hhms.h"
#ifndef HHMS_VERSION
#define HHMS_VERSION "0.0.3"
#endif


#define HHMS_APP_MESSAGE_MAX 384

typedef enum {
    HHMS_PENDING_NONE = 0,
    HHMS_PENDING_NEW,
    HHMS_PENDING_LOAD,
    HHMS_PENDING_CLOSE
} HhmsPendingAction;

typedef enum {
    HHMS_UNSAVED_SAVE = 0,
    HHMS_UNSAVED_DISCARD,
    HHMS_UNSAVED_CANCEL
} HhmsUnsavedChoice;

typedef enum {
    HHMS_APP_OK = 0,
    HHMS_APP_NO_CHANGE,
    HHMS_APP_NEEDS_CONFIRMATION,
    HHMS_APP_NEEDS_SAVE_PATH,
    HHMS_APP_QUIT,
    HHMS_APP_ERROR
} HhmsAppResult;

typedef struct {
    HhmsMap map;
    HhmsAnalysis analysis;
    HhmsView view;
    HhmsView saved_view;
    uint64_t saved_state;
    char *path;
    char *pending_path;
    HhmsPendingAction pending;
    int quit;
    int analysis_valid;
    int fit_after_load;
    char message[HHMS_APP_MESSAGE_MAX];
} HhmsApp;

void hhms_app_init(HhmsApp *app);
void hhms_app_destroy(HhmsApp *app);
int hhms_app_dirty(const HhmsApp *app);
const char *hhms_app_filename(const HhmsApp *app);
void hhms_app_set_message(HhmsApp *app, const char *message);

HhmsAppResult hhms_app_request_new(HhmsApp *app);
HhmsAppResult hhms_app_request_load(HhmsApp *app, const char *path);
HhmsAppResult hhms_app_request_close(HhmsApp *app);
HhmsAppResult hhms_app_resolve_unsaved(HhmsApp *app, HhmsUnsavedChoice choice,
                                       const char *save_path);
HhmsAppResult hhms_app_save(HhmsApp *app);
HhmsAppResult hhms_app_save_as(HhmsApp *app, const char *path);

int hhms_app_begin_edit(HhmsApp *app);
int hhms_app_commit_edit(HhmsApp *app);
void hhms_app_cancel_edit(HhmsApp *app);
int hhms_app_undo(HhmsApp *app);
int hhms_app_redo(HhmsApp *app);
void hhms_app_solve(HhmsApp *app);

void hhms_app_reset_view(HhmsApp *app);
void hhms_app_fit_view(HhmsApp *app, float grid_width, float grid_height,
                       int mark_dirty);
void hhms_app_accept_loaded_fit(HhmsApp *app);

void hhms_format_odds(uint64_t mine_models, uint64_t total_models,
                      int detailed, char *buffer, size_t capacity);
void hhms_app_format_cell(const HhmsApp *app, int x, int y,
                          char *buffer, size_t capacity);
const char *hhms_reason_name(HhmsReason reason);

#endif
