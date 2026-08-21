#ifndef HHMS_GUI_H
#define HHMS_GUI_H

#include "app.h"
#include "raylib.h"

#include <stdint.h>

typedef enum {
    HHMS_BRUSH_COUNT = 0,
    HHMS_BRUSH_OPEN,
    HHMS_BRUSH_WATER,
    HHMS_BRUSH_FLAG,
    HHMS_BRUSH_ERASE,
    HHMS_BRUSH_SUPPORT
} HhmsBrush;

typedef struct {
    HhmsApp *app;
    HhmsBrush brush;
    int count;
    HhmsSupportKind support_kind;
    HhmsOrientation orientation;
    int topmost;
    int help;
    int focus;
    int modal_focus;
    int hover_board;
    int hover_x, hover_y;
    int cursor_x, cursor_y;
    int cursor_set;
    int painting;
    int paint_x, paint_y;
    int close_latched;
    int dragging_pan;
    int space_board_pending;
    float sidebar_scroll;
    uint32_t remove_warning_id;
    uint32_t selected_support_id;
    char title[512];
} HhmsGui;

void hhms_gui_init(HhmsGui *gui, HhmsApp *app, int show_first_run_help);
void hhms_gui_process(HhmsGui *gui);
void hhms_gui_draw(const HhmsGui *gui);
void hhms_gui_request_load(HhmsGui *gui, const char *path);

#endif
