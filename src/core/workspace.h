#ifndef WORKSPACE_H
#define WORKSPACE_H

// Directorios por defecto persistidos en ~/.config/visor_malla/ (RNF-02):
//  - workspace.cfg: espacio de trabajo (US-23 RF-08/RF-09), la carpeta del
//    último archivo de malla elegido con "Buscar…".
//  - export.cfg: carpeta de exportación de imágenes (US-18), la elegida con
//    "Buscar…" en el diálogo de exportar; si no se eligió nunca, la app usa el
//    espacio de trabajo como directorio por defecto.
#define WORKSPACE_TAM 1024

// Carga el espacio de trabajo guardado en 'buf'. Si no hay configuración o
// falla la lectura, deja 'buf' vacío y la app usa el directorio actual
// (RF-10/CE-08).
void WorkspaceCargar(char *buf, int tam);

// Persiste 'ruta' (una carpeta) como espacio de trabajo (RF-08/RF-09). Si no
// se puede escribir la app sigue funcionando; solo no recordará la carpeta
// (RNF-02).
void WorkspaceGuardar(const char *ruta);

// Carga la carpeta por defecto de exportación de imágenes en 'buf' (US-18).
// Sin configuración deja 'buf' vacío (se usa el espacio de trabajo o el
// directorio actual).
void WorkspaceCargarExportDir(char *buf, int tam);

// Persiste 'ruta' (una carpeta) como carpeta por defecto de exportación de
// imágenes (US-18): sobrevive al cierre de la app. Fallo de escritura = la app
// continúa y solo no recuerda la carpeta (RNF-02).
void WorkspaceGuardarExportDir(const char *ruta);

// Extrae en 'dir' la carpeta que contiene 'archivo' (RF-08): elimina el último
// componente tras el último '/'. Sin '/' devuelve ".".
void WorkspaceDirDeArchivo(const char *archivo, char *dir, int tam);

#endif
