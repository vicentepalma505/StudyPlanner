#include "route.h"

#include <math.h>
#include <string.h>

#include "render_malla.h"

// ---------------------------------------------------------------------------
// Rejilla del A* (referencia US-21): celdas de RUTA_CELDA_PX de mundo,
// 4-conectividad, buffers estáticos reutilizados (sin alocación dinámica por
// fotograma, RNF-04). La ventana de búsqueda queda acotada por la malla de
// MAX_W × MAX_H celdas; si el caso excede el tamaño, el A* falla y se usa la
// ruta de respaldo (RF-07).
// ---------------------------------------------------------------------------
#define RUTA_GRID_MAX_W    180
#define RUTA_GRID_MAX_H    180
#define RUTA_GRID_MAX_CELDAS (RUTA_GRID_MAX_W * RUTA_GRID_MAX_H)

#define RUTA_EPS 0.001f

static bool  s_bloqueadas[RUTA_GRID_MAX_CELDAS];
static int   s_g[RUTA_GRID_MAX_CELDAS];       // -1 = no visitado
static int   s_padre[RUTA_GRID_MAX_CELDAS];
static int   s_heap[RUTA_GRID_MAX_CELDAS];
static int   s_heapPos[RUTA_GRID_MAX_CELDAS]; // posición en s_heap o -1
static int   s_heapTam;
static int   s_gw, s_gh;                       // dimensiones de la rejilla actual
static float s_wx, s_wy;                       // origen de la ventana (mundo)
static int   s_meta;                           // celda objetivo (heurística)

// Recuadro (mundo) de un ramo, centrado en su posición.
static Rectangle RectRamo(const Ramo *r)
{
    return (Rectangle){ r->posicion.x - NODO_ANCHO * 0.5f,
                        r->posicion.y - NODO_ALTO * 0.5f,
                        NODO_ANCHO, NODO_ALTO };
}

// True si el segmento horizontal (x0,y)-(x1,y) toca el rectángulo 'r'.
static bool SegHColisiona(float x0, float x1, float y, Rectangle r)
{
    if (y < r.y || y > r.y + r.height)
        return false;
    float a = fminf(x0, x1), b = fmaxf(x0, x1);
    return (a <= r.x + r.width && b >= r.x);
}

// True si el segmento vertical (y0,x)-(y1,x) toca el rectángulo 'r'.
static bool SegVColisiona(float y0, float y1, float x, Rectangle r)
{
    if (x < r.x || x > r.x + r.width)
        return false;
    float a = fminf(y0, y1), b = fmaxf(y0, y1);
    return (a <= r.y + r.height && b >= r.y);
}

// True si algún segmento de la polilínea 'pts' (ortogonal) colisiona con
// algún obstáculo de 'obs'.
static bool RutaColisiona(const Vector2 *pts, int n,
                          const Rectangle *obs, int nObs)
{
    for (int i = 0; i + 1 < n; i++)
    {
        Vector2 a = pts[i], b = pts[i + 1];
        bool horizontal = (fabsf(a.y - b.y) < RUTA_EPS);
        for (int k = 0; k < nObs; k++)
        {
            if (horizontal)
            {
                if (SegHColisiona(a.x, b.x, a.y, obs[k]))
                    return true;
            }
            else
            {
                if (SegVColisiona(a.y, b.y, a.x, obs[k]))
                    return true;
            }
        }
    }
    return false;
}

// Rectángulo 'rec' inflado uniformemente en 'm' (margen de seguridad RF-05).
static Rectangle RectInflado(Rectangle rec, float m)
{
    return (Rectangle){ rec.x - m, rec.y - m, rec.width + 2.0f * m,
                        rec.height + 2.0f * m };
}

