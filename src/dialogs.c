#include "dialogs.h"

#include "nfd.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>

static const nfdu8filteritem_t HHMS_FILTERS[] = {{"HH map", "hhmap"}};
static int dialogs_ready;

static void set_error(char *error, size_t capacity, const char *detail)
{
    if (!error || capacity == 0)
        return;
    snprintf(error, capacity,
             "File dialog failed: %s. Check desktop integration and try again.",
             detail && detail[0] ? detail : "unknown native dialog error");
}

int hhms_dialogs_init(char *error, size_t capacity)
{
    nfdresult_t result = NFD_Init();
    dialogs_ready = result == NFD_OKAY;
    if (dialogs_ready)
        return 1;
    set_error(error, capacity, NFD_GetError());
    return 0;
}

void hhms_dialogs_quit(void)
{
    if (dialogs_ready) {
        NFD_Quit();
        dialogs_ready = 0;
    }
}

static void leave_topmost(int restore_topmost)
{
    if (restore_topmost)
        ClearWindowState(FLAG_WINDOW_TOPMOST);
}

static void restore_topmost_state(int restore_topmost)
{
    if (restore_topmost)
        SetWindowState(FLAG_WINDOW_TOPMOST);
}

HhmsDialogResult hhms_dialog_open(char **path, int restore_topmost,
                                  char *error, size_t capacity)
{
    if (path)
        *path = NULL;
    if (!dialogs_ready) {
        set_error(error, capacity, "native dialogs did not initialize");
        return HHMS_DIALOG_ERROR;
    }
    nfdopendialogu8args_t args = {0};
    args.filterList = HHMS_FILTERS;
    args.filterCount = 1;
    args.parentWindow = (nfdwindowhandle_t){0};
    nfdu8char_t *selected = NULL;
    leave_topmost(restore_topmost);
    nfdresult_t result = NFD_OpenDialogU8_With(&selected, &args);
    restore_topmost_state(restore_topmost);
    if (result == NFD_OKAY) {
        if (path)
            *path = (char *)selected;
        else
            NFD_FreePathU8(selected);
        return HHMS_DIALOG_OK;
    }
    if (result == NFD_CANCEL)
        return HHMS_DIALOG_CANCEL;
    set_error(error, capacity, NFD_GetError());
    return HHMS_DIALOG_ERROR;
}

HhmsDialogResult hhms_dialog_save(char **path, int restore_topmost,
                                  const char *default_name,
                                  char *error, size_t capacity)
{
    if (path)
        *path = NULL;
    if (!dialogs_ready) {
        set_error(error, capacity, "native dialogs did not initialize");
        return HHMS_DIALOG_ERROR;
    }
    nfdsavedialogu8args_t args = {0};
    args.filterList = HHMS_FILTERS;
    args.filterCount = 1;
    args.defaultName = default_name && default_name[0]
        ? (const nfdu8char_t *)default_name : (const nfdu8char_t *)"Untitled.hhmap";
    args.parentWindow = (nfdwindowhandle_t){0};
    nfdu8char_t *selected = NULL;
    leave_topmost(restore_topmost);
    nfdresult_t result = NFD_SaveDialogU8_With(&selected, &args);
    restore_topmost_state(restore_topmost);
    if (result == NFD_OKAY) {
        if (path)
            *path = (char *)selected;
        else
            NFD_FreePathU8(selected);
        return HHMS_DIALOG_OK;
    }
    if (result == NFD_CANCEL)
        return HHMS_DIALOG_CANCEL;
    set_error(error, capacity, NFD_GetError());
    return HHMS_DIALOG_ERROR;
}

void hhms_dialog_free(char *path)
{
    if (path)
        NFD_FreePathU8((nfdu8char_t *)path);
}
