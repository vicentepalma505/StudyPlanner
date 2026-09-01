#ifndef APP_H
#define APP_H

#include "raylib.h"
#include "raygui.h"

#include "malla.h"
#include "camera.h"
#include "workspace.h"

typedef enum {
    SCREEN_START = 0,
    SCREEN_CANVAS,
} AppScreen;

// Acción pendiente de la confirmación de 3 opciones (US-19 RF-04..RF-07):
// SALIR (cierre de ventana), ABRIR (US-17) o CREAR_NUEVA (US-01). Solo se
// ejecuta si la confirmación lo autoriza.
typedef enum {
    ACCION_NINGUNA = 0,
    ACCION_SALIR,
    ACCION_ABRIR,
    ACCION_CREAR_NUEVA,
} AppAccion;

typedef enum {
    MODAL_NONE = 0,
    MODAL_CREAR_AREA,              // US-02
    MODAL_EDITAR_AREA,             // US-04
    MODAL_CONFIRMAR_ELIMINAR,      // US-05 RF-02
    MODAL_CONFIRMAR_ELIMINAR_RAMOS,// US-05 RF-03
    MODAL_CREAR_RAMO,              // US-06
    MODAL_EDITAR_RAMO,             // US-08
    MODAL_CONFIRMAR_ELIMINAR_RAMO, // US-09 RF-03
    MODAL_GUARDAR,                 // US-16 RF-02 (diálogo de nombre/ubicación)
    MODAL_ABRIR,                   // US-17 RF-04/RF-05 (lista + ruta manual)
    MODAL_EXPORTAR,                // US-18 RF-02 (nombre/ruta + formato PNG/JPG)
} Modal;

typedef struct {
    Malla malla;
    AppScreen screen;
    CameraView camara;

    Font font;
    bool fontOwned;

    int areaSeleccionadaId;
    int ramoSeleccionadoId;

    // Selección múltiple: lista de IDs de ramos seleccionados y contador.
    // ramoSeleccionados[i] contiene el ID del i-ésimo ramo seleccionado.
    // Se mantiene ramoSeleccionadoId para compatibilidad con lógica existente
    // (indica el ramo "principal" o el último seleccionado).
    int ramoSeleccionados[MAX_RAMOS];
    int nRamosSeleccionados;

    // Rectángulo selector (caja de selección con mouse): se activa con click
    // en el vacío del lienzo. Al soltar, selecciona todos los ramos dentro.
    bool rectSelectorActivo;
    Vector2 rectSelectorInicio;
    Vector2 rectSelectorActual;

    bool modoConectar;
    int origenConectarId;

    // Selección de flecha de prerrequisito (US-13 RF-02): par (origenId,
    // destinoId). Ambos SIN_AREA_ID si no hay flecha seleccionada.
    int conexionSelOrigenId;
    int conexionSelDestinoId;

    // Arrastre de ramos (US-10 RF-02/RF-03): ramo candidato, pulsación en
    // pantalla y offset mundo ramo-cursor (para que el ramo no "salte").
    // Para arrastre múltiple, se guarda la posición anterior para calcular delta.
    bool arrastrando;
    int arrastreRamoId;
    Vector2 arrastrePress;
    Vector2 arrastreOffset;
    Vector2 arrastredeltaAnterior;  // Posición anterior para cálculo de delta en arrastre múltiple

    // Aviso transitorio sobre el lienzo (US-12): texto y segundos restantes.
    char aviso[160];
    double avisoRestante;

    // Pan del lienzo (US-15 RF-05): arrastre con el botón central. Guarda la
    // pulsación en pantalla y la posición de cámara inicial.
    bool panActivo;
    Vector2 panPress;
    Vector2 panPosInicial;

    // US-19 RF-04..RF-07: confirmación de 3 opciones reutilizable para
    // SALIR/ABRIR/CREAR_NUEVA cuando la malla tiene cambios sin guardar.
    AppAccion accionPendiente;
    bool dialogConfirmar;

    // US-19 RF-08/RF-11: si "Guardar" abre el diálogo de US-16 (malla nueva),
    // al completar el guardado con éxito se ejecuta la acción pendiente; si el
    // guardado no completa, la acción se cancela.
    bool continuarTrasGuardar;

    // US-19 RF-04: cierre de ventana diferido; solo se pone a true cuando la
    // confirmación (o la ausencia de cambios) lo autoriza.
    bool cerrarAprobado;

    // US-16 RF-11: error de guardado mostrado con GuiMessageBox.
    bool dialogErrorGuardar;
    char errorGuardar[256];

    // US-17 RF-09: error de apertura mostrado con GuiMessageBox; la malla
    // actual queda intacta.
    bool dialogErrorAbrir;
    char errorAbrir[256];

    // US-18 RF-10: error de exportación mostrado con GuiMessageBox; la malla
    // actual queda intacta.
    bool dialogErrorExportar;
    char errorExportar[256];

    Modal modal;            // modal abierto (bloqueante), MODAL_NONE si no hay
    Vector2 panelScroll;    // offset de scroll del panel de áreas (US-03)

    // US-23 RF-08/RF-09: espacio de trabajo (carpeta del último archivo
    // elegido con "Buscar…"). Vacío = no definido (se usa el directorio
    // actual, RF-10/CE-08). Se carga en AppInit y se persiste con
    // WorkspaceGuardar al elegir un archivo con "Buscar…".
    char espacioTrabajo[WORKSPACE_TAM];

    // US-18 (agregado): carpeta por defecto de exportación de imágenes
    // (elegida con "Buscar…" en el diálogo de exportar, persistida en
    // export.cfg). Vacío = se usa el espacio de trabajo de las mallas (o el
    // directorio actual si tampoco hay).
    char espacioExportar[WORKSPACE_TAM];
} App;