// Agrega 'p' al final de la ruta manteniendo segmentos ortogonales: si el
// último vértice no comparte fila ni columna con 'p', inserta la esquina de
// 90°. También fusiona tramos colineales consecutivos.
static void RutaPushOrtogonal(Ruta *r, Vector2 p)
{
    if (r->n >= RUTA_MAX_PUNTOS)
        return;
    if (r->n == 0)
    {
        r->pts[r->n++] = p;
        return;
    }

    Vector2 q = r->pts[r->n - 1];
    if (fabsf(p.x - q.x) < RUTA_EPS && fabsf(p.y - q.y) < RUTA_EPS)
        return;

    if (fabsf(p.x - q.x) < RUTA_EPS || fabsf(p.y - q.y) < RUTA_EPS)
    {
        // Alineado: si el anterior también lo está en la misma línea, el
        // punto 'q' es una esquina intermedia innecesaria → se descarta.
        if (r->n >= 2)
        {
            Vector2 prev = r->pts[r->n - 2];
            if ((fabsf(prev.y - q.y) < RUTA_EPS &&
                 fabsf(q.y - p.y) < RUTA_EPS) ||
                (fabsf(prev.x - q.x) < RUTA_EPS &&
                 fabsf(q.x - p.x) < RUTA_EPS))
            {
                r->pts[r->n - 1] = p;
                return;
            }
        }
        r->pts[r->n++] = p;
        return;
    }

    // Esquina de 90°: (p.x, q.y) como vértice intermedio.
    if (r->n + 1 < RUTA_MAX_PUNTOS)
    {
        r->pts[r->n++] = (Vector2){ p.x, q.y };
        r->pts[r->n++] = p;
    }
}

void RutaBordes(const Ramo *origen, const Ramo *destino,
                RutaBorde *salida, RutaBorde *entrada)
{
    Rectangle o = RectRamo(origen);
    Rectangle d = RectRamo(destino);

    if (d.x >= o.x + o.width)
    {
        *salida = RUTA_BORDE_DERECHA;
        *entrada = RUTA_BORDE_IZQUIERDA;
    }
    else if (d.x + d.width <= o.x)
    {
        *salida = RUTA_BORDE_IZQUIERDA;
        *entrada = RUTA_BORDE_DERECHA;
    }
    else if (d.y >= o.y + o.height)
    {
        // Solapan en X: destino debajo → bordes inferior/superior (RF-04).
        *salida = RUTA_BORDE_INFERIOR;
        *entrada = RUTA_BORDE_SUPERIOR;
    }
    else
    {
        *salida = RUTA_BORDE_SUPERIOR;
        *entrada = RUTA_BORDE_INFERIOR;
    }
}

Vector2 RutaPuntoBorde(const Ramo *r, RutaBorde borde)
{
    Vector2 c = r->posicion;
    switch (borde)
    {
        case RUTA_BORDE_DERECHA:
            return (Vector2){ c.x + NODO_ANCHO * 0.5f, c.y };
        case RUTA_BORDE_IZQUIERDA:
            return (Vector2){ c.x - NODO_ANCHO * 0.5f, c.y };
        case RUTA_BORDE_SUPERIOR:
            return (Vector2){ c.x, c.y - NODO_ALTO * 0.5f };
        default:
            return (Vector2){ c.x, c.y + NODO_ALTO * 0.5f };
    }
}

Vector2 RutaDesplazarBorde(const Ramo *r, RutaBorde borde, Vector2 base,
                           float d)
{
    (void)r;
    float maxH = NODO_ALTO * 0.5f - 4.0f;
    float maxW = NODO_ANCHO * 0.5f - 4.0f;
    switch (borde)
    {
        case RUTA_BORDE_DERECHA:
        case RUTA_BORDE_IZQUIERDA:
            if (d > maxH) d = maxH;
            if (d < -maxH) d = -maxH;
            return (Vector2){ base.x, base.y + d };
        default:
            if (d > maxW) d = maxW;
            if (d < -maxW) d = -maxW;
            return (Vector2){ base.x + d, base.y };
    }
}

