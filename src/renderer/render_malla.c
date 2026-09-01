#include "render_malla.h"

#include <math.h>
#include <string.h>

#include "malla.h"
#include "theme.h"

// Oscurece un color multiplicando canales RGB (mantiene alpha).
static Color ColorOscuro(Color c, float factor)
{
    return (Color){ (unsigned char)(c.r * factor),
                    (unsigned char)(c.g * factor),
                    (unsigned char)(c.b * factor), c.a };
}

// Devuelve un color de texto legible sobre el fondo dado.
static Color TextoContraste(Color bg)
{
    float lum = 0.299f * bg.r + 0.587f * bg.g + 0.114f * bg.b;
    return (lum > 140.0f) ? DARKGRAY : WHITE;
}

// Recorta un texto al ancho máximo aproximado (placeholder del render de US-14).
static void RecortarTexto(Font font, char *texto, int size, float maxW)
{
    int len = (int)strlen(texto);
    if (MeasureTextEx(font, texto, (float)size, 1.0f).x <= maxW)
        return;

    int maxChars = (int)(maxW / ((float)size * 0.5f));
    if (maxChars > 3)
    {
        texto[maxChars - 3] = '.';
        texto[maxChars - 2] = '.';
        texto[maxChars - 1] = '.';
        texto[maxChars] = '\0';
    }
    else if (maxChars >= 0)
    {
        texto[0] = '\0';
    }
    (void)len;
}

void DrawRamoNode(const Malla *m, const Ramo *ramo, bool seleccionado,
                  CameraView cam, Rectangle viewport, Font font)
{
    // US-07 RF-03/RF-04: el ramo se dibuja con el color de su área asignada;
    // sin área (areaId = -1) usa su color propio (gris neutro por defecto).
    const Area *area = (ramo->areaId != SIN_AREA_ID)
                           ? MallaFindAreaById(m, ramo->areaId)
                           : NULL;
    Color color = (area != NULL) ? area->color : ramo->color;

    // US-15: el nodo pertenece al mundo y escala con el zoom (tamaño de
    // NODO_ANCHO×NODO_ALTO en unidades de mundo → píxeles = ×cam.zoom).
    float ancho = NODO_ANCHO * cam.zoom;
    float alto = NODO_ALTO * cam.zoom;
    Vector2 c = CameraWorldToScreen(cam, viewport, ramo->posicion);
    Rectangle rec = { c.x - ancho * 0.5f, c.y - alto * 0.5f, ancho, alto };

    DrawRectangleRounded(rec, 0.2f, 8, color);
    DrawRectangleRoundedLines(rec, 0.2f, 8, ColorOscuro(color, 0.6f));

    // US-08/US-09: borde de acento para el ramo seleccionado.
    if (seleccionado)
    {
        float m = 4.0f * cam.zoom;
        DrawRectangleRoundedLines((Rectangle){ rec.x - m, rec.y - m,
                                                rec.width + m * 2.0f,
                                                rec.height + m * 2.0f },
                                  0.2f, 8, THEME_ACCENT);
    }

    Color texto = TextoContraste(color);
    float textSize = 12.0f * cam.zoom;
    float pad = 6.0f * cam.zoom;

    Vector2 sizeCod = MeasureTextEx(font, ramo->codigo, textSize, 1.0f);
    DrawTextEx(font, ramo->codigo,
               (Vector2){ c.x - sizeCod.x * 0.5f, rec.y + pad },
               textSize, 1.0f, texto);

    char nombre[RAMO_NOMBRE_MAX + 1];
    strncpy(nombre, ramo->nombre, sizeof(nombre) - 1);
    nombre[sizeof(nombre) - 1] = '\0';
    RecortarTexto(font, nombre, (int)textSize, rec.width - 12.0f * cam.zoom);
    Vector2 sizeNom = MeasureTextEx(font, nombre, textSize, 1.0f);
    DrawTextEx(font, nombre,
               (Vector2){ c.x - sizeNom.x * 0.5f,
                          rec.y + rec.height - pad - textSize },
               textSize, 1.0f, texto);
}

Rectangle RamoNodeScreenRect(const Ramo *r, CameraView cam, Rectangle viewport)
{
    Vector2 c = CameraWorldToScreen(cam, viewport, r->posicion);
    float ancho = NODO_ANCHO * cam.zoom;
    float alto = NODO_ALTO * cam.zoom;
    return (Rectangle){ c.x - ancho * 0.5f, c.y - alto * 0.5f, ancho, alto };
}

