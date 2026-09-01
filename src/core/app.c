#include "app.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "font.h"
#include "historial.h"
#include "layout.h"
#include "dialogs.h"
#include "malla_json.h"
#include "panel_areas.h"
#include "render_malla.h"
#include "theme.h"
#include "workspace.h"

// Doble clic sobre un ramo para editarlo (US-08): último ramo clicado y
// momento del clic.
static int s_ultimoClicId = SIN_AREA_ID;
static double s_ultimoClicTiempo = -10.0;
static double s_ultimoCtrlFlechaTiempo = -10.0;

// US-22 RF-06: durante el arrastre de un ramo (US-10) o una secuencia de
// Ctrl+flechas se mantiene una instantánea previa al movimiento; al terminar
// se registra una única entrada de deshacer, solo si la posición cambió
// (CE-05/CE-06). Las pilas de deshacer/rehacer viven en historial.c.
static Malla s_movimientoSnapshot;
static bool s_movimientoEnCurso = false;

// US-10 RF-03: umbral (px) para distinguir el clic (selección) del arrastre.
#define ARRASTRE_UMBRAL_PX 5.0f
// US-10 RF-04: intervalo entre pasos del movimiento continuo con Ctrl + flecha.
#define CTRL_FLECHA_INTERVALO_S 0.10f

// US-15 RF-08: rango de zoom 25%–400%.
#define ZOOM_MIN 0.25f
#define ZOOM_MAX 4.0f
// RNF-03: ≈10% por muesca de rueda y 25% por botón.
#define ZOOM_PASO_RUEDA  1.1f
#define ZOOM_PASO_BOTON  1.25f
// RF-03/RF-04: desplazamiento por muesca de rueda (px de pantalla).
#define PAN_RUEDA_PX 60.0f

// ---------------------------------------------------------------------------
// US-15 RF-06/RF-07: aplica un factor de zoom manteniendo fijo el punto de
// mundo que queda bajo 'centroPantalla' (el cursor con Ctrl+rueda, el centro
// del lienzo con los botones +/−). Respeta el rango ZOOM_MIN–ZOOM_MAX.
// ---------------------------------------------------------------------------
static void CamaraAplicarZoom(App *app, Rectangle canvas, Vector2 centroPantalla,
                              float factor)
{
    float nuevo = app->camara.zoom * factor;
    if (nuevo < ZOOM_MIN) nuevo = ZOOM_MIN;
    if (nuevo > ZOOM_MAX) nuevo = ZOOM_MAX;
    if (nuevo == app->camara.zoom)
        return;

    // La cámara usa como objetivo el centro del viewport (camera.h).
    Vector2 offset = { canvas.x + canvas.width * 0.5f,
                       canvas.y + canvas.height * 0.5f };
    Vector2 mundo = { (centroPantalla.x - offset.x) / app->camara.zoom +
                          app->camara.pos.x,
                      (centroPantalla.y - offset.y) / app->camara.zoom +
                          app->camara.pos.y };

    app->camara.zoom = nuevo;
    app->camara.pos.x = mundo.x - (centroPantalla.x - offset.x) / nuevo;
    app->camara.pos.y = mundo.y - (centroPantalla.y - offset.y) / nuevo;
}

// ---------------------------------------------------------------------------
// Estado del título de la ventana (US-01 RF-08)
// ---------------------------------------------------------------------------

static void UpdateTitle(App *app)
{
    char title[128];

    if (app->screen == SCREEN_START)
    {
        snprintf(title, sizeof(title), "Visor de Mallas Universitarias");
    }
    else if (MallaGetRuta(&app->malla)[0] == '\0')
    {
        snprintf(title, sizeof(title), "Malla sin t\u00edtulo \u2014 sin guardar");
    }
    else
    {
        snprintf(title, sizeof(title), "%s", MallaGetRuta(&app->malla));
    }

    // US-16 RF-12: asterisco al final con cambios sin guardar.
    if (app->screen == SCREEN_CANVAS && MallaIsModified(&app->malla))
        strncat(title, "*", sizeof(title) - strlen(title) - 1);

    SetWindowTitle(title);
}

// ---------------------------------------------------------------------------
// Creación de malla y confirmación (US-01 RF-02/RF-07)
// ---------------------------------------------------------------------------

static void DoNuevaMalla(App *app)
{
    InitMalla(&app->malla);
    // US-22: una malla nueva no comparte historial con la anterior.
    AppLimpiarHistorial();
    app->screen = SCREEN_CANVAS;
    app->camara = CameraViewNew();
    app->areaSeleccionadaId = SIN_AREA_ID;
    app->ramoSeleccionadoId = SIN_AREA_ID;
    app->modoConectar = false;
    app->origenConectarId = SIN_AREA_ID;
    app->conexionSelOrigenId = SIN_AREA_ID;
    app->conexionSelDestinoId = SIN_AREA_ID;
    app->arrastrando = false;
    app->arrastreRamoId = SIN_AREA_ID;
    app->arrastrePress = (Vector2){ 0.0f, 0.0f };
    app->arrastreOffset = (Vector2){ 0.0f, 0.0f };
    app->arrastredeltaAnterior = (Vector2){ 0.0f, 0.0f };
    
    // Inicializar selección múltiple.
    app->nRamosSeleccionados = 0;
    memset(app->ramoSeleccionados, 0, sizeof(app->ramoSeleccionados));
    app->rectSelectorActivo = false;
    app->rectSelectorInicio = (Vector2){ 0.0f, 0.0f };
    app->rectSelectorActual = (Vector2){ 0.0f, 0.0f };
    
    app->aviso[0] = '\0';
    app->avisoRestante = 0.0;
    app->panActivo = false;
    app->panPress = (Vector2){ 0.0f, 0.0f };
    app->panPosInicial = (Vector2){ 0.0f, 0.0f };
    app->dialogErrorGuardar = false;
    app->dialogErrorAbrir = false;
    app->dialogErrorExportar = false;
    app->dialogConfirmar = false;
    app->continuarTrasGuardar = false;
    app->accionPendiente = ACCION_NINGUNA;
    app->modal = MODAL_NONE;
    app->panelScroll = (Vector2){ 0.0f, 0.0f };
    UpdateTitle(app);
}

// ---------------------------------------------------------------------------
// US-19: confirmación de 3 opciones reutilizable (SALIR/ABRIR/CREAR_NUEVA)
// ---------------------------------------------------------------------------

// Ejecuta la acción pendiente una vez autorizada por la confirmación.
static void EjecutarAccion(App *app, AppAccion accion)
{
    switch (accion)
    {
        case ACCION_SALIR:
            // RF-04: cierre diferido; el bucle principal sale al ver la bandera.
            app->cerrarAprobado = true;
            break;
        case ACCION_ABRIR:
            DialogsOpenAbrir(app);
            break;
        case ACCION_CREAR_NUEVA:
            DoNuevaMalla(app);
            break;
        case ACCION_NINGUNA:
        default:
            break;
    }
}

// RF-04..RF-07/RF-14: si hay cambios sin guardar se pide la confirmación de 3
// opciones; si no, la acción se ejecuta directamente.
void AppRequestAccion(App *app, AppAccion accion)
{
    if (accion == ACCION_NINGUNA)
        return;

    if (MallaIsModified(&app->malla))
    {
        app->accionPendiente = accion;
        app->dialogConfirmar = true;
    }
    else
    {
        EjecutarAccion(app, accion);
    }
}

