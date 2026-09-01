#include "malla.h"

#include <string.h>

#include "validate.h"

void InitMalla(Malla *m)
{
    memset(m, 0, sizeof(*m));
    m->nAreas = 0;
    m->nRamos = 0;
    m->siguienteIdArea = 1;
    m->siguienteIdRamo = 1;
    m->modified = false;
    m->rutaArchivo[0] = '\0';
}

int MallaGetAreaCount(const Malla *m)
{
    return m->nAreas;
}

int MallaGetRamoCount(const Malla *m)
{
    return m->nRamos;
}

const Area *MallaGetArea(const Malla *m, int idx)
{
    if (idx < 0 || idx >= m->nAreas)
        return NULL;
    return &m->areas[idx];
}

const Area *MallaFindAreaById(const Malla *m, int id)
{
    for (int i = 0; i < m->nAreas; i++)
    {
        if (m->areas[i].id == id)
            return &m->areas[i];
    }
    return NULL;
}

int MallaFindAreaIdxByName(const Malla *m, const char *nombre)
{
    for (int i = 0; i < m->nAreas; i++)
    {
        if (StrIgualCaseInsensitive(m->areas[i].nombre, nombre))
            return i;
    }
    return -1;
}

int MallaAddArea(Malla *m, const char *nombre, Color color,
                 const char *descripcion)
{
    if (m->nAreas >= MAX_AREAS)
        return -1;

    Area *a = &m->areas[m->nAreas];
    a->id = m->siguienteIdArea++;
    strncpy(a->nombre, nombre, AREA_NOMBRE_MAX);
    a->nombre[AREA_NOMBRE_MAX] = '\0';
    a->color = color;
    strncpy(a->descripcion, descripcion != NULL ? descripcion : "",
            AREA_DESCRIPCION_MAX);
    a->descripcion[AREA_DESCRIPCION_MAX] = '\0';
    m->nAreas++;
    m->modified = true;
    return a->id;
}

