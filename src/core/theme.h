#ifndef THEME_H
#define THEME_H

#include "raylib.h"

#include "types.h"


// Ventana
#define WINDOW_MIN_WIDTH   1280
#define WINDOW_MIN_HEIGHT  720

// Layout
#define TOOLBAR_HEIGHT     40
#define PANEL_WIDTH        240   // panel lateral de áreas (US-03)

// Lienzo / grid (US-01)
#define GRID_SPACING       32
#define COLOR_GRID         CLITERAL(Color){ 246, 173, 201, 155 }

// Constantes del tema (reglas transversales de las specs)
#define THEME_GRIS_NEUTRO   COLOR_RAMO_SIN_AREA
#define THEME_ACCENT        (Color){ 30, 144, 255, 255 }
#define COLOR_CELESTE       (Color){ 135, 206, 250, 255 }

#endif