// RF-08: cuando el guardado del diálogo de US-16 (malla sin archivo) completa
// con éxito, se ejecuta la acción pendiente que originó la confirmación. Si
// no hay acción pendiente, no hace nada.
void AppContinuarAccion(App *app)
{
    AppAccion accion = app->accionPendiente;
    app->accionPendiente = ACCION_NINGUNA;
    app->continuarTrasGuardar = false;
    EjecutarAccion(app, accion);
}

// RF-13: mensaje de la confirmación con el nombre de la malla/archivo actual.
static void MensajeConfirmar(App *app, char *buf, int tam)
{
    const char *ruta = MallaGetRuta(&app->malla);
    if (ruta[0] != '\0')
    {
        const char *base = strrchr(ruta, '/');
        base = (base != NULL) ? base + 1 : ruta;
        snprintf(buf, tam, "Hay cambios sin guardar en \xC2\xAB%s\xC2\xBB.",
                 base);
    }
    else
    {
        snprintf(buf, tam,
                 "La malla sin t\xC3\xADtulo tiene cambios sin guardar.");
    }
    strncat(buf, " \xC2\xBFQu\xC3\xA9 deseas hacer?", tam - strlen(buf) - 1);
}

// ---------------------------------------------------------------------------
// Funciones helper para selección múltiple
// ---------------------------------------------------------------------------

bool AppEstaSeleccionado(const App *app, int ramoId)
{
    for (int i = 0; i < app->nRamosSeleccionados; i++)
    {
        if (app->ramoSeleccionados[i] == ramoId)
            return true;
    }
    return false;
}

void AppSelectRamo(App *app, int ramoId, bool limpiar)
{
    if (limpiar)
    {
        // Reemplazar selección
        app->nRamosSeleccionados = 0;
    }
    else if (AppEstaSeleccionado(app, ramoId))
    {
        // Ya está seleccionado, no agregarlo de nuevo
        return;
    }

    if (app->nRamosSeleccionados < MAX_RAMOS)
    {
        app->ramoSeleccionados[app->nRamosSeleccionados] = ramoId;
        app->nRamosSeleccionados++;
    }

    // Actualizar el ramo seleccionado "principal" (para compatibilidad)
    app->ramoSeleccionadoId = ramoId;
}

void AppDeselectRamo(App *app, int ramoId)
{
    for (int i = 0; i < app->nRamosSeleccionados; i++)
    {
        if (app->ramoSeleccionados[i] == ramoId)
        {
            // Remover este ramo moviendo los posteriores
            for (int j = i; j < app->nRamosSeleccionados - 1; j++)
            {
                app->ramoSeleccionados[j] = app->ramoSeleccionados[j + 1];
            }
            app->nRamosSeleccionados--;

            // Actualizar ramo seleccionado "principal"
            if (app->nRamosSeleccionados > 0)
                app->ramoSeleccionadoId = app->ramoSeleccionados[app->nRamosSeleccionados - 1];
            else
                app->ramoSeleccionadoId = SIN_AREA_ID;

            return;
        }
    }
}

void AppClearSelection(App *app)
{
    app->nRamosSeleccionados = 0;
    app->ramoSeleccionadoId = SIN_AREA_ID;
}

void AppSelectRamosEnRectangulo(App *app, Rectangle rectPantalla,
                                CameraView cam, Rectangle viewport)
{
    // Convertir el rectángulo de pantalla a mundo
    Vector2 esquina1 = CameraScreenToWorld(cam, viewport,
                                           (Vector2){ rectPantalla.x, rectPantalla.y });
    Vector2 esquina2 = CameraScreenToWorld(
        cam, viewport,
        (Vector2){ rectPantalla.x + rectPantalla.width,
                   rectPantalla.y + rectPantalla.height });

    // Normalizar el rectángulo en mundo
    float minX = (esquina1.x < esquina2.x) ? esquina1.x : esquina2.x;
    float maxX = (esquina1.x < esquina2.x) ? esquina2.x : esquina1.x;
    float minY = (esquina1.y < esquina2.y) ? esquina1.y : esquina2.y;
    float maxY = (esquina1.y < esquina2.y) ? esquina2.y : esquina1.y;

    Rectangle rectMundo = { minX, minY, maxX - minX, maxY - minY };

    // Limpiar selección anterior
    AppClearSelection(app);

    // Verificar cada ramo
    int nRamos = MallaGetRamoCount(&app->malla);
    for (int i = 0; i < nRamos; i++)
    {
        const Ramo *r = MallaGetRamo(&app->malla, i);
        if (r == NULL) continue;

        // Calcular rectángulo del nodo en mundo
        Rectangle rec = { r->posicion.x - NODO_ANCHO * 0.5f,
                          r->posicion.y - NODO_ALTO * 0.5f,
                          NODO_ANCHO, NODO_ALTO };

        // Verificar si el nodo intersecta con el selector
        if (CheckCollisionRecs(rec, rectMundo))
        {
            AppSelectRamo(app, r->id, false);  // false = acumular
        }
    }
}

// ---------------------------------------------------------------------------
// Confirmación de 3 opciones reutilizable (US-19)
// ---------------------------------------------------------------------------

// Confirmación de 3 opciones (RF-07): "Guardar" ejecuta el flujo de US-16 y,
// si completa con éxito, la acción pendiente (RF-08); "No guardar" descarta y
// ejecuta la acción (RF-09); "Cancelar" aborta todo (RF-10). Si el guardado
// no completa, la acción pendiente se cancela y la malla queda intacta
// (RF-11). Sin anidamiento (RF-12).
static void DrawConfirmar(App *app)
{
    if (!app->dialogConfirmar)
        return;

    Rectangle box = { (GetScreenWidth() - 460.0f) * 0.5f,
                      (GetScreenHeight() - 170.0f) * 0.5f,
                      460.0f, 170.0f };

    char msg[1100];
    MensajeConfirmar(app, msg, (int)sizeof(msg));

    int btnActive = -1;
    if (GuiMessageBox(box, "Confirmar", msg, "Guardar;No guardar;Cancelar",
                      &btnActive) != 0)
    {
        app->dialogConfirmar = false;

        if (btnActive == 0)
        {
            // "Guardar": con archivo asociado se guarda y continúa; sin
            // archivo se abre el diálogo de US-16 y al completarlo se ejecuta
            // la acción pendiente (RF-08).
            if (MallaGetRuta(&app->malla)[0] != '\0')
            {
                AppAccion accion = app->accionPendiente;
                app->accionPendiente = ACCION_NINGUNA;
                if (AppGuardarEnRuta(app, MallaGetRuta(&app->malla)))
                    EjecutarAccion(app, accion);
                // Si el guardado falla (RF-11) el error lo muestra US-16 y la
                // acción queda cancelada.
            }
            else
            {
                app->continuarTrasGuardar = true;
                DialogsOpenGuardar(app);
            }
        }
        else if (btnActive == 1)
        {
            // "No guardar": descarta y ejecuta la acción pendiente (RF-09).
            AppAccion accion = app->accionPendiente;
            app->accionPendiente = ACCION_NINGUNA;
            EjecutarAccion(app, accion);
        }
        else
        {
            // "Cancelar": aborta; la malla y los cambios quedan intactos.
            app->accionPendiente = ACCION_NINGUNA;
        }
    }
}

// ---------------------------------------------------------------------------
// Aviso transitorio (US-12)
// ---------------------------------------------------------------------------

void AppShowAviso(App *app, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(app->aviso, sizeof(app->aviso), fmt, args);
    va_end(args);
    app->avisoRestante = 3.0;
}

