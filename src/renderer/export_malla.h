#ifndef EXPORT_MALLA_H
#define EXPORT_MALLA_H

#include "raylib.h"

#include "malla.h"

// US-18: renderiza la malla completa fuera de pantalla (fondo blanco uniforme
// sin grid, sin UI ni selecciones, independiente de la cámara de vista) y la
// guarda en 'ruta'. El formato se deduce de la extensión (.png o .jpg; la
// extensión la completa el diálogo antes de llamar). Devuelve true en éxito;
// en fallo deja el mensaje en 'error'.
bool ExportMallaImagen(const Malla *m, Font font, const char *ruta,
                       char *error, int errorTam);

#endif