bool MallaUpdateArea(Malla *m, int id, const char *nombre, Color color,
                     const char *descripcion)
{
    int idx = -1;
    for (int i = 0; i < m->nAreas; i++)
    {
        if (m->areas[i].id == id)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return false;

    Area *a = &m->areas[idx];
    bool cambiaColor = !(a->color.r == color.r && a->color.g == color.g &&
                         a->color.b == color.b && a->color.a == color.a);
    bool cambiaNombre = !StrIgualCaseInsensitive(a->nombre, nombre);
    bool cambiaDesc = strcmp(a->descripcion,
                             descripcion != NULL ? descripcion : "") != 0;

    // US-04 RF-10: confirmar sin cambios reales no marca la malla.
    if (!cambiaColor && !cambiaNombre && !cambiaDesc)
        return true;

    strncpy(a->nombre, nombre, AREA_NOMBRE_MAX);
    a->nombre[AREA_NOMBRE_MAX] = '\0';
    strncpy(a->descripcion, descripcion != NULL ? descripcion : "",
            AREA_DESCRIPCION_MAX);
    a->descripcion[AREA_DESCRIPCION_MAX] = '\0';
    a->color = color;

    // US-04 RF-06: los ramos asignados adoptan el nuevo color del área.
    if (cambiaColor)
    {
        for (int i = 0; i < m->nRamos; i++)
        {
            if (m->ramos[i].areaId == id)
                m->ramos[i].color = color;
        }
    }

    m->modified = true;
    return true;
}

const Ramo *MallaGetRamo(const Malla *m, int idx)
{
    if (idx < 0 || idx >= m->nRamos)
        return NULL;
    return &m->ramos[idx];
}

const Ramo *MallaFindRamoById(const Malla *m, int id)
{
    for (int i = 0; i < m->nRamos; i++)
    {
        if (m->ramos[i].id == id)
            return &m->ramos[i];
    }
    return NULL;
}

int MallaFindRamoIdxByName(const Malla *m, const char *nombre)
{
    for (int i = 0; i < m->nRamos; i++)
    {
        if (StrIgualCaseInsensitive(m->ramos[i].nombre, nombre))
            return i;
    }
    return -1;
}

int MallaFindRamoIdxByCodigo(const Malla *m, const char *codigo)
{
    for (int i = 0; i < m->nRamos; i++)
    {
        if (StrIgualCaseInsensitive(m->ramos[i].codigo, codigo))
            return i;
    }
    return -1;
}

int MallaAddRamo(Malla *m, const char *nombre, const char *codigo, int creditos,
                 int semestre, int anio, int horas, Color color, int areaId,
                 Vector2 posicion, const int *prerrequisitos, int nPrerrequisitos)
{
    if (m->nRamos >= MAX_RAMOS)
        return -1;

    Ramo *r = &m->ramos[m->nRamos];
    r->id = m->siguienteIdRamo++;
    strncpy(r->nombre, nombre, RAMO_NOMBRE_MAX);
    r->nombre[RAMO_NOMBRE_MAX] = '\0';
    strncpy(r->codigo, codigo, RAMO_CODIGO_MAX);
    r->codigo[RAMO_CODIGO_MAX] = '\0';
    r->creditos = creditos;
    r->semestre = semestre;
    r->anio = anio;
    r->horas = horas;
    r->color = color;
    r->areaId = areaId;
    r->posicion = posicion;

    // US-07 RF-03: al asignar un área, el color del ramo es el del área
    // (sobrescribe el color propio almacenado; coherente con US-04 RF-06).
    if (areaId != SIN_AREA_ID)
    {
        const Area *a = MallaFindAreaById(m, areaId);
        if (a != NULL)
            r->color = a->color;
    }

    r->nPrerrequisitos = (nPrerrequisitos > MAX_RAMOS) ? MAX_RAMOS : nPrerrequisitos;
    if (r->nPrerrequisitos > 0)
        memcpy(r->prerrequisitos, prerrequisitos,
               (size_t)r->nPrerrequisitos * sizeof(int));

    m->nRamos++;
    m->modified = true;
    return r->id;
}

int MallaCountRamosOfArea(const Malla *m, int areaId)
{
    int n = 0;
    for (int i = 0; i < m->nRamos; i++)
    {
        if (m->ramos[i].areaId == areaId)
            n++;
    }
    return n;
}

void MallaRemoveArea(Malla *m, int id)
{
    int idx = -1;
    for (int i = 0; i < m->nAreas; i++)
    {
        if (m->areas[i].id == id)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return;

    for (int i = idx; i < m->nAreas - 1; i++)
        m->areas[i] = m->areas[i + 1];
    m->nAreas--;

    // US-05 RF-04: los ramos del área quedan sin área y con color neutro;
    // conservan sus conexiones de prerrequisito (RF-07).
    for (int i = 0; i < m->nRamos; i++)
    {
        if (m->ramos[i].areaId == id)
        {
            m->ramos[i].areaId = SIN_AREA_ID;
            m->ramos[i].color = COLOR_RAMO_SIN_AREA;
        }
    }

    m->modified = true;   // el contador de IDs no retrocede (RF-06)
}

bool MallaIsModified(const Malla *m)
{
    return m->modified;
}

void MallaMarkModified(Malla *m)
{
    m->modified = true;
}

void MallaClearModified(Malla *m)
{
    m->modified = false;
}

void MallaSetRuta(Malla *m, const char *ruta)
{
    if (ruta != NULL) {
        strncpy(m->rutaArchivo, ruta, sizeof(m->rutaArchivo) - 1);
        m->rutaArchivo[sizeof(m->rutaArchivo) - 1] = '\0';
    } else {
        m->rutaArchivo[0] = '\0';
    }
}

const char *MallaGetRuta(const Malla *m)
{
    return m->rutaArchivo;
}

bool MallaUpdateRamo(Malla *m, int id, const char *nombre, const char *codigo,
                     int creditos, int semestre, int anio, int horas,
                     Color color, int areaId, const int *prerrequisitos,
                     int nPrerrequisitos)
{
    int idx = -1;
    for (int i = 0; i < m->nRamos; i++)
    {
        if (m->ramos[i].id == id)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return false;

    Ramo *r = &m->ramos[idx];

    // US-07 RF-03: al asignar/cambiar área, el color del ramo es el del área.
    if (areaId != SIN_AREA_ID)
    {
        const Area *a = MallaFindAreaById(m, areaId);
        if (a != NULL)
            color = a->color;
    }

    // US-08 RF-11: sin cambios reales no se marca la malla como modificada.
    bool cabiaNombre = !StrIgualCaseInsensitive(r->nombre, nombre);
    bool cabiaCodigo = !StrIgualCaseInsensitive(r->codigo, codigo);
    bool cabiaNumericos = (r->creditos != creditos || r->semestre != semestre ||
                           r->anio != anio || r->horas != horas);
    bool cabiaArea = (r->areaId != areaId);
    bool cabiaColor = !(r->color.r == color.r && r->color.g == color.g &&
                        r->color.b == color.b && r->color.a == color.a);
    bool cabiaPre = (r->nPrerrequisitos != nPrerrequisitos);
    if (!cabiaPre && nPrerrequisitos > 0)
    {
        int res = memcmp(r->prerrequisitos, prerrequisitos,
                         (size_t)nPrerrequisitos * sizeof(int));
        cabiaPre = (res != 0);
    }

    if (!cabiaNombre && !cabiaCodigo && !cabiaNumericos && !cabiaArea &&
        !cabiaColor && !cabiaPre)
        return true;   // sin cambios: no se marca modificada

    strncpy(r->nombre, nombre, RAMO_NOMBRE_MAX);
    r->nombre[RAMO_NOMBRE_MAX] = '\0';
    strncpy(r->codigo, codigo, RAMO_CODIGO_MAX);
    r->codigo[RAMO_CODIGO_MAX] = '\0';
    r->creditos = creditos;
    r->semestre = semestre;
    r->anio = anio;
    r->horas = horas;
    r->color = color;
    r->areaId = areaId;
    r->nPrerrequisitos = (nPrerrequisitos > MAX_RAMOS) ? MAX_RAMOS : nPrerrequisitos;
    if (r->nPrerrequisitos > 0)
        memcpy(r->prerrequisitos, prerrequisitos,
               (size_t)r->nPrerrequisitos * sizeof(int));

    m->modified = true;
    return true;
}

bool MallaCreaCiclo(const Malla *m, int idRamo, int candidatoId)
{
    if (idRamo == candidatoId)
        return true;

    // BFS por índices (los IDs no se pueden usar como índice: crecen sin
    // límite). Recorre los ramos que alcanzan a candidatoId; si encuentra a
    // idRamo, el nuevo arco candidatoId->idRamo cerraría un ciclo.
    bool visitado[MAX_RAMOS] = { false };
    int cola[MAX_RAMOS];
    int head = 0, tail = 0;

    int idxCand = -1;
    for (int i = 0; i < m->nRamos; i++)
    {
        if (m->ramos[i].id == candidatoId)
        {
            idxCand = i;
            break;
        }
    }
    if (idxCand < 0)
        return false;

    visitado[idxCand] = true;
    cola[tail++] = idxCand;

    while (head < tail)
    {
        int i = cola[head++];
        const Ramo *r = &m->ramos[i];
        for (int k = 0; k < r->nPrerrequisitos; k++)
        {
            int pid = r->prerrequisitos[k];
            if (pid == idRamo)
                return true;

            int j = -1;
            for (int q = 0; q < m->nRamos; q++)
            {
                if (m->ramos[q].id == pid)
                {
                    j = q;
                    break;
                }
            }
            if (j >= 0 && !visitado[j])
            {
                visitado[j] = true;
                if (tail < MAX_RAMOS)
                    cola[tail++] = j;
            }
        }
    }
    return false;
}

bool MallaAddPrerrequisito(Malla *m, int idRamo, int prereqId)
{
    if (idRamo == prereqId)
        return false;

    int idx = -1;
    for (int i = 0; i < m->nRamos; i++)
    {
        if (m->ramos[i].id == idRamo)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return false;

    Ramo *r = &m->ramos[idx];

    // Sin duplicados (US-12).
    for (int i = 0; i < r->nPrerrequisitos; i++)
    {
        if (r->prerrequisitos[i] == prereqId)
            return false;
    }

    if (r->nPrerrequisitos >= MAX_RAMOS)
        return false;

    // US-08 RF-06: no crear ciclos directos o indirectos.
    if (MallaCreaCiclo(m, idRamo, prereqId))
        return false;

    r->prerrequisitos[r->nPrerrequisitos++] = prereqId;
    m->modified = true;
    return true;
}

bool MallaRemovePrerrequisito(Malla *m, int idRamo, int prereqId)
{
    Ramo *r = NULL;
    for (int i = 0; i < m->nRamos; i++)
    {
        if (m->ramos[i].id == idRamo)
        {
            r = &m->ramos[i];
            break;
        }
    }
    if (r == NULL)
        return false;

    int idx = -1;
    for (int i = 0; i < r->nPrerrequisitos; i++)
    {
        if (r->prerrequisitos[i] == prereqId)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return false;   // la relación no existe (CE-08)

    for (int i = idx; i < r->nPrerrequisitos - 1; i++)
        r->prerrequisitos[i] = r->prerrequisitos[i + 1];
    r->nPrerrequisitos--;
    m->modified = true;
    return true;
}

bool MallaMoveRamo(Malla *m, int id, Vector2 posicion)
{
    Ramo *r = NULL;
    for (int i = 0; i < m->nRamos; i++)
    {
        if (m->ramos[i].id == id)
        {
            r = &m->ramos[i];
            break;
        }
    }
    if (r == NULL)
        return false;

    // CE-07: arrastre nulo, no se marca la malla como modificada.
    if (r->posicion.x == posicion.x && r->posicion.y == posicion.y)
        return true;

    r->posicion = posicion;
    m->modified = true;
    return true;
}

int MallaCountFlechasDeRamo(const Malla *m, int id)
{
    const Ramo *x = MallaFindRamoById(m, id);
    if (x == NULL)
        return 0;

    // Como destino: su propia lista de prerrequisitos (flechas salientes).
    int total = x->nPrerrequisitos;

    // Como origen: otros ramos que lo tienen de prerrequisito (flechas entrantes).
    for (int i = 0; i < m->nRamos; i++)
    {
        const Ramo *r = &m->ramos[i];
        if (r->id == id)
            continue;
        for (int k = 0; k < r->nPrerrequisitos; k++)
        {
            if (r->prerrequisitos[k] == id)
                total++;
        }
    }
    return total;
}

void MallaRemoveRamo(Malla *m, int id)
{
    int idx = -1;
    for (int i = 0; i < m->nRamos; i++)
    {
        if (m->ramos[i].id == id)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return;

    // US-09 RF-04: quitar el ID como origen de las listas de los demás ramos.
    for (int i = 0; i < m->nRamos; i++)
    {
        if (i == idx)
            continue;
        Ramo *r = &m->ramos[i];
        for (int k = 0; k < r->nPrerrequisitos; k++)
        {
            if (r->prerrequisitos[k] == id)
            {
                for (int q = k; q < r->nPrerrequisitos - 1; q++)
                    r->prerrequisitos[q] = r->prerrequisitos[q + 1];
                r->nPrerrequisitos--;
                k--;
            }
        }
    }

    // Descartar el ramo (su propia lista de prerrequisitos se pierde: RF-04
    // destino) y compactar el arreglo. El ID no se reutiliza (RF-07).
    for (int i = idx; i < m->nRamos - 1; i++)
        m->ramos[i] = m->ramos[i + 1];
    m->nRamos--;
    m->modified = true;
}