// US-13 RF-06/RF-11: elimina la conexión seleccionada de forma inmediata (sin
// confirmación). No hace nada si no hay flecha seleccionada (CE-08/CE-10).
// US-22 RF-05: registra el estado anterior antes de mutar.
static void DoEliminarConexion(App *app)
{
    if (app->conexionSelOrigenId == SIN_AREA_ID)
        return;

    AppRegistrarUndo(app);
    if (!MallaRemovePrerrequisito(&app->malla, app->conexionSelDestinoId,
                                  app->conexionSelOrigenId))
        AppDescartarUndo();   // la relación ya no existía
    app->conexionSelOrigenId = SIN_AREA_ID;
    app->conexionSelDestinoId = SIN_AREA_ID;
    AppShowAviso(app, "Conexi\u00f3n eliminada.");
}

// Dibuja el aviso transitorio en la parte inferior del lienzo, con desvanecido.
static void DrawAviso(App *app)
{
    if (app->avisoRestante <= 0.0)
        return;

    Rectangle canvas = LayoutCanvasRect(GetScreenWidth(), GetScreenHeight());
    Vector2 t = MeasureTextEx(app->font, app->aviso, 14.0f, 1.0f);
    Rectangle box = { canvas.x + (canvas.width - t.x - 24.0f) * 0.5f,
                      canvas.y + canvas.height - 40.0f,
                      t.x + 24.0f, 28.0f };
    unsigned char alpha =
        (unsigned char)(255.0f * fminf(1.0f, (float)(app->avisoRestante / 0.5)));
    Color bg = { 28, 28, 34, alpha };
    Color fg = { 255, 255, 255, alpha };
    DrawRectangleRounded(box, 0.3f, 6, bg);
    DrawTextEx(app->font, app->aviso,
               (Vector2){ box.x + 12.0f, box.y + 6.0f }, 14.0f, 1.0f, fg);
}

// ---------------------------------------------------------------------------
// Guardado (US-16)
// ---------------------------------------------------------------------------

bool AppGuardarEnRuta(App *app, const char *ruta)
{
    char err[256];
    if (MallaGuardarArchivo(&app->malla, ruta, err, sizeof(err)))
    {
        // RF-10: tras guardar con éxito se limpia 'modificado' y el título
        // muestra el nombre del archivo (sin asterisco).
        MallaSetRuta(&app->malla, ruta);
        MallaClearModified(&app->malla);
        UpdateTitle(app);
        AppShowAviso(app, "Malla guardada.");
        return true;
    }
    else
    {
        // RF-11: ante fallo, mensaje de error y 'modificado' sigue en true;
        // el trabajo en memoria no se pierde.
        app->dialogErrorGuardar = true;
        snprintf(app->errorGuardar, sizeof(app->errorGuardar), "%s", err);
        return false;
    }
}

// Muestra el error de guardado (RF-11) con GuiMessageBox.
static void DrawErrorGuardar(App *app)
{
    if (!app->dialogErrorGuardar)
        return;

    Rectangle box = { (GetScreenWidth() - 500.0f) * 0.5f,
                      (GetScreenHeight() - 150.0f) * 0.5f,
                      500.0f, 150.0f };

    int btnActive = -1;
    if (GuiMessageBox(box, "Error al guardar", app->errorGuardar,
                      "Aceptar", &btnActive) != 0)
        app->dialogErrorGuardar = false;
}

// ---------------------------------------------------------------------------
// Apertura de malla (US-17)
// ---------------------------------------------------------------------------

// RF-10..RF-13: reemplaza la malla, resetea cámara y selecciones y actualiza
// el título. Solo se llama con un archivo ya leído con éxito.
void AppAbrirEnRuta(App *app, const char *ruta)
{
    char err[256];
    if (!MallaCargarArchivo(&app->malla, ruta, err, sizeof(err)))
    {
        // RF-09: ante cualquier fallo la malla actual queda intacta (datos y
        // 'modificado' sin cambios); solo se muestra el error.
        app->dialogErrorAbrir = true;
        snprintf(app->errorAbrir, sizeof(app->errorAbrir), "%s", err);
        return;
    }

    // RF-11: cámara a (0,0)/100% y limpieza de selecciones y modo conectar.
    app->camara = CameraViewNew();
    // US-22: el historial de deshacer no aplica a la malla recién abierta.
    AppLimpiarHistorial();
    app->areaSeleccionadaId = SIN_AREA_ID;
    app->ramoSeleccionadoId = SIN_AREA_ID;
    app->modoConectar = false;
    app->origenConectarId = SIN_AREA_ID;
    app->conexionSelOrigenId = SIN_AREA_ID;
    app->conexionSelDestinoId = SIN_AREA_ID;
    app->arrastrando = false;
    app->arrastreRamoId = SIN_AREA_ID;
    app->arrastrePress = (Vector2){ 0.0f, 0.0f };
    app->arrastreOffset = (Vector2){ 0.0f, 0.0f };
    app->aviso[0] = '\0';
    app->avisoRestante = 0.0;
    app->panActivo = false;
    app->panPress = (Vector2){ 0.0f, 0.0f };
    app->panPosInicial = (Vector2){ 0.0f, 0.0f };
    app->panelScroll = (Vector2){ 0.0f, 0.0f };

    // RF-10: la carga ya dejó 'modificado' en false y rutaArchivo puesta;
    // el título muestra el nombre del archivo (sin asterisco).
    app->screen = SCREEN_CANVAS;
    UpdateTitle(app);
    AppShowAviso(app, "Malla abierta.");
}

// RF-02/RF-03: la confirmación de apertura con cambios sin guardar ahora la
// gestiona la confirmación reutilizable de US-19 (AppRequestAccion + ACCION_ABRIR).

// Muestra el error de apertura (RF-09) con GuiMessageBox.
static void DrawErrorAbrir(App *app)
{
    if (!app->dialogErrorAbrir)
        return;

    Rectangle box = { (GetScreenWidth() - 500.0f) * 0.5f,
                      (GetScreenHeight() - 150.0f) * 0.5f,
                      500.0f, 150.0f };

    int btnActive = -1;
    if (GuiMessageBox(box, "Error al abrir", app->errorAbrir,
                      "Aceptar", &btnActive) != 0)
        app->dialogErrorAbrir = false;
}

// Muestra el error de exportación (US-18 RF-10) con GuiMessageBox.
static void DrawErrorExportar(App *app)
{
    if (!app->dialogErrorExportar)
        return;

    Rectangle box = { (GetScreenWidth() - 500.0f) * 0.5f,
                      (GetScreenHeight() - 150.0f) * 0.5f,
                      500.0f, 150.0f };

    int btnActive = -1;
    if (GuiMessageBox(box, "Error al exportar", app->errorExportar,
                      "Aceptar", &btnActive) != 0)
        app->dialogErrorExportar = false;
}

// ---------------------------------------------------------------------------
// US-22: deshacer/rehacer y sesión de movimiento
// ---------------------------------------------------------------------------

// RF-06/CE-06: compara solo las posiciones (no 'modified'): un arrastre que
// vuelve al punto de partida no genera entrada aunque MallaMoveRamo marcara la
// malla como modificada durante el trayecto.
static bool PosicionesCoinciden(const Malla *a, const Malla *b)
{
    if (a->nRamos != b->nRamos)
        return false;
    for (int i = 0; i < a->nRamos; i++)
    {
        const Ramo *ra = &a->ramos[i];
        const Ramo *rb = &b->ramos[i];
        if (ra->id != rb->id)
            return false;
        if (ra->posicion.x != rb->posicion.x ||
            ra->posicion.y != rb->posicion.y)
            return false;
    }
    return true;
}

