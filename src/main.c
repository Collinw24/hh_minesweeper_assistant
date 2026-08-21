#include "app.h"
#include "dialogs.h"
#include "gui.h"

#include "raylib.h"
#include <stdio.h>
#include <string.h>


int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("HH Minesweeper %s\n", HHMS_VERSION);
        return 0;
    }
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 ||
                     strcmp(argv[1], "-h") == 0)) {
        printf("Usage: hhms [map.hhmap]\n");
        printf("       hhms --version\n");
        return 0;
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_TOPMOST |
                   FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT);
    InitWindow(1100, 720, "HH Minesweeper " HHMS_VERSION);
    SetWindowMinSize(760, 540);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    static HhmsApp app;
    hhms_app_init(&app);
    char dialog_error[HHMS_APP_MESSAGE_MAX];
    if (!hhms_dialogs_init(dialog_error, sizeof(dialog_error)))
        hhms_app_set_message(&app, dialog_error);

    static HhmsGui gui;
    hhms_gui_init(&gui, &app, argc <= 1);
    if (argc > 1)
        hhms_gui_request_load(&gui, argv[1]);

    while (!app.quit) {
        hhms_gui_process(&gui);
        if (app.quit)
            break;
        BeginDrawing();
        hhms_gui_draw(&gui);
        EndDrawing();
    }

    hhms_dialogs_quit();
    hhms_app_destroy(&app);
    CloseWindow();
    return 0;
}
