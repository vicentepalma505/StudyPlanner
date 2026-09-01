#ifndef FONT_H
#define FONT_H

#include "raylib.h"

// Carga una fuente del sistema con codepoints UTF-8 para español.
// *owned = true si el llamante debe llamar UnloadFont(font) al salir.
Font LoadAppFont(bool *owned);

#endif
