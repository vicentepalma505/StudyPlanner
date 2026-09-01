#ifndef CAMERA_H
#define CAMERA_H

#include "raylib.h"

// Cámara de vista del lienzo: camaraPos (centro en coordenadas de mundo) + zoom.
// No se persiste en el archivo de malla (US-15/US-16).
typedef struct {
    Vector2 pos;
    float zoom;
} CameraView;

CameraView CameraViewNew(void);

// Construye la Camera2D de raylib equivalente para un viewport dado.
Camera2D CameraToRaylib(CameraView cam, Rectangle viewport);

Vector2 CameraWorldToScreen(CameraView cam, Rectangle viewport, Vector2 world);
Vector2 CameraScreenToWorld(CameraView cam, Rectangle viewport, Vector2 screen);

// Dibuja el grid de fondo (espaciado base GRID_SPACING) en el viewport.
void DrawCanvasGrid(CameraView cam, Rectangle viewport);

#endif