// Heurística Manhattan (consistente → A* óptimo) desde la celda a la meta.
static int Heuristica(int celda)
{
    int col = celda % s_gw, row = celda / s_gw;
    int mcol = s_meta % s_gw, mrow = s_meta / s_gw;
    int dx = col - mcol;
    if (dx < 0) dx = -dx;
    int dy = row - mrow;
    if (dy < 0) dy = -dy;
    return dx + dy;
}

static int FValor(int celda)
{
    return s_g[celda] + Heuristica(celda);
}

static void HeapSubir(int i)
{
    while (i > 0)
    {
        int padre = (i - 1) / 2;
        if (FValor(s_heap[padre]) <= FValor(s_heap[i]))
            break;
        int t = s_heap[i];
        s_heap[i] = s_heap[padre];
        s_heap[padre] = t;
        s_heapPos[s_heap[i]] = i;
        s_heapPos[s_heap[padre]] = padre;
        i = padre;
    }
}

static void HeapBajar(int i)
{
    int n = s_heapTam;
    for (;;)
    {
        int izq = 2 * i + 1, der = 2 * i + 2, m = i;
        if (izq < n && FValor(s_heap[izq]) < FValor(s_heap[m]))
            m = izq;
        if (der < n && FValor(s_heap[der]) < FValor(s_heap[m]))
            m = der;
        if (m == i)
            break;
        int t = s_heap[i];
        s_heap[i] = s_heap[m];
        s_heap[m] = t;
        s_heapPos[s_heap[i]] = i;
        s_heapPos[s_heap[m]] = m;
        i = m;
    }
}

static void HeapPush(int celda)
{
    if (s_heapTam >= RUTA_GRID_MAX_CELDAS)
        return;
    s_heap[s_heapTam] = celda;
    s_heapPos[celda] = s_heapTam;
    s_heapTam++;
    HeapSubir(s_heapTam - 1);
}

static int HeapPop(void)
{
    int c = s_heap[0];
    s_heapPos[c] = -1;
    s_heap[0] = s_heap[s_heapTam - 1];
    if (s_heapTam > 1)
        s_heapPos[s_heap[0]] = 0;
    s_heapTam--;
    if (s_heapTam > 0)
        HeapBajar(0);
    return c;
}