// RF-06: captura la instantánea previa al primer paso del movimiento en curso
// (no vuelve a capturar si la sesión ya está activa).
static void IniciarMovimiento(App *app)
{
    if (!s_movimientoEnCurso)
    {
        s_movimientoEnCurso = true;
        s_movimientoSnapshot = app->malla;
    }
}

// RF-06: al terminar la sesión registra la entrada única si hubo cambio real.
static void FinalizarMovimiento(App *app)
{
    if (!s_movimientoEnCurso)
        return;
    s_movimientoEnCurso = false;
    if (!PosicionesCoinciden(&s_movimientoSnapshot, &app->malla))
        HistorialRegistrar(s_movimientoSnapshot);
}

// RF-09/RNF-04: la selección no es parte del snapshot; tras restaurar un
// estado se validan los elementos seleccionados contra la malla restaurada.
void AppReconciliarSeleccion(App *app)
{
    // Ramo principal.
    if (app->ramoSeleccionadoId != SIN_AREA_ID &&
        MallaFindRamoById(&app->malla, app->ramoSeleccionadoId) == NULL)
        app->ramoSeleccionadoId = SIN_AREA_ID;

    // Selección múltiple: se conservan solo los IDs que siguen existiendo.
    if (app->nRamosSeleccionados > 0)
    {
        int vivos[MAX_RAMOS];
        int n = 0;
        for (int i = 0; i < app->nRamosSeleccionados; i++)
        {
            if (MallaFindRamoById(&app->malla, app->ramoSeleccionados[i]) != NULL)
                vivos[n++] = app->ramoSeleccionados[i];
        }
        for (int i = 0; i < n; i++)
            app->ramoSeleccionados[i] = vivos[i];
        app->nRamosSeleccionados = n;
        if (n == 0)
        {
            app->ramoSeleccionadoId = SIN_AREA_ID;
        }
        else if (MallaFindRamoById(&app->malla, app->ramoSeleccionadoId) == NULL)
        {
            // El principal desapareció pero quedan otros: apunta al último
            // superviviente (coherente con AppSelectRamo/AppDeselectRamo).
            app->ramoSeleccionadoId = vivos[n - 1];
        }
    }

    // Área.
    if (app->areaSeleccionadaId != SIN_AREA_ID &&
        MallaFindAreaById(&app->malla, app->areaSeleccionadaId) == NULL)
        app->areaSeleccionadaId = SIN_AREA_ID;

    // Flecha: ambos extremos deben existir y la relación debe seguir presente
    // en la lista de prerrequisitos del destino.
    if (app->conexionSelOrigenId != SIN_AREA_ID)
    {
        const Ramo *destino = MallaFindRamoById(&app->malla,
                                                app->conexionSelDestinoId);
        const Ramo *origen = MallaFindRamoById(&app->malla,
                                               app->conexionSelOrigenId);
        bool existe = (destino != NULL && origen != NULL);
        if (existe)
        {
            existe = false;
            for (int i = 0; i < destino->nPrerrequisitos; i++)
            {
                if (destino->prerrequisitos[i] == app->conexionSelOrigenId)
                {
                    existe = true;
                    break;
                }
            }
        }
        if (!existe)
        {
            app->conexionSelOrigenId = SIN_AREA_ID;
            app->conexionSelDestinoId = SIN_AREA_ID;
        }
    }

    // Modo conectar: si el origen elegido desapareció, se sale del modo.
    if (app->origenConectarId != SIN_AREA_ID &&
        MallaFindRamoById(&app->malla, app->origenConectarId) == NULL)
    {
        app->origenConectarId = SIN_AREA_ID;
        app->modoConectar = false;
    }

    // Arrastre de un ramo que ya no existe.
    if (app->arrastreRamoId != SIN_AREA_ID &&
        MallaFindRamoById(&app->malla, app->arrastreRamoId) == NULL)
    {
        app->arrastreRamoId = SIN_AREA_ID;
        app->arrastrando = false;
        s_movimientoEnCurso = false;
    }
}

void AppRegistrarUndo(App *app)
{
    HistorialRegistrar(app->malla);
}

void AppDeshacer(App *app)
{
    // Cierra una sesión de movimiento pendiente (arrastre o Ctrl+flechas que
    // aún no se registró) antes de deshacer.
    FinalizarMovimiento(app);

    if (!HistorialDeshacer(&app->malla))
        return;   // CE-01/CE-02

    MallaMarkModified(&app->malla);   // RF-08
    AppReconciliarSeleccion(app);     // RF-09
    UpdateTitle(app);
}

void AppRehacer(App *app)
{
    FinalizarMovimiento(app);

    if (!HistorialRehacer(&app->malla))
        return;   // CE-03

    MallaMarkModified(&app->malla);   // RF-08
    AppReconciliarSeleccion(app);     // RF-09
    UpdateTitle(app);
}

bool AppHayDeshacer(void)
{
    return HistorialHayDeshacer();
}

bool AppHayRehacer(void)
{
    return HistorialHayRehacer();
}

void AppDescartarUndo(void)
{
    HistorialDescartar();
}

void AppLimpiarHistorial(void)
{
    HistorialLimpiar();
    s_movimientoEnCurso = false;
}

// ---------------------------------------------------------------------------
// Inicialización
// ---------------------------------------------------------------------------

void AppInit(App *app)
{
    app->font = LoadAppFont(&app->fontOwned);
    GuiSetFont(app->font);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 14);
    // US-22 RF-15: tooltips de la barra con los atajos (GuiSetTooltip por
    // botón); sin ellos no se dibujan.
    GuiEnableTooltip();

    InitMalla(&app->malla);
    app->screen = SCREEN_START;
    app->camara = CameraViewNew();
    app->areaSeleccionadaId = SIN_AREA_ID;
    app->ramoSeleccionadoId = SIN_AREA_ID;
    app->modoConectar = false;
    app->origenConectarId = SIN_AREA_ID;
    app->conexionSelOrigenId = SIN_AREA_ID;
    app->conexionSelDestinoId = SIN_AREA_ID;
    app->arrastrando = false;
    app->arrastreRamoId = SIN_AREA_ID;
    app->arrastrePress = (Vector2){ 0.0f, 0.0f };
    app->arrastreOffset = (Vector2){ 0.0f, 0.0f };
    app->aviso[0] = '\0';
    app->avisoRestante = 0.0;
    app->panActivo = false;
    app->panPress = (Vector2){ 0.0f, 0.0f };
    app->panPosInicial = (Vector2){ 0.0f, 0.0f };
    app->dialogErrorGuardar = false;
    app->dialogErrorAbrir = false;
    app->dialogErrorExportar = false;
    app->dialogConfirmar = false;
    app->continuarTrasGuardar = false;
    app->accionPendiente = ACCION_NINGUNA;
    app->cerrarAprobado = false;
    app->modal = MODAL_NONE;
    app->panelScroll = (Vector2){ 0.0f, 0.0f };

    // US-23 RF-09: se recuerda la carpeta de trabajo entre sesiones; si no
    // hay configuración queda vacío y la app usa el directorio actual.
    WorkspaceCargar(app->espacioTrabajo, (int)sizeof(app->espacioTrabajo));

    // US-18 (agregado): se recuerda la carpeta de exportación de imágenes
    // elegida con "Buscar…"; vacío → se usa el espacio de trabajo.
    WorkspaceCargarExportDir(app->espacioExportar,
                             (int)sizeof(app->espacioExportar));

    UpdateTitle(app);
}

