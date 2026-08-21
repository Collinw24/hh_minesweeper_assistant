#ifndef HHMS_DIALOGS_H
#define HHMS_DIALOGS_H

#include <stddef.h>

typedef enum {
    HHMS_DIALOG_OK = 0,
    HHMS_DIALOG_CANCEL,
    HHMS_DIALOG_ERROR
} HhmsDialogResult;

int hhms_dialogs_init(char *error, size_t capacity);
void hhms_dialogs_quit(void);
HhmsDialogResult hhms_dialog_open(char **path, int restore_topmost,
                                  char *error, size_t capacity);
HhmsDialogResult hhms_dialog_save(char **path, int restore_topmost,
                                  const char *default_name,
                                  char *error, size_t capacity);
void hhms_dialog_free(char *path);

#endif