// A* con 4-conectividad entre 'salida' y 'entrada' (mundo) evitando 'obs'.
// Devuelve true y deja la ruta ortogonal en 'ruta' (con los extremos
// exactamente en salida/entrada) o false si no encuentra camino.
static bool AStar(const Rectangle *obs, int nObs, Vector2 salida,
                  Vector2 entrada, Ruta *ruta)
{
    float x0 = fminf(salida.x, entrada.x), x1 = fmaxf(salida.x, entrada.x);
    float y0 = fminf(salida.y, entrada.y), y1 = fmaxf(salida.y, entrada.y);
    float manh = (x1 - x0) + (y1 - y0);
    float margen = fmaxf(manh * 0.5f, RUTA_MARGEN_PX * 3.0f);
    x0 -= margen;
    x1 += margen;
    y0 -= margen;
    y1 += margen;

    // Amplía la ventana para incluir los obstáculos que la tocan.
    for (int k = 0; k < nObs; k++)
    {
        Rectangle r = obs[k];
        if (r.x < x0) x0 = r.x;
        if (r.x + r.width > x1) x1 = r.x + r.width;
        if (r.y < y0) y0 = r.y;
        if (r.y + r.height > y1) y1 = r.y + r.height;
    }

    int gw = (int)ceilf((x1 - x0) / RUTA_CELDA_PX);
    int gh = (int)ceilf((y1 - y0) / RUTA_CELDA_PX);
    if (gw > RUTA_GRID_MAX_W) gw = RUTA_GRID_MAX_W;
    if (gh > RUTA_GRID_MAX_H) gh = RUTA_GRID_MAX_H;
    if (gw < 1) gw = 1;
    if (gh < 1) gh = 1;

    s_gw = gw;
    s_gh = gh;
    s_wx = x0;
    s_wy = y0;

    int total = gw * gh;
    for (int i = 0; i < total; i++)
    {
        s_g[i] = -1;
        s_bloqueadas[i] = false;
    }

    // Celdas tocadas por obstáculos → bloqueadas.
    for (int k = 0; k < nObs; k++)
    {
        Rectangle r = obs[k];
        int c0 = (int)floorf((r.x - x0) / RUTA_CELDA_PX);
        int c1 = (int)floorf((r.x + r.width - x0) / RUTA_CELDA_PX);
        int r0 = (int)floorf((r.y - y0) / RUTA_CELDA_PX);
        int r1 = (int)floorf((r.y + r.height - y0) / RUTA_CELDA_PX);
        if (c0 < 0) c0 = 0;
        if (c1 >= gw) c1 = gw - 1;
        if (r0 < 0) r0 = 0;
        if (r1 >= gh) r1 = gh - 1;
        if (c0 > c1 || r0 > r1)
            continue;
        for (int i = c0; i <= c1; i++)
            for (int j = r0; j <= r1; j++)
                s_bloqueadas[j * gw + i] = true;
    }

    int si = (int)floorf((salida.x - x0) / RUTA_CELDA_PX);
    int sj = (int)floorf((salida.y - y0) / RUTA_CELDA_PX);
    int gi = (int)floorf((entrada.x - x0) / RUTA_CELDA_PX);
    int gj = (int)floorf((entrada.y - y0) / RUTA_CELDA_PX);
    if (si < 0) si = 0;
    if (si >= gw) si = gw - 1;
    if (sj < 0) sj = 0;
    if (sj >= gh) sj = gh - 1;
    if (gi < 0) gi = 0;
    if (gi >= gw) gi = gw - 1;
    if (gj < 0) gj = 0;
    if (gj >= gh) gj = gh - 1;

    int inicio = sj * gw + si;
    s_meta = gj * gw + gi;
    s_bloqueadas[inicio] = false;
    s_bloqueadas[s_meta] = false;

    s_heapTam = 0;
    for (int i = 0; i < total; i++)
        s_heapPos[i] = -1;

    s_g[inicio] = 0;
    s_padre[inicio] = -1;
    HeapPush(inicio);

    bool encontrado = false;
    while (s_heapTam > 0)
    {
        int c = HeapPop();
        if (c == s_meta)
        {
            encontrado = true;
            break;
        }

        int col = c % gw, row = c / gw;
        int vec[4][2] = { { col + 1, row }, { col - 1, row },
                          { col, row + 1 }, { col, row - 1 } };
        for (int v = 0; v < 4; v++)
        {
            int nc = vec[v][0], nr = vec[v][1];
            if (nc < 0 || nc >= gw || nr < 0 || nr >= gh)
                continue;
            int idx = nr * gw + nc;
            if (s_bloqueadas[idx])
                continue;
            int tent = s_g[c] + 1;
            if (s_g[idx] == -1 || tent < s_g[idx])
            {
                s_g[idx] = tent;
                s_padre[idx] = c;
                if (s_heapPos[idx] == -1)
                    HeapPush(idx);
                else
                    HeapSubir(s_heapPos[idx]);
            }
        }
    }

    if (!encontrado)
        return false;

    // Reconstrucción: celdas de la meta al inicio (orden inverso).
    int celdas[RUTA_GRID_MAX_CELDAS];
    int nCeldas = 0;
    for (int c = s_meta; c != -1; c = s_padre[c])
    {
        if (nCeldas < RUTA_GRID_MAX_CELDAS)
            celdas[nCeldas++] = c;
    }

    Ruta res = { 0 };
    RutaPushOrtogonal(&res, salida);
    for (int i = nCeldas - 1; i >= 0; i--)
    {
        int col = celdas[i] % gw, row = celdas[i] / gw;
        Vector2 p = { x0 + (col + 0.5f) * RUTA_CELDA_PX,
                      y0 + (row + 0.5f) * RUTA_CELDA_PX };
        RutaPushOrtogonal(&res, p);
    }
    RutaPushOrtogonal(&res, entrada);

    if (res.n >= 2)
    {
        *ruta = res;
        return true;
    }
    return false;
}