void AppInit(App *app);
void AppShutdown(App *app);
void AppUpdate(App *app);
void AppDraw(App *app);

// US-16: guarda la malla en 'ruta' (serialización + escritura atómica). En
// éxito actualiza ruta, limpia 'modificado' y el título; en fallo muestra el
// error (RF-10/RF-11). Devuelve true si el guardado tuvo éxito.
bool AppGuardarEnRuta(App *app, const char *ruta);

// US-19 RF-04..RF-07/RF-14: inicia una acción que puede requerir confirmación
// (SALIR/ABRIR/CREAR_NUEVA). Si la malla tiene cambios sin guardar se muestra
// la confirmación de 3 opciones; si no, la acción se ejecuta directamente.
void AppRequestAccion(App *app, AppAccion accion);

// US-19 RF-08: cuando el diálogo de guardado de US-16 completa con éxito
// (malla sin archivo), ejecuta la acción pendiente (SALIR/ABRIR/CREAR_NUEVA).
void AppContinuarAccion(App *app);

// US-17 RF-09..RF-13: carga la malla desde 'ruta'. En éxito reemplaza la
// malla en memoria, resetea cámara/selecciones/modo conectar y actualiza el
// título. En fallo muestra el error y la malla actual queda intacta.
void AppAbrirEnRuta(App *app, const char *ruta);

// Muestra un aviso transitorio sobre el lienzo (US-12), con formato printf.
void AppShowAviso(App *app, const char *fmt, ...);

// US-22: deshacer/rehacer por instantáneas (historial.h). AppRegistrarUndo
// captura el estado actual ANTES de la mutación (RF-05); AppDeshacer y
// AppRehacer restauran el estado completo (RF-07), marcan la malla como
// modificada (RF-08) y reconcilian la selección (RF-09). AppDescartarUndo
// revierte el último registro cuando la mutación posterior no aplicó.
void AppRegistrarUndo(App *app);
void AppDeshacer(App *app);
void AppRehacer(App *app);
bool AppHayDeshacer(void);
bool AppHayRehacer(void);
void AppDescartarUndo(void);
void AppLimpiarHistorial(void);

// US-22 RF-09: tras deshacer/rehacer, limpia los elementos de la selección
// (ramo, área, flecha) que ya no existen en el estado restaurado, y cancela el
// modo conectar/arrastre si su ramo desapareció.
void AppReconciliarSeleccion(App *app);

// Funciones helper para manejo de selección múltiple.

// Verifica si un ramo con el ID dado está seleccionado.
bool AppEstaSeleccionado(const App *app, int ramoId);

// Agrega un ramo a la selección. Si 'limpiar' es true, limpia la selección
// anterior; si es false, agrega a la selección existente (Ctrl+Click).
void AppSelectRamo(App *app, int ramoId, bool limpiar);

// Remueve un ramo de la selección.
void AppDeselectRamo(App *app, int ramoId);

// Limpia toda la selección de ramos.
void AppClearSelection(App *app);

// Selecciona todos los ramos cuyo nodo (rectángulo en mundo) está dentro del
// rectángulo dado (en pantalla). Se adapta a la cámara para calcular en mundo.
void AppSelectRamosEnRectangulo(App *app, Rectangle rectPantalla,
                                CameraView cam, Rectangle viewport);

#endif