void AppShutdown(App *app)
{
    if (app->fontOwned)
        UnloadFont(app->font);

    // US-23 CE-11: si el diálogo nativo sigue abierto se termina el proceso
    // hijo (no se dejan procesos huérfanos).
    DialogsShutdown(app);
}

// ---------------------------------------------------------------------------
// Actualización
// ---------------------------------------------------------------------------

void AppUpdate(App *app)
{
    // US-19 RF-04/RF-05: el botón de cierre de ventana inicia la confirmación
    // de salida si hay cambios sin guardar; si no hay, se cierra directamente.
    // Solo se intercepta cuando no hay un modal/diálogo abierto que deba
    // resolver antes el usuario.
    if (WindowShouldClose() && !app->cerrarAprobado &&
        app->modal == MODAL_NONE && !app->dialogConfirmar &&
        !app->dialogErrorGuardar && !app->dialogErrorAbrir &&
        !app->dialogErrorExportar)
    {
        AppRequestAccion(app, ACCION_SALIR);
        return;
    }

    // US-19: la confirmación de 3 opciones bloquea el lienzo mientras se
    // muestra (RF-07).
    if (app->dialogConfirmar)
        return;

    // El error de guardado (RF-11) bloquea el lienzo mientras se muestra.
    if (app->dialogErrorGuardar)
        return;

    // El error de apertura (US-17 RF-09) bloquea el lienzo mientras se muestra.
    if (app->dialogErrorAbrir)
        return;

    // El error de exportación (US-18 RF-10) bloquea el lienzo mientras se
    // muestra.
    if (app->dialogErrorExportar)
        return;

    // US-02 RF-02: con un modal abierto el lienzo queda bloqueado.
    if (app->modal != MODAL_NONE)
        return;

    // El aviso transitorio (US-12) se desvanece con el tiempo.
    if (app->avisoRestante > 0.0)
        app->avisoRestante -= GetFrameTime();

    // US-22 RF-14: los atajos solo se procesan sin modal/diálogo abierto (los
    // checks anteriores ya retornaron si lo hay), así no interfieren con la
    // edición de texto en los modales (CE-13). RF-16: en la pantalla de inicio
    // solo funcionan Ctrl+N/Ctrl+O/Ctrl+S; Supr/Ctrl+Z/Ctrl+Y/Ctrl+E no.
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if (ctrl)
    {
        if (IsKeyPressed(KEY_S))
        {
            // RF-01/CE-09: misma acción que el botón "Guardar" (US-16).
            if (MallaGetRuta(&app->malla)[0] == '\0')
                DialogsOpenGuardar(app);
            else
                AppGuardarEnRuta(app, MallaGetRuta(&app->malla));
        }
        else if (IsKeyPressed(KEY_O))
        {
            // RF-12: misma acción que el botón "Abrir" (US-17).
            AppRequestAccion(app, ACCION_ABRIR);
        }
        else if (IsKeyPressed(KEY_N))
        {
            // RF-11: misma acción que el botón "Nueva malla" (US-01).
            AppRequestAccion(app, ACCION_CREAR_NUEVA);
        }
        else if (app->screen == SCREEN_CANVAS && IsKeyPressed(KEY_E))
        {
            // RF-13: misma acción que el botón "Exportar imagen" (US-18).
            DialogsOpenExportar(app);
        }
        else if (app->screen == SCREEN_CANVAS && IsKeyPressed(KEY_Z))
        {
            // RF-02/RF-03: Ctrl+Z deshace; Ctrl+Shift+Z rehace.
            if (shift)
                AppRehacer(app);
            else
                AppDeshacer(app);
        }
        else if (app->screen == SCREEN_CANVAS && IsKeyPressed(KEY_Y))
        {
            // RF-03: Ctrl+Y rehace.
            AppRehacer(app);
        }
    }

    if (app->screen == SCREEN_CANVAS)
    {
        // "Nueva malla" en el lienzo (US-01 RF-07): si hay cambios sin
        // guardar, la confirmación reutilizable de US-19 lo resuelve. Con Ctrl
        // se maneja arriba (US-22 RF-11) para no ejecutarla dos veces.
        if (IsKeyPressed(KEY_N) && !ctrl)
            AppRequestAccion(app, ACCION_CREAR_NUEVA);

        // US-13 RF-08: Esc deselecciona la flecha (sin eliminar); si no hay
        // flecha, cancela el modo conectar (US-12).
        if (IsKeyPressed(KEY_ESCAPE))
        {
            if (app->conexionSelOrigenId != SIN_AREA_ID)
            {
                app->conexionSelOrigenId = SIN_AREA_ID;
                app->conexionSelDestinoId = SIN_AREA_ID;
            }
            else if (app->modoConectar)
            {
                app->modoConectar = false;
                app->origenConectarId = SIN_AREA_ID;
                AppShowAviso(app, "Modo conectar cancelado.");
            }
        }

        // US-22 RF-10/CE-07/CE-08: Supr elimina con prioridad ramo > flecha >
        // área, usando las confirmaciones actuales (US-09/US-13/US-05).
        if (IsKeyPressed(KEY_DELETE))
        {
            if (app->nRamosSeleccionados > 0)
                DialogsOpenEliminarRamo(app);
            else if (app->conexionSelOrigenId != SIN_AREA_ID)
                DoEliminarConexion(app);
            else if (app->areaSeleccionadaId != SIN_AREA_ID)
                DialogsOpenEliminarArea(app);
        }

        // US-10 RF-04 (extendido para múltiple selección): Ctrl + Arrow keys
        // desplaza los ramos seleccionados en intervalos temporales para
        // mantener el movimiento suave y continuo. US-22 RF-06: la secuencia de
        // pasos forma una única entrada de deshacer (se registra al terminar).
        if (app->nRamosSeleccionados > 0 && ctrl)
        {
            bool direccionActiva = false;
            Vector2 delta = { 0.0f, 0.0f };

            if (IsKeyDown(KEY_UP))
            {
                delta.y -= 5.0f;
                direccionActiva = true;
            }
            else if (IsKeyDown(KEY_DOWN))
            {
                delta.y += 5.0f;
                direccionActiva = true;
            }
            else if (IsKeyDown(KEY_LEFT))
            {
                delta.x -= 5.0f;
                direccionActiva = true;
            }
            else if (IsKeyDown(KEY_RIGHT))
            {
                delta.x += 5.0f;
                direccionActiva = true;
            }

            if (direccionActiva)
            {
                double ahora = GetTime();
                if (ahora - s_ultimoCtrlFlechaTiempo >=
                    CTRL_FLECHA_INTERVALO_S)
                {
                    IniciarMovimiento(app);
                    // Mover todos los ramos seleccionados
                    for (int i = 0; i < app->nRamosSeleccionados; i++)
                    {
                        int id = app->ramoSeleccionados[i];
                        const Ramo *r = MallaFindRamoById(&app->malla, id);
                        if (r != NULL)
                        {
                            Vector2 nueva_pos = { r->posicion.x + delta.x,
                                                  r->posicion.y + delta.y };
                            MallaMoveRamo(&app->malla, id, nueva_pos);
                        }
                    }
                    s_ultimoCtrlFlechaTiempo = ahora;
                }
            }
        }
        else
        {
            s_ultimoCtrlFlechaTiempo = -10.0;
            // RF-06: al soltar Ctrl/flechas se cierra la sesión de movimiento;
            // durante un arrastre con el ratón la cierra el soltado del botón.
            if (!app->arrastrando)
                FinalizarMovimiento(app);
        }

        Rectangle canvasNav = LayoutCanvasRect(GetScreenWidth(),
                                               GetScreenHeight());

        // US-15 RF-03/RF-04/RF-06: navegación con la rueda, solo cuando el
        // cursor está sobre el lienzo (si hay modal abierto, AppUpdate ya
        // retornó: CE-08). Ctrl+rueda hace zoom centrado en el cursor.
        if (CheckCollisionPointRec(GetMousePosition(), canvasNav))
        {
            float ruedaY = GetMouseWheelMove();
            float ruedaX = GetMouseWheelMoveV().x;
            if (ruedaY != 0.0f || ruedaX != 0.0f)
            {
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
                {
                    if (ruedaY != 0.0f)
                        CamaraAplicarZoom(app, canvasNav, GetMousePosition(),
                                          ruedaY > 0.0f ? ZOOM_PASO_RUEDA
                                                        : 1.0f / ZOOM_PASO_RUEDA);
                }
                else
                {
                    float pasoT = PAN_RUEDA_PX / app->camara.zoom;
                    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                        app->camara.pos.x -= ruedaY * pasoT;
                    else
                        app->camara.pos.y -= ruedaY * pasoT;
                    // Rueda horizontal, si el hardware la proporciona.
                    app->camara.pos.x += ruedaX * pasoT;
                }
            }
        }

        // US-15 RF-05: arrastre con el botón central desplaza la vista en
        // cualquier dirección (sin límites de cámara, CE-04).
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) &&
            CheckCollisionPointRec(GetMousePosition(), canvasNav))
        {
            app->panActivo = true;
            app->panPress = GetMousePosition();
            app->panPosInicial = app->camara.pos;
        }
        if (app->panActivo && IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
        {
            Vector2 p = GetMousePosition();
            float inv = 1.0f / app->camara.zoom;
            app->camara.pos.x = app->panPosInicial.x -
                                (p.x - app->panPress.x) * inv;
            app->camara.pos.y = app->panPosInicial.y -
                                (p.y - app->panPress.y) * inv;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE))
            app->panActivo = false;

        // US-08/US-09/US-11 (extendido para múltiple selección): el clic
        // selecciona el ramo bajo el cursor. Ctrl+clic agrega/remueve de la
        // selección. Clic en el vacío inicia un rectángulo selector o deselecciona.
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            Rectangle canvas = LayoutCanvasRect(GetScreenWidth(),
                                                 GetScreenHeight());
            if (CheckCollisionPointRec(GetMousePosition(), canvas))
            {
                // US-09 RF-01: el botón "Eliminar ramo" flota sobre el lienzo;
                // el clic en él no debe seleccionar/deseleccionar nodos (si no,
                // se deselecciona el ramo y el botón nunca llega a abrir el
                // diálogo).
                if (CheckCollisionPointRec(GetMousePosition(),
                                           LayoutEliminarRamoBtnRect()))
                    return;

                Vector2 world = CameraScreenToWorld(app->camara, canvas,
                                                    GetMousePosition());
                int idx = FindRamoAt(&app->malla, world);

                // US-12: en modo conectar el clic elige origen y destino en
                // lugar de seleccionar el ramo. En modo conectar, no permitir
                // múltiple selección (deshabilitada).
                if (app->modoConectar)
                {
                    if (idx < 0)
                    {
                        AppShowAviso(app, "Haz clic sobre un ramo para conectarlo.");
                        return;
                    }

                    const Ramo *destino = MallaGetRamo(&app->malla, idx);
                    if (app->origenConectarId == SIN_AREA_ID)
                    {
                        app->origenConectarId = destino->id;
                        AppShowAviso(app, "Origen: %s \u2014 haz clic en el ramo "
                                       "que lo requiere.", destino->nombre);
                    }
                    else if (destino->id == app->origenConectarId)
                    {
                        AppShowAviso(app, "Ese ramo es el origen; elige otro como "
                                       "destino.");
                    }
                    else
                    {
                        // US-22 RF-05: registrar antes de mutar (US-12).
                        AppRegistrarUndo(app);
                        if (MallaAddPrerrequisito(&app->malla, destino->id,
                                                  app->origenConectarId))
                        {
                            const Ramo *origen = MallaFindRamoById(
                                &app->malla, app->origenConectarId);
                            AppShowAviso(app, "%s es ahora prerrequisito de %s.",
                                      origen != NULL ? origen->nombre : "?",
                                      destino->nombre);
                        }
                        else
                        {
                            // La mutación no aplicó (ciclo o duplicado).
                            AppDescartarUndo();
                            AppShowAviso(app, "No se pudo conectar: la relaci\u00f3n "
                                           "crear\u00eda un ciclo o ya existe.");
                        }
                        app->origenConectarId = SIN_AREA_ID;
                    }
                    return;
                }

                if (idx >= 0)
                {
                    const Ramo *r = MallaGetRamo(&app->malla, idx);

                    // US-13 RF-02: seleccionar un ramo limpia la flecha.
                    app->conexionSelOrigenId = SIN_AREA_ID;
                    app->conexionSelDestinoId = SIN_AREA_ID;

                    // Manejo de selección múltiple (Ctrl+Click)
                    bool esCtrlClick = IsKeyDown(KEY_LEFT_CONTROL) ||
                                       IsKeyDown(KEY_RIGHT_CONTROL);

                    if (esCtrlClick)
                    {
                        // Ctrl+Click: agregar/remover de la selección
                        if (AppEstaSeleccionado(app, r->id))
                        {
                            AppDeselectRamo(app, r->id);
                        }
                        else
                        {
                            AppSelectRamo(app, r->id, false);  // false = acumular
                        }
                    }
                    else
                    {
                        // Clic normal: selección única
                        AppSelectRamo(app, r->id, true);  // true = limpiar
                    }

                    // US-10: candidato a arrastre; se activa si el mouse se
                    // desplaza más del umbral (RF-03). Solo permite arrastrar
                    // si el ramo clicado está en la selección.
                    if (AppEstaSeleccionado(app, r->id))
                    {
                        app->arrastreRamoId = r->id;
                        app->arrastrePress = GetMousePosition();
                        app->arrastreOffset = (Vector2){ r->posicion.x - world.x,
                                                         r->posicion.y - world.y };
                        app->arrastrando = false;
                    }

                    double now = GetTime();
                    bool dobleClic = (s_ultimoClicId == r->id &&
                                      now - s_ultimoClicTiempo < 0.4 &&
                                      !esCtrlClick);
                    s_ultimoClicId = r->id;
                    s_ultimoClicTiempo = now;

                    // Doble clic solo si hay exactamente 1 ramo seleccionado
                    if (dobleClic && app->nRamosSeleccionados == 1)
                        DialogsOpenEditarRamo(app);
                }
                else
                {
                    // US-13 RF-01: hit-test con prioridad ramos → flechas.
                    int origenId = SIN_AREA_ID, destinoId = SIN_AREA_ID;
                    if (FindFlechaAt(&app->malla, GetMousePosition(),
                                     app->camara, canvas, &origenId,
                                     &destinoId))
                    {
                        // RF-02: la flecha limpia la selección de ramo y área.
                        app->conexionSelOrigenId = origenId;
                        app->conexionSelDestinoId = destinoId;
                        app->areaSeleccionadaId = SIN_AREA_ID;
                        AppClearSelection(app);
                        s_ultimoClicId = SIN_AREA_ID;
                    }
                    else
                    {
                        // RF-09: clic en el vacío inicia rectángulo selector o
                        // deselecciona. Si Ctrl está presionado, inicia selector.
                        // Si no, deselecciona.
                        bool esCtrlClick = IsKeyDown(KEY_LEFT_CONTROL) ||
                                           IsKeyDown(KEY_RIGHT_CONTROL);

                        if (esCtrlClick)
                        {
                            // Iniciar rectángulo selector
                            app->rectSelectorActivo = true;
                            app->rectSelectorInicio = GetMousePosition();
                            app->rectSelectorActual = GetMousePosition();
                        }
                        else
                        {
                            // Deseleccionar todo
                            app->conexionSelOrigenId = SIN_AREA_ID;
                            app->conexionSelDestinoId = SIN_AREA_ID;
                            AppClearSelection(app);
                            s_ultimoClicId = SIN_AREA_ID;
                        }
                    }
                }
            }
        }

        // US-10 (extendido para múltiple selección): arrastre con feedback en
        // vivo (RF-02). Un movimiento ≤ 5 px entre pulsación y liberación es
        // selección, no movimiento (RF-03). Rectángulo selector actualiza
        // continuamente mientras se arrastra.
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            // Actualizar rectángulo selector si está activo
            if (app->rectSelectorActivo)
            {
                app->rectSelectorActual = GetMousePosition();
            }

            // Lógica de arrastre
            if (app->arrastreRamoId != SIN_AREA_ID && !app->arrastrando)
            {
                Vector2 p = GetMousePosition();
                float dx = p.x - app->arrastrePress.x;
                float dy = p.y - app->arrastrePress.y;
                if (dx * dx + dy * dy > ARRASTRE_UMBRAL_PX * ARRASTRE_UMBRAL_PX)
                {
                    app->arrastrando = true;
                    // US-22 RF-06: instantánea previa al primer movimiento; la
                    // entrada se registra al soltar (una sola por arrastre).
                    IniciarMovimiento(app);
                }
            }

            if (app->arrastrando)
            {
                Rectangle canvas = LayoutCanvasRect(GetScreenWidth(),
                                                     GetScreenHeight());
                Vector2 mundo = CameraScreenToWorld(app->camara, canvas,
                                                    GetMousePosition());

                // Si solo hay 1 ramo seleccionado, usar el offset tradicional
                if (app->nRamosSeleccionados == 1)
                {
                    Vector2 nueva = { mundo.x - app->arrastreOffset.x,
                                      mundo.y - app->arrastreOffset.y };
                    MallaMoveRamo(&app->malla, app->arrastreRamoId, nueva);
                }
                else if (app->nRamosSeleccionados > 1)
                {
                    // Múltiple selección: usar movimiento diferencial
                    Vector2 delta = { mundo.x - app->arrastredeltaAnterior.x,
                                      mundo.y - app->arrastredeltaAnterior.y };

                    for (int i = 0; i < app->nRamosSeleccionados; i++)
                    {
                        int id = app->ramoSeleccionados[i];
                        const Ramo *r = MallaFindRamoById(&app->malla, id);
                        if (r != NULL)
                        {
                            Vector2 nueva = { r->posicion.x + delta.x,
                                              r->posicion.y + delta.y };
                            MallaMoveRamo(&app->malla, id, nueva);
                        }
                    }

                    app->arrastredeltaAnterior = mundo;
                }
            }
        }

        // US-10 RF-02 (extendido): al soltar, la nueva posición queda fijada.
        // Si estaba activo el rectángulo selector, seleccionar los ramos dentro.
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
        {
            if (app->rectSelectorActivo)
            {
                // Normalizar el rectángulo selector
                Vector2 inicio = app->rectSelectorInicio;
                Vector2 actual = app->rectSelectorActual;

                float minX = (inicio.x < actual.x) ? inicio.x : actual.x;
                float maxX = (inicio.x < actual.x) ? actual.x : inicio.x;
                float minY = (inicio.y < actual.y) ? inicio.y : actual.y;
                float maxY = (inicio.y < actual.y) ? actual.y : inicio.y;

                Rectangle rectPantalla = { minX, minY, maxX - minX, maxY - minY };
                Rectangle canvas = LayoutCanvasRect(GetScreenWidth(),
                                                     GetScreenHeight());

                // Seleccionar todos los ramos dentro del rectángulo
                AppSelectRamosEnRectangulo(app, rectPantalla, app->camara,
                                           canvas);

                app->rectSelectorActivo = false;
            }

            // US-22 RF-06/CE-05/CE-06: el arrastre se registra como una única
            // entrada al soltarlo, y solo si la posición cambió.
            if (app->arrastrando)
                FinalizarMovimiento(app);

            app->arrastreRamoId = SIN_AREA_ID;
            app->arrastrando = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Dibujo
// ---------------------------------------------------------------------------

static void HandleToolbarAction(App *app, ToolbarAction action)
{
    switch (action)
    {
        case TOOLBAR_CREAR_AREA:
            // US-02 RF-02: abre el modal de creación de área.
            DialogsOpenCrearArea(app);
            break;

        case TOOLBAR_CREAR_RAMO:
            // US-06 RF-02: abre el modal de creación de ramo.
            DialogsOpenCrearRamo(app);
            break;

        case TOOLBAR_CONECTAR:
            // US-12: alterna el modo conectar; el origen se elige con el
            // primer clic y el destino con el segundo (ESC cancela).
            app->modoConectar = !app->modoConectar;
            app->origenConectarId = SIN_AREA_ID;
            // RF-10: en modo conectar no hay selección de flechas.
            app->conexionSelOrigenId = SIN_AREA_ID;
            app->conexionSelDestinoId = SIN_AREA_ID;
            if (app->modoConectar)
            {
                app->ramoSeleccionadoId = SIN_AREA_ID;
                AppShowAviso(app, "Modo conectar: haz clic en un ramo (origen) y "
                               "luego en el que lo requiere. ESC para salir.");
            }
            break;

        case TOOLBAR_ELIMINAR_CONEXION:
            // US-13 RF-04: botón contextual de la barra.
            DoEliminarConexion(app);
            break;

        case TOOLBAR_GUARDAR:
            // US-16 RF-02/RF-03: malla nueva → diálogo de nombre/ubicación;
            // malla con archivo asociado → sobrescribe sin preguntar.
            if (MallaGetRuta(&app->malla)[0] == '\0')
                DialogsOpenGuardar(app);
            else
                AppGuardarEnRuta(app, MallaGetRuta(&app->malla));
            break;

        case TOOLBAR_ABRIR:
            // US-17 RF-01..RF-03: confirmación de US-19 si hay cambios sin
            // guardar; luego el diálogo de apertura (RF-04..RF-06).
            AppRequestAccion(app, ACCION_ABRIR);
            break;

        case TOOLBAR_ZOOM_IN:
        case TOOLBAR_ZOOM_OUT:
            // US-15 RF-06/RF-07: zoom por botones, centrado en el lienzo.
        {
            Rectangle canvas = LayoutCanvasRect(GetScreenWidth(),
                                                GetScreenHeight());
            Vector2 centro = { canvas.x + canvas.width * 0.5f,
                               canvas.y + canvas.height * 0.5f };
            CamaraAplicarZoom(app, canvas, centro,
                              (action == TOOLBAR_ZOOM_IN) ? ZOOM_PASO_BOTON
                                                          : 1.0f / ZOOM_PASO_BOTON);
            break;
        }

        // US-18 RF-01: "Exportar imagen" abre el diálogo de exportación
        // (nombre/ruta + formato PNG/JPG).
        case TOOLBAR_EXPORTAR:
            DialogsOpenExportar(app);
            break;

        case TOOLBAR_NONE:
        default:
            break;
    }
}

void AppDraw(App *app)
{
    BeginDrawing();
    ClearBackground(RAYWHITE);

    if (app->screen == SCREEN_START)
    {
        StartScreenAction a = LayoutDrawStartScreen(GetScreenWidth(), GetScreenHeight());
        if (a == START_NUEVA_MALLA)
            AppRequestAccion(app, ACCION_CREAR_NUEVA);
        else if (a == START_ABRIR_MALLA)
            AppRequestAccion(app, ACCION_ABRIR);
        // El modal de apertura (US-17) y la confirmación de US-19 también se
        // dibujan sobre la pantalla inicial.
        DialogsDraw(app);
        DrawConfirmar(app);
        DrawErrorAbrir(app);
    }
    else
    {
        Rectangle canvas = LayoutCanvasRect(GetScreenWidth(), GetScreenHeight());

        // US-10 RNF-03 / US-15: cursor de "mover" durante el arrastre de un
        // ramo o el pan con botón central.
        SetMouseCursor((app->arrastrando || app->panActivo)
                           ? MOUSE_CURSOR_RESIZE_ALL
                           : MOUSE_CURSOR_DEFAULT);

        DrawCanvasGrid(app->camara, canvas);

        // US-14: flechas de prerrequisito debajo de los nodos; la flecha
        // seleccionada se resalta (US-13 RF-03). US-21: polilíneas ortogonales.
        DrawPrerrequisitoFlechas(&app->malla, app->camara, canvas,
                                 app->conexionSelOrigenId,
                                 app->conexionSelDestinoId);

        // US-21 RF-12: en modo conectar se dibuja la vista previa ortogonal
        // del origen al cursor (se enruta contra los ramos actuales; el
        // cursor no es obstáculo).
        if (app->modoConectar && app->origenConectarId != SIN_AREA_ID)
        {
            const Ramo *origen = MallaFindRamoById(&app->malla,
                                                   app->origenConectarId);
            if (origen != NULL)
            {
                Vector2 cursorWorld = CameraScreenToWorld(app->camara, canvas,
                                                          GetMousePosition());
                Ruta previa;
                RutaVistaPrevia(&app->malla, origen, cursorWorld, &previa);
                DrawRutaOrtogonal(&previa, app->camara, canvas,
                                  (Color){ 30, 144, 255, 150 }, 2.0f);
            }
        }

        // US-06 RF-09: los ramos creados se colocan en el centro del lienzo y
        // se dibujan con el color del área asignada o su color propio (US-07).
        // El ramo seleccionado se resalta (US-08/US-09). US-15 RF-13: culling,
        // solo se dibujan los ramos visibles en el lienzo.
        Rectangle areaVisible = { canvas.x - 32.0f, canvas.y - 32.0f,
                                  canvas.width + 64.0f, canvas.height + 64.0f };
        for (int i = 0; i < MallaGetRamoCount(&app->malla); i++)
        {
            const Ramo *r = MallaGetRamo(&app->malla, i);
            if (!CheckCollisionRecs(RamoNodeScreenRect(r, app->camara, canvas),
                                    areaVisible))
                continue;
            DrawRamoNode(&app->malla, r, AppEstaSeleccionado(app, r->id),
                         app->camara, canvas, app->font);
        }

        // US-12: resalta el origen elegido en modo conectar.
        if (app->modoConectar && app->origenConectarId != SIN_AREA_ID)
        {
            const Ramo *origen = MallaFindRamoById(&app->malla,
                                                   app->origenConectarId);
            if (origen != NULL)
            {
                Rectangle rec = RamoNodeScreenRect(origen, app->camara, canvas);
                float m = 4.0f * app->camara.zoom;
                DrawRectangleRoundedLines((Rectangle){ rec.x - m, rec.y - m,
                                                       rec.width + m * 2.0f,
                                                       rec.height + m * 2.0f },
                                          0.2f, 8, THEME_ACCENT);
            }
        }

        // Dibujar rectángulo selector si está activo.
        if (app->rectSelectorActivo)
        {
            Vector2 inicio = app->rectSelectorInicio;
            Vector2 actual = app->rectSelectorActual;

            float minX = (inicio.x < actual.x) ? inicio.x : actual.x;
            float maxX = (inicio.x < actual.x) ? actual.x : inicio.x;
            float minY = (inicio.y < actual.y) ? inicio.y : actual.y;
            float maxY = (inicio.y < actual.y) ? actual.y : inicio.y;

            Rectangle rect = { minX, minY, maxX - minX, maxY - minY };

            // Dibujar el rectángulo con relleno semitransparente y borde
            DrawRectangle((int)rect.x, (int)rect.y, (int)rect.width,
                          (int)rect.height, Fade(THEME_ACCENT, 0.1f));
            DrawRectangleLines((int)rect.x, (int)rect.y, (int)rect.width,
                               (int)rect.height, THEME_ACCENT);
        }

        // US-12: barra indicadora del modo conectar.
        if (app->modoConectar)
        {
            const char *mensaje =
                (app->origenConectarId == SIN_AREA_ID)
                    ? "Modo conectar: haz clic en un ramo (origen). ESC para salir."
                    : "Elige el ramo que lo requiere como destino. ESC para salir.";
            Vector2 t = MeasureTextEx(app->font, mensaje, 14.0f, 1.0f);
            Rectangle banner = { canvas.x + (canvas.width - t.x - 24.0f) * 0.5f,
                                 canvas.y + 8.0f, t.x + 24.0f, 28.0f };
            DrawRectangleRounded(banner, 0.3f, 6, (Color){ 28, 28, 34, 220 });
            DrawTextEx(app->font, mensaje,
                       (Vector2){ banner.x + 12.0f, banner.y + 6.0f },
                       14.0f, 1.0f, WHITE);
        }

        // Bloquea barra y panel si hay modal o diálogo de confirmación abierto.
        // La condición se captura una sola vez para garantizar que el
        // GuiUnlock() siempre balancee al GuiLock(), aunque el modal cambie
        // mientras se dibujan barra y panel.
        bool bloquearUI = (app->dialogConfirmar || app->modal != MODAL_NONE);
        if (bloquearUI)
            GuiLock();
        ToolbarAction tb = LayoutDrawToolbar(GetScreenWidth(),
                                             MallaGetRamoCount(&app->malla),
                                             app->modoConectar,
                                             app->conexionSelOrigenId !=
                                                 SIN_AREA_ID);
        PanelAreasDraw(app);

        // US-09 RF-01 (extendido): botón "Eliminar ramo" flotante en el lienzo,
        // habilitado cuando hay al menos un ramo seleccionado. Se oculta en
        // modo conectar (US-12) para no interferir con los clics de conexión.
        if (!app->modoConectar)
        {
            Rectangle btnEliminarRamo = LayoutEliminarRamoBtnRect();
            bool haySeleccion = (app->nRamosSeleccionados > 0);
            if (!haySeleccion) GuiDisable();
            bool pulsadoEliminar = GuiButton(btnEliminarRamo, "Eliminar ramo");
            if (!haySeleccion) GuiEnable();
            if (pulsadoEliminar) DialogsOpenEliminarRamo(app);
        }

        if (bloquearUI)
            GuiUnlock();

        HandleToolbarAction(app, tb);

        DrawConfirmar(app);
        DialogsDraw(app);

        DrawErrorGuardar(app);
        DrawErrorAbrir(app);
        DrawErrorExportar(app);
        DrawAviso(app);
    }

    EndDrawing();
}