int FindRamoAt(const Malla *m, Vector2 world)
{
    // El nodo ocupa NODO_ANCHO×NODO_ALTO en unidades de mundo (US-15: escala
    // con el zoom en pantalla); el hit-test se hace en el mismo rectángulo de
    // mundo para coincidir con lo visual en todo el rango (RF-11).
    // Recorre en orden inverso: el último dibujado (aparece encima) gana.
    for (int i = m->nRamos - 1; i >= 0; i--)
    {
        const Ramo *r = &m->ramos[i];
        Rectangle rec = { r->posicion.x - NODO_ANCHO * 0.5f,
                          r->posicion.y - NODO_ALTO * 0.5f,
                          NODO_ANCHO, NODO_ALTO };
        if (CheckCollisionPointRec(world, rec))
            return i;
    }
    return -1;
}

// Punta de flecha en el punto 'fin' (pantalla), orientada hacia 'prev'
// (dirección del último segmento de la polilínea, US-21 RF-08). El espesor y
// el tamaño se escalan con el zoom para que la flecha pertenezca al mundo
// (US-15).
static void DrawPuntaFlecha(Vector2 fin, Vector2 prev, Color color, float ancho,
                            float zoom)
{
    Vector2 d = { fin.x - prev.x, fin.y - prev.y };
    float len = sqrtf(d.x * d.x + d.y * d.y);
    if (len < 1.0f)
        return;
    d.x /= len;
    d.y /= len;

    float w = ancho * zoom;
    const float tam = 10.0f * zoom;
    Vector2 base = { fin.x - d.x * tam, fin.y - d.y * tam };
    Vector2 perp = { -d.y, d.x };
    Vector2 a1 = { base.x + perp.x * tam * 0.5f, base.y + perp.y * tam * 0.5f };
    Vector2 a2 = { base.x - perp.x * tam * 0.5f, base.y - perp.y * tam * 0.5f };
    DrawLineEx(fin, a1, w, color);
    DrawLineEx(fin, a2, w, color);
}

void DrawRutaOrtogonal(const Ruta *ruta, CameraView cam, Rectangle viewport,
                       Color color, float ancho)
{
    if (ruta->n < 2)
        return;

    for (int i = 0; i + 1 < ruta->n; i++)
    {
        Vector2 a = CameraWorldToScreen(cam, viewport, ruta->pts[i]);
        Vector2 b = CameraWorldToScreen(cam, viewport, ruta->pts[i + 1]);
        DrawLineEx(a, b, ancho * cam.zoom, color);
    }

    // US-21 RF-08: la punta se dibuja en el punto de entrada del destino,
    // orientada según el último segmento de la ruta.
    Vector2 fin = CameraWorldToScreen(cam, viewport, ruta->pts[ruta->n - 1]);
    Vector2 prev = CameraWorldToScreen(cam, viewport, ruta->pts[ruta->n - 2]);
    DrawPuntaFlecha(fin, prev, color, ancho, cam.zoom);
}

// Dibuja un indicador de salto (arco visual) en la intersección de dos flechas.
// El arco se dibuja en el lado de la flecha que pasa "encima" de la otra.
static void DrawSaltoFlecha(Vector2 cruce_pantalla, int tipoA, Color color,
                            float ancho, float zoom)
{
    float radio = ancho * zoom * 2.5f;  // Radio del arco de salto

    if (tipoA == 0)
    {
        // Flecha A es horizontal → dibuja arco vertical (arriba)
        DrawCircleLines((int)cruce_pantalla.x, (int)cruce_pantalla.y,
                        (int)radio, color);
    }
    else
    {
        // Flecha A es vertical → dibuja arco horizontal (derecha)
        DrawCircleLines((int)cruce_pantalla.x, (int)cruce_pantalla.y,
                        (int)radio, color);
    }
}

// US-15 RF-13 / US-21: culling conservador de flechas. Si el rectángulo
// envolvente en pantalla de la polilínea no intersecta el viewport (con
// margen para punta y grosor), la flecha no se dibuja.
static bool RutaVisible(const Ruta *ruta, CameraView cam, Rectangle viewport)
{
    if (ruta->n < 2)
        return false;
    Vector2 p0 = CameraWorldToScreen(cam, viewport, ruta->pts[0]);
    float minX = p0.x, maxX = p0.x, minY = p0.y, maxY = p0.y;
    for (int i = 1; i < ruta->n; i++)
    {
        Vector2 p = CameraWorldToScreen(cam, viewport, ruta->pts[i]);
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }
    Rectangle margen = { viewport.x - 40.0f, viewport.y - 40.0f,
                         viewport.width + 80.0f, viewport.height + 80.0f };
    Rectangle bb = { minX, minY, maxX - minX, maxY - minY };
    return CheckCollisionRecs(bb, margen);
}