// Simplificar ruta eliminando vértices que pueden saltarse sin colisión.
// Intenta conexiones L entre puntos no consecutivos para reducir esquinas.
static void RutaSimplificar(const Rectangle *obs, int nObs, Ruta *ruta)
{
    if (ruta->n <= 2)
        return;

    Ruta res = { 0 };
    res.pts[res.n++] = ruta->pts[0];

    int i = 0;
    while (i < ruta->n - 1)
    {
        // Intentar saltar desde i al máximo j posible
        int mejor_j = i + 1;

        for (int j = ruta->n - 1; j > i + 1; j--)
        {
            Vector2 a = ruta->pts[i];
            Vector2 b = ruta->pts[j];

            // Intenta dos formas de conexión L: (a → (b.x, a.y) → b) o
            // (a → (a.x, b.y) → b)
            Vector2 cand1[3] = { a, { b.x, a.y }, b };
            Vector2 cand2[3] = { a, { a.x, b.y }, b };

            if (!RutaColisiona(cand1, 3, obs, nObs))
            {
                mejor_j = j;
                break;
            }
            if (!RutaColisiona(cand2, 3, obs, nObs))
            {
                mejor_j = j;
                break;
            }
        }

        // Agregar el punto intermedio (esquina) entre i y mejor_j
        if (mejor_j > i + 1)
        {
            Vector2 a = ruta->pts[i];
            Vector2 b = ruta->pts[mejor_j];

            // Elegir la forma L que no colisiona (or fallback a primera)
            Vector2 cand1[3] = { a, { b.x, a.y }, b };
            Vector2 cand2[3] = { a, { a.x, b.y }, b };

            if (!RutaColisiona(cand1, 3, obs, nObs))
            {
                if (res.n < RUTA_MAX_PUNTOS - 1)
                    res.pts[res.n++] = (Vector2){ b.x, a.y };
            }
            else if (!RutaColisiona(cand2, 3, obs, nObs))
            {
                if (res.n < RUTA_MAX_PUNTOS - 1)
                    res.pts[res.n++] = (Vector2){ a.x, b.y };
            }
        }

        if (res.n < RUTA_MAX_PUNTOS)
            res.pts[res.n++] = ruta->pts[mejor_j];
        i = mejor_j;
    }

    *ruta = res;
}

