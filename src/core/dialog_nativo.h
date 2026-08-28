#ifndef DIALOG_NATIVO_H
#define DIALOG_NATIVO_H

// Diálogo gráfico nativo de selección de archivos (US-23 RF-02..RF-07 y
// US-18): se lanza zenity (kdialog como respaldo, CE-03) en un proceso hijo y
// se consulta por fotograma sin bloquear el bucle. Solo hay una instancia a la
// vez (los diálogos "Abrir malla" y "Exportar imagen" nunca están abiertos
// simultáneamente).

// Modo del diálogo: elegir un archivo existente ("Abrir malla") o elegir la
// ruta de un archivo nuevo ("Exportar imagen", equivalente a "Guardar como").
typedef enum {
    DNATIVO_ABRIR = 0,
    DNATIVO_GUARDAR,
} DialogNativoModo;

typedef enum {
    DNATIVO_INACTIVO = 0,   // sin proceso o ya resuelto
    DNATIVO_ACTIVO,         // el diálogo sigue abierto (RF-07)
    DNATIVO_SELECCION,      // el usuario aceptó; 'resultado' tiene la ruta
    DNATIVO_CANCELADO,      // canceló sin elegir (RF-06)
    DNATIVO_NO_DISPONIBLE,  // ni zenity ni kdialog disponibles (RF-12)
} DialogNativoEstado;

typedef struct {
    DialogNativoEstado estado;
    char resultado[1024];   // ruta elegida (solo en DNATIVO_SELECCION)
} DialogNativo;

// Lanza el diálogo nativo en modo 'modo' abierto en la carpeta 'carpeta'
// (puede ser NULL o vacía → directorio actual, RF-03). En modo abrir filtra
// *.malla (RF-04); en modo guardar ofrece PNG/JPG y confirma antes de
// sobrescribir.
void DialogNativoIniciar(DialogNativo *d, DialogNativoModo modo,
                         const char *carpeta);

// Consulta el estado del hijo (no bloqueante, waitpid WNOHANG). Debe llamarse
// cada fotograma mientras 'estado == DNATIVO_ACTIVO'; la app sigue dibujando
// (RF-07/RNF-01).
void DialogNativoPoll(DialogNativo *d);

// Termina el proceso hijo si sigue vivo y lo espera (CE-11). Debe llamarse al
// cerrar el diálogo de abrir o la app (no deja procesos huérfanos).
void DialogNativoTerminar(DialogNativo *d);

#endif