void DrawPrerrequisitoFlechas(const Malla *m, CameraView cam,
                              Rectangle viewport, int selOrigenId,
                              int selDestinoId)
{
    // US-21: flecha B->A para cada A con B en su lista de prerrequisitos; el
    // ramo prerrequisito apunta hacia el ramo que lo requiere. Las rutas
    // ortogonales se calculan en mundo y se transforman a pantalla (RF-14).
    FlechaRuta flechas[RUTA_MAX_FLECHAS];
    int nF = RutaFlechasDeMalla(m, flechas, RUTA_MAX_FLECHAS);

    for (int i = 0; i < nF; i++)
    {
        // Culling: solo se dibujan las flechas potencialmente visibles.
        if (!RutaVisible(&flechas[i].ruta, cam, viewport))
            continue;

        // US-13 RF-03: la flecha seleccionada se resalta (grosor + acento).
        if (flechas[i].destinoId == selDestinoId &&
            flechas[i].origenId == selOrigenId)
            DrawRutaOrtogonal(&flechas[i].ruta, cam, viewport, THEME_ACCENT,
                              4.0f);
        else
            DrawRutaOrtogonal(&flechas[i].ruta, cam, viewport,
                              (Color){ 110, 110, 120, 255 }, 2.0f);
    }

    // Detectar y dibujar indicadores de salto en las intersecciones de flechas
    // (cuando una flecha cruza ortogonalmente a otra).
    CruceFlecha cruces[64];
    for (int i = 0; i < nF; i++)
    {
        if (!RutaVisible(&flechas[i].ruta, cam, viewport))
            continue;

        for (int j = i + 1; j < nF; j++)
        {
            if (!RutaVisible(&flechas[j].ruta, cam, viewport))
                continue;

            int nCruces = RutaEncontrarCruces(&flechas[i].ruta, &flechas[j].ruta,
                                              cruces, 64);
            for (int c = 0; c < nCruces; c++)
            {
                Vector2 cruce_pantalla = CameraWorldToScreen(
                    cam, viewport, cruces[c].punto);
                Color colorCruce = (Color){ 110, 110, 120, 200 };
                DrawSaltoFlecha(cruce_pantalla, cruces[c].tipoA, colorCruce,
                                2.0f, cam.zoom);
            }
        }
    }
}

// Distancia del punto 'p' al segmento [a, b].
static float DistPuntoSegmento(Vector2 p, Vector2 a, Vector2 b)
{
    Vector2 ab = { b.x - a.x, b.y - a.y };
    Vector2 ap = { p.x - a.x, p.y - a.y };
    float len2 = ab.x * ab.x + ab.y * ab.y;
    float t = (len2 > 0.0f) ? (ap.x * ab.x + ap.y * ab.y) / len2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    else if (t > 1.0f) t = 1.0f;
    Vector2 proj = { a.x + ab.x * t, a.y + ab.y * t };
    float dx = p.x - proj.x, dy = p.y - proj.y;
    return sqrtf(dx * dx + dy * dy);
}

bool FindFlechaAt(const Malla *m, Vector2 screenPoint, CameraView cam,
                  Rectangle viewport, int *origenId, int *destinoId)
{
    // US-21 RF-10: el hit-test recorre cada segmento de la polilínea en
    // pantalla (≤ FLECHA_TOLERANCIA_PX); un vértice pertenece a ambos
    // segmentos y también selecciona la flecha (CE-08).
    FlechaRuta flechas[RUTA_MAX_FLECHAS];
    int nF = RutaFlechasDeMalla(m, flechas, RUTA_MAX_FLECHAS);

    for (int i = 0; i < nF; i++)
    {
        for (int s = 0; s + 1 < flechas[i].ruta.n; s++)
        {
            Vector2 a = CameraWorldToScreen(cam, viewport,
                                            flechas[i].ruta.pts[s]);
            Vector2 b = CameraWorldToScreen(cam, viewport,
                                            flechas[i].ruta.pts[s + 1]);
            if (DistPuntoSegmento(screenPoint, a, b) <= FLECHA_TOLERANCIA_PX)
            {
                *origenId = flechas[i].origenId;
                *destinoId = flechas[i].destinoId;
                return true;
            }
        }
    }
    return false;
}