void RutaCalcular(const Rectangle *obs, int nObs, Vector2 salida,
                  Vector2 entrada, Ruta *out)
{
    out->n = 0;
    if (nObs < 0)
        nObs = 0;

    // Candidatas L/Z (RF-01/RF-07): dos en L (horizontal/vertical primero) y
    // múltiples Z que despegan con escalones de diferentes magnitudes hacia la
    // franja entre filas/columnas. Esto resuelve sin A* la mayoría de flechas
    // típicas de columnas de semestres, cuya línea horizontal caería en el
    // centro de la fila. Se prueban múltiples escalones (0.5x, 1x, 1.5x, 2x,
    // 3x) para maximizar la probabilidad de encontrar un camino directo.
    float paso = NODO_ALTO + RUTA_MARGEN_PX;

    // Array para candidatas: 2 L simples + 10 Z (5 escalones × 2 direcciones)
    typedef struct {
        Vector2 pts[4];
        int n;
    } Candidata;

    Candidata cand[12];
    int nCand = 0;

    // L simple: horizontal primero
    cand[nCand].pts[0] = salida;
    cand[nCand].pts[1] = (Vector2){ entrada.x, salida.y };
    cand[nCand].pts[2] = entrada;
    cand[nCand].n = 3;
    nCand++;

    // L simple: vertical primero
    cand[nCand].pts[0] = salida;
    cand[nCand].pts[1] = (Vector2){ salida.x, entrada.y };
    cand[nCand].pts[2] = entrada;
    cand[nCand].n = 3;
    nCand++;

    // Z con múltiples escalones: hacia arriba y hacia abajo
    float escalones[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f };
    for (int e = 0; e < 5; e++)
    {
        float esc = escalones[e] * paso;

        // Z hacia arriba
        cand[nCand].pts[0] = salida;
        cand[nCand].pts[1] = (Vector2){ salida.x, salida.y - esc };
        cand[nCand].pts[2] = (Vector2){ entrada.x, salida.y - esc };
        cand[nCand].pts[3] = entrada;
        cand[nCand].n = 4;
        nCand++;

        // Z hacia abajo
        cand[nCand].pts[0] = salida;
        cand[nCand].pts[1] = (Vector2){ salida.x, salida.y + esc };
        cand[nCand].pts[2] = (Vector2){ entrada.x, salida.y + esc };
        cand[nCand].pts[3] = entrada;
        cand[nCand].n = 4;
        nCand++;
    }

    for (int c = 0; c < nCand; c++)
    {
        if (!RutaColisiona(cand[c].pts, cand[c].n, obs, nObs))
        {
            // Se reconstruye con RutaPushOrtogonal para descartar vértices
            // duplicados o tramos colineales (p. ej. el caso vertical CE-02).
            Ruta res = { 0 };
            for (int i = 0; i < cand[c].n; i++)
                RutaPushOrtogonal(&res, cand[c].pts[i]);
            if (res.n >= 2)
            {
                *out = res;
                return;
            }
            break;
        }
    }

    // Sin camino directo libre → A* (RF-05).
    if (AStar(obs, nObs, salida, entrada, out))
    {
        // Simplificar la ruta A* para eliminar esquinas innecesarias
        RutaSimplificar(obs, nObs, out);
        return;
    }

    // Ruta de respaldo en L/Z sin garantía de evitar colisiones (RF-07).
    out->n = 0;
    RutaPushOrtogonal(out, cand[0].pts[0]);
    RutaPushOrtogonal(out, cand[0].pts[1]);
    RutaPushOrtogonal(out, cand[0].pts[2]);
}

void RutaVistaPrevia(const Malla *m, const Ramo *origen, Vector2 cursor,
                     Ruta *ruta)
{
    ruta->n = 0;
    Rectangle o = RectRamo(origen);

    RutaBorde salida;
    if (cursor.x >= o.x + o.width)
        salida = RUTA_BORDE_DERECHA;
    else if (cursor.x <= o.x)
        salida = RUTA_BORDE_IZQUIERDA;
    else if (cursor.y > o.y + o.height)
        salida = RUTA_BORDE_INFERIOR;
    else
        salida = RUTA_BORDE_SUPERIOR;

    Vector2 s = RutaPuntoBorde(origen, salida);

    Rectangle obs[MAX_RAMOS];
    int nObs = 0;
    for (int i = 0; i < m->nRamos; i++)
    {
        if (m->ramos[i].id == origen->id)
            continue;
        if (nObs < MAX_RAMOS)
            obs[nObs++] = RectInflado(RectRamo(&m->ramos[i]), RUTA_MARGEN_PX);
    }

    RutaCalcular(obs, nObs, s, cursor, ruta);
}

