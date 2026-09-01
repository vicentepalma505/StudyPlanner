#ifndef MALLA_JSON_H
#define MALLA_JSON_H

#include "malla.h"

// Serializa la malla a JSON legible (US-16 RF-05..RF-08) y la escribe de forma
// atómica (RF-09: temporal + renombrado) en 'ruta'. Devuelve true en éxito. En
// fallo (permisos, disco, ruta inválida) deja un mensaje en 'errMsg' (si no es
// NULL) y la malla en memoria queda intacta (RF-11). No persiste el estado de
// cámara (RF-13).
bool MallaGuardarArchivo(const Malla *m, const char *ruta, char *errMsg,
                         int errMsgSize);

// Lee un archivo .malla (JSON de US-16) y reconstruye la malla en *m (US-17
// RF-10..RF-13). Devuelve true en éxito; en fallo (archivo inexistente, JSON
// corrupto, versión no soportada, validación) deja un mensaje en 'errMsg' y la
// malla en *m queda intacta (RF-09). Validaciones: IDs únicos (RF-08),
// prerrequisitos con ID inexistente descartados (CE-08), areaId inexistente
// pasa a -1 (CE-09) y el contador de IDs se reanuda desde el máximo (RF-13).
bool MallaCargarArchivo(Malla *m, const char *ruta, char *errMsg,
                        int errMsgSize);

#endif
