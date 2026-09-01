#ifndef RENDER_MALLA_H
#define RENDER_MALLA_H

#include "raylib.h"

#include "camera.h"
#include "malla.h"
#include "route.h"

// Tamaño (unidades de mundo) del recuadro de cada nodo de ramo (US-15: en
// pantalla escala con el zoom).
#define NODO_ANCHO 150.0f
#define NODO_ALTO  54.0f

// Dibuja un nodo de ramo en el lienzo. El color del nodo es el del área
// asignada (US-07 RF-03) o el color propio si el ramo está sin área (RF-04).
// Si 'seleccionado' es true dibuja un borde de acento (US-08/US-09).
void DrawRamoNode(const Malla *m, const Ramo *ramo, bool seleccionado,
                  CameraView cam, Rectangle viewport, Font font);

// Índice del ramo cuyo nodo (rectángulo NODO_ANCHO×NODO_ALTO en coordenadas
// de mundo; escala con el zoom en pantalla) contiene el punto dado, o -1. El
// último dibujado (más arriba en el arreglo) tiene prioridad.
int FindRamoAt(const Malla *m, Vector2 world);

// Tolerancia (px, en pantalla) para el hit-test de selección de flechas
// (US-13 RNF-02), medida sobre los segmentos de la polilínea (US-21 RF-10).
#define FLECHA_TOLERANCIA_PX 10.0f

// Busca la flecha de prerrequisito (US-13 RF-01) cuya polilínea en pantalla
// pasa a ≤ FLECHA_TOLERANCIA_PX del punto dado (RF-10: cualquier segmento de
// la ruta, incluido un vértice). Devuelve true y deja los IDs en *origenId
// (prerrequisito) y *destinoId (ramo que lo requiere). El hit-test de ramos
// tiene prioridad (RF-01): se asume que el punto no está sobre un recuadro
// de ramo.
bool FindFlechaAt(const Malla *m, Vector2 screenPoint, CameraView cam,
                  Rectangle viewport, int *origenId, int *destinoId);

// Dibuja las flechas de prerrequisito (US-21): polilíneas ortogonales desde
// cada ramo prerrequisito hacia el ramo que lo requiere, rodeando los
// recuadros (route.c). Debe llamarse ANTES de dibujar los nodos para que las
// puntas queden bajo ellos. La flecha seleccionada (par origenId/destinoId,
// US-13 RF-03) se dibuja más gruesa y con acento.
void DrawPrerrequisitoFlechas(const Malla *m, CameraView cam,
                              Rectangle viewport, int selOrigenId,
                              int selDestinoId);

// Dibuja una ruta ortogonal (mundo) como polilínea + punta de flecha en el
// último vértice (orientada según el último segmento). El espesor 'ancho'
// está en unidades de mundo y escala con el zoom (US-15). Se usa para las
// flechas de prerrequisito y para la vista previa del modo conectar (US-21
// RF-12).
void DrawRutaOrtogonal(const Ruta *ruta, CameraView cam, Rectangle viewport,
                       Color color, float ancho);

// Rectángulo en pantalla del nodo del ramo (para resaltar el origen en modo
// conectar, US-12).
Rectangle RamoNodeScreenRect(const Ramo *r, CameraView cam, Rectangle viewport);

#endif
