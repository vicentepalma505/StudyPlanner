#include "raylib.h"

#include "app.h"
#include "theme.h"

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT, "Visor de Mallas Universitarias");
    SetWindowMinSize(WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);
    SetTargetFPS(60);

    // Esc no cierra la ventana: se usa para cerrar modales (US-02 RF-10).
    SetExitKey(0);

    App app;
    AppInit(&app);

    // US-19 RF-04: el cierre de ventana con cambios sin guardar pasa por la
    // confirmación; 'cerrarAprobado' indica que ya se autorizó la salida.
    while (!WindowShouldClose() && !app.cerrarAprobado)
    {
        AppUpdate(&app);
        AppDraw(&app);
    }

    AppShutdown(&app);
    CloseWindow();
    return 0;
}
