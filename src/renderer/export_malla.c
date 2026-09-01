#include "export_malla.h"

#include <math.h>
#include <stdio.h>

#include "render_malla.h"

// Tamaño del nodo de ramo (coincide con render_malla.c, US-14): se usa para
// el rectángulo envolvente del contenido.
#define NODO_ANCHO 150.0f
#define NODO_ALTO  54.0f

// US-18 RF-06: margen alrededor del contenido (unidades de mundo).
#define EXPORT_MARGEN 60.0f

// US-18 RF-07: factor de resolución alta (≥2× del tamaño calculado) para que
// el texto (nombre/código) salga legible en la imagen.
#define EXPORT_FACTOR 2.0f

// Tope de la dimensión mayor de la imagen: evita superar el límite de tamaño
// de textura de la GPU cuando los ramos están muy separados (CE-02).
#define EXPORT_MAX_DIM 8192

bool ExportMallaImagen(const Malla *m, Font font, const char *ruta,
                       char *error, int errorTam)
{
    // US-18 RF-06: rectángulo envolvente de todas las posiciones ± tamaño del
    // recuadro, con margen alrededor (CE-02: posiciones extremas incluídas).
    float minX, minY, maxX, maxY;
    if (m->nRamos > 0)
    {
        minX = maxX = m->ramos[0].posicion.x;
        minY = maxY = m->ramos[0].posicion.y;
        for (int i = 1; i < m->nRamos; i++)
        {
            float x = m->ramos[i].posicion.x;
            float y = m->ramos[i].posicion.y;
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    }
    else
    {
        // CE-01: malla vacía → área mínima de exportación centrada en el
        // origen, para que la imagen sea válida igualmente.
        minX = -300.0f; maxX = 300.0f;
        minY = -150.0f; maxY = 150.0f;
    }

    float contentW = (maxX - minX) + NODO_ANCHO + EXPORT_MARGEN * 2.0f;
    float contentH = (maxY - minY) + NODO_ALTO + EXPORT_MARGEN * 2.0f;
    float centroX = (minX + maxX) * 0.5f;
    float centroY = (minY + maxY) * 0.5f;

    // RF-07: escala alta fija; si el contenido es enorme se reduce para que la
    // mayor dimensión no supere EXPORT_MAX_DIM (todo sigue cabiendo, CE-02).
    float escala = EXPORT_FACTOR;
    int imgW = (int)ceilf(contentW * escala);
    int imgH = (int)ceilf(contentH * escala);
    int maxDim = (imgW > imgH) ? imgW : imgH;
    if (maxDim > EXPORT_MAX_DIM)
    {
        escala *= (float)EXPORT_MAX_DIM / (float)maxDim;
        imgW = (int)ceilf(contentW * escala);
        imgH = (int)ceilf(contentH * escala);
    }

    RenderTexture2D tex = LoadRenderTexture(imgW, imgH);
    if (tex.id == 0 || tex.texture.id == 0)
    {
        snprintf(error, errorTam,
                 "No se pudo reservar la memoria gr\xC3\xA1" "fica para exportar.");
        return false;
    }

    // La cámara apunta al centro del contenido; el zoom es el factor de
    // exportación. El render es de la malla completa, sin cámara de vista.
    Rectangle viewport = { 0.0f, 0.0f, (float)imgW, (float)imgH };
    CameraView cam;
    cam.pos = (Vector2){ centroX, centroY };
    cam.zoom = escala;

    BeginTextureMode(tex);
    ClearBackground(WHITE);   // RF-05: fondo blanco uniforme, sin grid.

    // RF-03/RF-04: flechas bajo los nodos, igual que en el lienzo (US-14), sin
    // flecha seleccionada.
    DrawPrerrequisitoFlechas(m, cam, viewport, SIN_AREA_ID, SIN_AREA_ID);

    for (int i = 0; i < m->nRamos; i++)
        DrawRamoNode(m, &m->ramos[i], false, cam, viewport, font);
    EndTextureMode();

    Image img = LoadImageFromTexture(tex.texture);
    UnloadRenderTexture(tex);
    if (img.data == NULL)
    {
        snprintf(error, errorTam, "No se pudo generar la imagen.");
        return false;
    }

    // El render texture está invertido verticalmente (convención OpenGL).
    ImageFlipVertical(&img);

    bool ok = ExportImage(img, ruta);
    UnloadImage(img);

    // RF-10: ruta inválida o escritura fallida → error; la malla queda intacta.
    if (!ok)
    {
        snprintf(error, errorTam,
                 "No se pudo escribir el archivo '%s' (revisa la ruta y los "
                 "permisos de escritura).", ruta);
    }
    return ok;
}
