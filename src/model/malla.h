#ifndef MALLA_H
#define MALLA_H

#include "types.h"

// Inicializa (o reinicia) una malla vacía: 0 áreas, 0 ramos, sin guardar.
void InitMalla(Malla *m);

int MallaGetAreaCount(const Malla *m);
int MallaGetRamoCount(const Malla *m);

// Área en el índice dado (NULL si el índice está fuera de rango).
const Area *MallaGetArea(const Malla *m, int idx);

// Área por ID (NULL si no existe).
const Area *MallaFindAreaById(const Malla *m, int id);

// Ramo en el índice dado (NULL si el índice está fuera de rango).
const Ramo *MallaGetRamo(const Malla *m, int idx);

// Ramo por ID (NULL si no existe).
const Ramo *MallaFindRamoById(const Malla *m, int id);

// Índice del ramo cuyo nombre coincide sin distinguir mayúsculas, o -1.
int MallaFindRamoIdxByName(const Malla *m, const char *nombre);

// Índice del ramo cuyo código coincide sin distinguir mayúsculas, o -1.
int MallaFindRamoIdxByCodigo(const Malla *m, const char *codigo);

// Crea un ramo (US-06 RF-09): ID autoincremental, prerrequisitos por ID de
// ramo. Si el área no es SIN_AREA_ID, el color almacenado es el del área
// (US-07 RF-03). Devuelve el ID nuevo, o -1 si la lista está llena.
int MallaAddRamo(Malla *m, const char *nombre, const char *codigo, int creditos,
                 int semestre, int anio, int horas, Color color, int areaId,
                 Vector2 posicion, const int *prerrequisitos, int nPrerrequisitos);

// Aplica los cambios a un ramo (US-08). El color se sobrescribe con el del
// área si esta cambia/no es SIN_AREA_ID (US-07 RF-03). La posición no se
// modifica (RF-12). No marca la malla como modificada si no hubo cambios
// reales (RF-11). Devuelve false si el ID no existe.
bool MallaUpdateRamo(Malla *m, int id, const char *nombre, const char *codigo,
                     int creditos, int semestre, int anio, int horas,
                     Color color, int areaId, const int *prerrequisitos,
                     int nPrerrequisitos);

// True si agregar 'candidatoId' como prerrequisito de 'idRamo' generaría un
// ciclo directo o indirecto en el grafo de prerrequisitos (US-08 RF-06).
// BFS sobre el grafo actual (que aún no contiene el nuevo arco).
bool MallaCreaCiclo(const Malla *m, int idRamo, int candidatoId);

// Agrega 'prereqId' como prerrequisito de 'idRamo' (US-12). Rechaza el propio
// ramo, los duplicados y las relaciones que crearían ciclos. Marca la malla
// como modificada y devuelve true solo si la relación se agregó.
bool MallaAddPrerrequisito(Malla *m, int idRamo, int prereqId);

// Elimina la conexión 'prereqId' → 'idRamo' (US-13 RF-06): quita el ID de la
// lista de prerrequisitos del destino y marca la malla como modificada. No
// altera a los ramos involucrados. Devuelve false si el destino o la relación
// no existen (un segundo Supr no hace nada, CE-08).
bool MallaRemovePrerrequisito(Malla *m, int idRamo, int prereqId);

// Reposiciona el ramo (US-10 RF-02/RF-04): actualiza ramo.posicion (única
// fuente de verdad para dibujar) y marca la malla como modificada solo si la
// posición cambió (RF-07; un arrastre nulo no modifica nada, CE-07).
// Devuelve false si el ID no existe.
bool MallaMoveRamo(Malla *m, int id, Vector2 posicion);

// Cantidad de flechas de prerrequisito asociadas al ramo: las que tiene como
// destino (su propia lista) más las que otros ramos tienen hacia él (US-09
// RF-03).
int MallaCountFlechasDeRamo(const Malla *m, int id);

// Elimina el ramo (US-09 RF-04): sus referencias como origen (aparece en la
// lista de otros ramos) y como destino (su propia lista) se descartan. El ID
// no se reutiliza (RF-07). Marca la malla como modificada.
void MallaRemoveRamo(Malla *m, int id);

// Índice del área cuyo nombre coincide sin distinguir mayúsculas, o -1.
int MallaFindAreaIdxByName(const Malla *m, const char *nombre);

// Agrega un área al final de la lista con ID autoincremental y marca la malla
// como modificada. Devuelve el ID nuevo, o -1 si la lista está llena.
int MallaAddArea(Malla *m, const char *nombre, Color color,
                 const char *descripcion);

// Aplica los cambios a un área (US-04). Solo marca la malla como modificada si
// hubo cambios reales (RF-10). Si el color cambió, actualiza el color de los
// ramos asignados (RF-06). Devuelve false si el ID no existe.
bool MallaUpdateArea(Malla *m, int id, const char *nombre, Color color,
                     const char *descripcion);

// Cantidad de ramos asignados al área (US-05 RF-03).
int MallaCountRamosOfArea(const Malla *m, int areaId);

// Elimina el área (US-05 RF-04): desasigna sus ramos (sin área y con color
// neutro) y marca la malla como modificada. El ID no se reutiliza (RF-06).
void MallaRemoveArea(Malla *m, int id);

bool MallaIsModified(const Malla *m);
void MallaMarkModified(Malla *m);
void MallaClearModified(Malla *m);

void MallaSetRuta(Malla *m, const char *ruta);
const char *MallaGetRuta(const Malla *m);

#endif
