#ifndef DIALOGS_H
#define DIALOGS_H

#include "app.h"

// Abre el modal "Crear área" (US-02), reseteando sus campos.
void DialogsOpenCrearArea(App *app);

// Abre el modal "Editar área" (US-04) para el área seleccionada.
void DialogsOpenEditarArea(App *app);

// Abre el flujo de confirmación de eliminación (US-05) del área seleccionada.
void DialogsOpenEliminarArea(App *app);

// Abre el modal "Crear ramo" (US-06), reseteando sus campos.
void DialogsOpenCrearRamo(App *app);

// Abre el modal "Editar ramo" (US-08) para el ramo seleccionado.
void DialogsOpenEditarRamo(App *app);

// Abre el flujo de confirmación de eliminación (US-09) del ramo seleccionado.
void DialogsOpenEliminarRamo(App *app);

// Abre el diálogo de guardado (US-16 RF-02): nombre y ubicación del archivo.
void DialogsOpenGuardar(App *app);

// Abre el diálogo de apertura (US-17 RF-04..RF-06): lista de archivos .malla
// del directorio de trabajo + ruta manual.
void DialogsOpenAbrir(App *app);

// Abre el diálogo de exportación (US-18 RF-02): ruta (por defecto el mismo
// directorio en que se guardan/abren las mallas, o el elegido con "Buscar…")
// y formato PNG/JPG.
void DialogsOpenExportar(App *app);

// Dibuja y procesa el modal activo (si lo hay) por encima de todo.
void DialogsDraw(App *app);

// US-23 CE-11: termina los diálogos nativos de "Buscar…" (Abrir malla y
// Exportar imagen) si siguen abiertos. Debe llamarse al cerrar la app (no
// deja procesos huérfanos).
void DialogsShutdown(App *app);

#endif
