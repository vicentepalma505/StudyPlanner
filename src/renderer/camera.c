#include "camera.h"

#include <math.h>

#include "theme.h"

CameraView CameraViewNew(void)
{
    CameraView cam;
    cam.pos = (Vector2){ 0.0f, 0.0f };
    cam.zoom = 1.0f;
    return cam;
}

Camera2D CameraToRaylib(CameraView cam, Rectangle viewport)
{
    Camera2D c;
    c.target = cam.pos;
    c.offset = (Vector2){ viewport.x + viewport.width * 0.5f,
                          viewport.y + viewport.height * 0.5f };
    c.rotation = 0.0f;
    c.zoom = cam.zoom;
    return c;
}

Vector2 CameraWorldToScreen(CameraView cam, Rectangle viewport, Vector2 world)
{
    Camera2D c = CameraToRaylib(cam, viewport);
    return GetWorldToScreen2D(world, c);
}

Vector2 CameraScreenToWorld(CameraView cam, Rectangle viewport, Vector2 screen)
{
    Camera2D c = CameraToRaylib(cam, viewport);
    return GetScreenToWorld2D(screen, c);
}

// US-15 RF-09: umbrales (px) de espaciado visual para el grid adaptativo.
#define GRID_VISUAL_MIN 12.0f
#define GRID_VISUAL_MAX 96.0f

void DrawCanvasGrid(CameraView cam, Rectangle viewport)
{
    // Grid adaptativo (US-15 RF-09): el espaciado base GRID_SPACING se escala
    // con el zoom; si el espaciado visual queda por debajo de GRID_VISUAL_MIN
    // se multiplica por 2 (y si supera GRID_VISUAL_MAX se divide), siempre por
    // potencias de 2 para mantener la legibilidad en todo el rango.
    float spacing = (float)GRID_SPACING;
    float visual = spacing * cam.zoom;
    while (visual < GRID_VISUAL_MIN)
    {
        spacing *= 2.0f;
        visual *= 2.0f;
    }
    while (visual > GRID_VISUAL_MAX)
    {
        spacing *= 0.5f;
        visual *= 0.5f;
    }

    Vector2 tl = CameraScreenToWorld(cam, viewport, (Vector2){ viewport.x, viewport.y });
    Vector2 br = CameraScreenToWorld(cam, viewport, (Vector2){ viewport.x + viewport.width,
                                                               viewport.y + viewport.height });

    // Las líneas se anclan a múltiplos del espaciado en el origen de mundo, de
    // modo que se desplazan correctamente al hacer pan.
    for (float wx = floorf(tl.x / spacing) * spacing; wx <= br.x; wx += spacing)
    {
        float sx = CameraWorldToScreen(cam, viewport, (Vector2){ wx, 0.0f }).x;
        DrawLineV((Vector2){ sx, viewport.y }, (Vector2){ sx, viewport.y + viewport.height }, COLOR_GRID);
    }

    for (float wy = floorf(tl.y / spacing) * spacing; wy <= br.y; wy += spacing)
    {
        float sy = CameraWorldToScreen(cam, viewport, (Vector2){ 0.0f, wy }).y;
        DrawLineV((Vector2){ viewport.x, sy }, (Vector2){ viewport.x + viewport.width, sy }, COLOR_GRID);
    }
}