// Detecta intersecciones ortogonales entre dos rutas. Una intersección ocurre
// cuando un segmento horizontal de una ruta cruza un segmento vertical de la
// otra (o viceversa). No considera intersecciones en vértices (cruces que
// terminan exactamente donde comienza otro segmento).
int RutaEncontrarCruces(const Ruta *rutaA, const Ruta *rutaB,
                        CruceFlecha *cruces, int maxCruces)
{
    if (maxCruces <= 0 || !rutaA || !rutaB || rutaA->n < 2 || rutaB->n < 2)
        return 0;

    int nCruces = 0;
    const float EPS = RUTA_EPS;

    // Comparar cada segmento de rutaA contra cada segmento de rutaB
    for (int i = 0; i + 1 < rutaA->n && nCruces < maxCruces; i++)
    {
        Vector2 a1 = rutaA->pts[i];
        Vector2 a2 = rutaA->pts[i + 1];

        // Determinar si segmento A es horizontal o vertical
        bool aHorizontal = (fabsf(a1.y - a2.y) < EPS);

        for (int j = 0; j + 1 < rutaB->n && nCruces < maxCruces; j++)
        {
            Vector2 b1 = rutaB->pts[j];
            Vector2 b2 = rutaB->pts[j + 1];

            // Determinar si segmento B es horizontal o vertical
            bool bHorizontal = (fabsf(b1.y - b2.y) < EPS);

            // Solo interesa cruce si un segmento es horizontal y otro vertical
            if (aHorizontal == bHorizontal)
                continue;

            if (aHorizontal)
            {
                // a es horizontal, b es vertical
                float ay = a1.y;
                float ax0 = fminf(a1.x, a2.x);
                float ax1 = fmaxf(a1.x, a2.x);

                float bx = b1.x;
                float by0 = fminf(b1.y, b2.y);
                float by1 = fmaxf(b1.y, b2.y);

                // Comprobar si se cruzan (sin contar vértices)
                if (fabsf(ay - b1.y) < EPS || fabsf(ay - b2.y) < EPS)
                    continue;  // Coincide con vértice
                if (fabsf(bx - a1.x) < EPS || fabsf(bx - a2.x) < EPS)
                    continue;  // Coincide con vértice

                if (ay > by0 && ay < by1 && bx > ax0 && bx < ax1)
                {
                    if (nCruces < maxCruces)
                    {
                        cruces[nCruces].punto = (Vector2){ bx, ay };
                        cruces[nCruces].tipoA = 0;  // a es horizontal
                        nCruces++;
                    }
                }
            }
            else
            {
                // a es vertical, b es horizontal
                float ax = a1.x;
                float ay0 = fminf(a1.y, a2.y);
                float ay1 = fmaxf(a1.y, a2.y);

                float by = b1.y;
                float bx0 = fminf(b1.x, b2.x);
                float bx1 = fmaxf(b1.x, b2.x);

                // Comprobar si se cruzan (sin contar vértices)
                if (fabsf(by - b1.y) < EPS || fabsf(by - b2.y) < EPS)
                    continue;  // Coincide con vértice
                if (fabsf(ax - a1.x) < EPS || fabsf(ax - a2.x) < EPS)
                    continue;  // Coincide con vértice

                if (by > ay0 && by < ay1 && ax > bx0 && ax < bx1)
                {
                    if (nCruces < maxCruces)
                    {
                        cruces[nCruces].punto = (Vector2){ ax, by };
                        cruces[nCruces].tipoA = 1;  // a es vertical
                        nCruces++;
                    }
                }
            }
        }
    }

    return nCruces;
}

// Caché de rutas por fotograma (RNF-01; la sección 11 de la spec lo sugiere
// si se detecta degradación): las rutas se reutilizan mientras la malla no
// cambie (las posiciones y el grafo de prerrequisitos son la clave). Cualquier
// cambio de posición invalida el caché y recalcula en el siguiente fotograma
// (RF-06). Buffers estáticos reutilizados: sin alocación dinámica (RNF-04).
static Malla s_cacheMalla;
static FlechaRuta s_cacheRutas[RUTA_MAX_FLECHAS];
static int s_cacheN = -1; // -1 = caché vacío

