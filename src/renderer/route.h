#ifndef ROUTE_H
#define ROUTE_H

#include "raylib.h"

#include "malla.h"

// Enrutado ortogonal de flechas de prerrequisito (US-21): polilíneas con
// segmentos horizontales/verticales y esquinas de 90° que rodean los
// recuadros de los ramos. Todo se calcula en coordenadas de mundo (RF-14);
// el zoom/pan solo transforma a pantalla al dibujar. Las rutas se derivan de
// las posiciones actuales, se descartan al terminar el fotograma y nunca se
// persisten (RF-06).

// Ruta ortogonal: lista de vértices (≥ 2) en coordenadas de mundo.
#define RUTA_MAX_PUNTOS 64
typedef struct {
    int n;
    Vector2 pts[RUTA_MAX_PUNTOS];
} Ruta;

// Máximo de flechas que se calculan en un fotograma (acota el buffer
// estático; con MAX_RAMOS = 100 nunca se debería alcanzar).
#define RUTA_MAX_FLECHAS 512

// Parámetros por defecto del enrutado (ajustables si la estética lo requiere).
#define RUTA_MARGEN_PX      16.0f  // margen de seguridad al rodear ramos (RF-05)
#define RUTA_SEPARACION_PX  8.0f   // separación mínima entre rutas paralelas (RF-13)
#define RUTA_CELDA_PX       24.0f  // tamaño de celda de la rejilla del A* (RF-07)

// Borde del recuadro del ramo por el que una flecha sale/entra (RF-03/RF-04).
typedef enum {
    RUTA_BORDE_DERECHA,
    RUTA_BORDE_IZQUIERDA,
    RUTA_BORDE_SUPERIOR,
    RUTA_BORDE_INFERIOR,
} RutaBorde;

// Conexión con su ruta ya calculada (resultado de RutaFlechasDeMalla).
typedef struct {
    Ruta ruta;
    int origenId;
    int destinoId;
} FlechaRuta;

// Bordes de salida (origen) y entrada (destino) según la posición relativa:
// preferencia horizontal (RF-03); si los recuadros se solapan en X, se usan
// los bordes superior/inferior (RF-04).
void RutaBordes(const Ramo *origen, const Ramo *destino,
                RutaBorde *salida, RutaBorde *entrada);

// Punto central del borde del recuadro del ramo (coordenadas de mundo).
Vector2 RutaPuntoBorde(const Ramo *r, RutaBorde borde);

// Desplaza 'base' (un punto ya sobre el borde de 'r') una distancia 'd' a lo
// largo de ese borde, manteniéndolo dentro del recuadro. Se usa para separar
// rutas paralelas (RF-13).
Vector2 RutaDesplazarBorde(const Ramo *r, RutaBorde borde, Vector2 base,
                           float d);

// Ruta ortogonal en mundo entre 'salida' y 'entrada' evitando los
// rectángulos de 'obs' (que ya deben venir inflados con RUTA_MARGEN_PX; el
// origen y el destino no se incluyen). Estrategia: ruta en L/Z sin colisión;
// si colisiona, A* sobre rejilla; si A* falla, ruta de respaldo en L/Z
// (RF-07). 'out->n' queda ≥ 2 o la ruta queda vacía.
void RutaCalcular(const Rectangle *obs, int nObs, Vector2 salida,
                  Vector2 entrada, Ruta *out);

// Calcula las rutas de todas las conexiones de la malla (RF-01..RF-07),
// aplicando la separación mínima entre rutas paralelas (RF-13). Devuelve el
// número de flechas con ruta válida (≥ 2 puntos). Buffer 'flechas' con
// 'maxFlechas' entradas.
int RutaFlechasDeMalla(const Malla *m, FlechaRuta *flechas, int maxFlechas);

// Vista previa ortogonal del modo conectar (RF-12): del borde de 'origen'
// hacia 'cursor' (coordenadas de mundo), evitando los demás ramos. El cursor
// no se trata como obstáculo.
void RutaVistaPrevia(const Malla *m, const Ramo *origen, Vector2 cursor,
                     Ruta *ruta);

// Detecta intersecciones ortogonales entre dos rutas. Devuelve un arreglo de
// puntos de cruce (en coordenadas de mundo) y el número de cruces. Se usa
// para dibujar indicadores de salto visuales.
typedef struct {
    Vector2 punto;
    int tipoA;  // 0=horizontal, 1=vertical (dirección del segmento en rutaA)
} CruceFlecha;

int RutaEncontrarCruces(const Ruta *rutaA, const Ruta *rutaB,
                        CruceFlecha *cruces, int maxCruces);

#endif