int RutaFlechasDeMalla(const Malla *m, FlechaRuta *flechas, int maxFlechas)
{
    if (maxFlechas <= 0)
        return 0;

    if (s_cacheN >= 0 && memcmp(&s_cacheMalla, m, sizeof(Malla)) == 0)
    {
        int n = (s_cacheN < maxFlechas) ? s_cacheN : maxFlechas;
        memcpy(flechas, s_cacheRutas, (size_t)n * sizeof(FlechaRuta));
        return n;
    }

    s_cacheMalla = *m;

    typedef struct {
        int origenId;
        int destinoId;
        RutaBorde bs;
        RutaBorde be;
        Vector2 ps;
        Vector2 pe;
        int kSal, nSal;
        int kEnt, nEnt;
    } ConexionTmp;

    if (maxFlechas <= 0)
        return 0;

    ConexionTmp tmp[RUTA_MAX_FLECHAS];
    int nTmp = 0;

    // Pasada 1: recolectar conexiones y puntos base de salida/entrada.
    for (int i = 0; i < m->nRamos && nTmp < RUTA_MAX_FLECHAS; i++)
    {
        const Ramo *a = &m->ramos[i];
        for (int k = 0; k < a->nPrerrequisitos && nTmp < RUTA_MAX_FLECHAS;
             k++)
        {
            const Ramo *b = MallaFindRamoById(m, a->prerrequisitos[k]);
            if (b == NULL)
                continue;
            RutaBordes(b, a, &tmp[nTmp].bs, &tmp[nTmp].be);
            tmp[nTmp].origenId = b->id;
            tmp[nTmp].destinoId = a->id;
            tmp[nTmp].ps = RutaPuntoBorde(b, tmp[nTmp].bs);
            tmp[nTmp].pe = RutaPuntoBorde(a, tmp[nTmp].be);
            tmp[nTmp].kSal = 0;
            tmp[nTmp].nSal = 0;
            tmp[nTmp].kEnt = 0;
            tmp[nTmp].nEnt = 0;
            nTmp++;
        }
    }

    // RF-13: contar cuántas flechas comparten cada (ramo, borde) para
    // distribuirlas en paralelo y evitar superposiciones.
    for (int i = 0; i < nTmp; i++)
    {
        for (int j = 0; j < nTmp; j++)
        {
            if (tmp[j].origenId == tmp[i].origenId && tmp[j].bs == tmp[i].bs)
            {
                if (j < i)
                    tmp[i].kSal++;
                tmp[i].nSal++;
            }
            if (tmp[j].destinoId == tmp[i].destinoId && tmp[j].be == tmp[i].be)
            {
                if (j < i)
                    tmp[i].kEnt++;
                tmp[i].nEnt++;
            }
        }
    }

    int nF = 0;
    Rectangle obs[MAX_RAMOS];

    // Pasada 2: desplazar puntos y calcular cada ruta.
    for (int i = 0; i < nTmp && nF < maxFlechas; i++)
    {
        const Ramo *origen = MallaFindRamoById(m, tmp[i].origenId);
        const Ramo *destino = MallaFindRamoById(m, tmp[i].destinoId);
        if (origen == NULL || destino == NULL)
            continue;

        float dSal = (tmp[i].kSal - (tmp[i].nSal - 1) * 0.5f)
                     * RUTA_SEPARACION_PX;
        float dEnt = (tmp[i].kEnt - (tmp[i].nEnt - 1) * 0.5f)
                     * RUTA_SEPARACION_PX;
        Vector2 ps = RutaDesplazarBorde(origen, tmp[i].bs, tmp[i].ps, dSal);
        Vector2 pe = RutaDesplazarBorde(destino, tmp[i].be, tmp[i].pe, dEnt);

        // Obstáculos: recuadros de los demás ramos, inflados (RF-05).
        int nObs = 0;
        for (int j = 0; j < m->nRamos && nObs < MAX_RAMOS; j++)
        {
            const Ramo *r = &m->ramos[j];
            if (r->id == tmp[i].origenId || r->id == tmp[i].destinoId)
                continue;
            obs[nObs++] = RectInflado(RectRamo(r), RUTA_MARGEN_PX);
        }

        Ruta ruta;
        RutaCalcular(obs, nObs, ps, pe, &ruta);
        if (ruta.n >= 2)
        {
            flechas[nF].ruta = ruta;
            flechas[nF].origenId = tmp[i].origenId;
            flechas[nF].destinoId = tmp[i].destinoId;
            nF++;
        }
    }

    s_cacheN = nF;
    memcpy(s_cacheRutas, flechas, (size_t)nF * sizeof(FlechaRuta));
    return nF;
}
