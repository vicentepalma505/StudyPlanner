// d_type (dirent) requiere las macros de feature de glibc, desactivadas por
// -std=c11 estricto (__STRICT_ANSI__).
#define _DEFAULT_SOURCE

#include "dialogs.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "export_malla.h"
#include "layout.h"
#include "malla.h"
#include "theme.h"
#include "validate.h"
#include "dialog_nativo.h"
#include "workspace.h"

// ---------------------------------------------------------------------------
// Estado del formulario de área (crear US-02 / editar US-04)
// ---------------------------------------------------------------------------

typedef struct {
    int id;                       // área en edición, SIN_AREA_ID al crear
    char nombre[AREA_NOMBRE_MAX + 1];
    char descripcion[AREA_DESCRIPCION_MAX + 1];
    Color color;
    char error[160];
    bool nombreEdit;              // GuiTextBox en modo edición
    bool descripcionEdit;
} AreaFormState;

static AreaFormState g_area;

// ---------------------------------------------------------------------------
// Estado del formulario de ramo (crear, US-06)
// ---------------------------------------------------------------------------

typedef struct {
    int id;                    // ramo en edición, SIN_AREA_ID al crear (US-08)
    char nombre[RAMO_NOMBRE_MAX + 1];
    char codigo[RAMO_CODIGO_MAX + 1];
    char creditos[8];          // texto de los campos numéricos (GuiTextBox)
    char semestre[8];
    char anio[8];
    char horas[8];
    Color color;
    int areaSeleccionada;          // índice del combo: 0 = "Sin área" (RF-07)
    bool prerrequisitos[MAX_RAMOS];// flags por índice de ramo (selección múltiple RF-08)
    Vector2 prerreqScroll;         // scroll de la lista de prerrequisitos
    char error[160];
    bool editNombre;
    bool editCodigo;
    bool editCreditos;
    bool editSemestre;
    bool editAnio;
    bool editHoras;
} RamoFormState;

static RamoFormState g_ramo;

// ---------------------------------------------------------------------------
// Estado del diálogo de guardado (US-16)
// ---------------------------------------------------------------------------

static char g_guardarRuta[1024];

// US-09 (extendido para selección múltiple): IDs de los ramos a eliminar con
// la confirmación actual; la lista se fija al abrir el modal.
static int g_eliminarRamosIds[MAX_RAMOS];
static int g_eliminarRamosN = 0;

// US-18: estado del "Buscar…" del diálogo de exportar. Se declara aquí (antes
// que el propio diálogo) porque DialogsShutdown y CloseModal, definidos antes,
// lo terminan para no dejar procesos huérfanos.
static DialogNativo g_exportarNativo;
static bool g_exportarAvisoSinDialogo;

static void CloseModal(App *app);   // definida más abajo

void DialogsOpenGuardar(App *app)
{
    app->modal = MODAL_GUARDAR;
    g_guardarRuta[0] = '\0';
}

// RF-04: si el nombre no incluye extensión se agrega ".malla". La extensión
// se decide por el basename (tras el último '/') para no confundir carpetas
// con puntos (CE-04/CE-05). El resultado se trunca si no cabe en 'tam'.
static void CompletarExtension(const char *texto, char *salida, int tam)
{
    const char *base = strrchr(texto, '/');
    base = (base != NULL) ? base + 1 : texto;
    size_t n = strlen(texto);
    if (n > (size_t)tam - 1)
        n = (size_t)tam - 1;
    memcpy(salida, texto, n);
    salida[n] = '\0';

    if (strrchr(base, '.') != NULL)
        return;

    int restante = tam - (int)strlen(salida) - 1;
    if (restante > 0)
        strncat(salida, ".malla", (size_t)restante);
}

// RF-02/RF-06/CE-06: acepta el nombre (con ruta opcional), completa la
// extensión y ejecuta el guardado; cancelar no guarda nada.
static void DrawGuardar(App *app)
{
    // Esc equivale a Cancelar (CE-06).
    if (IsKeyPressed(KEY_ESCAPE))
    {
        app->continuarTrasGuardar = false;
        app->accionPendiente = ACCION_NINGUNA;   // cancelar aborta la acción
        CloseModal(app);
        return;
    }

    Rectangle box = { (GetScreenWidth() - 560.0f) * 0.5f,
                      (GetScreenHeight() - 200.0f) * 0.5f,
                      560.0f, 200.0f };

    int btnActive = -1;
    int res = GuiTextInputBox(box, "Guardar malla",
                              "Nombre del archivo (extensi\xC3\xB3n .malla "
                              "autom\xC3\xA1tica):",
                              g_guardarRuta, (int)sizeof(g_guardarRuta),
                              "Guardar;Cancelar", &btnActive, NULL);
    if (res != 0)
    {
        if (btnActive == 1)
        {
            if (EsTextoVacio(g_guardarRuta))
            {
                app->dialogErrorGuardar = true;
                snprintf(app->errorGuardar, sizeof(app->errorGuardar),
                         "Debes indicar el nombre del archivo.");
            }
            else
            {
                char rutaFinal[1024];
                CompletarExtension(g_guardarRuta, rutaFinal,
                                   (int)sizeof(rutaFinal));
                bool ok = AppGuardarEnRuta(app, rutaFinal);
                if (ok && app->continuarTrasGuardar)
                {
                    // US-19 RF-08: tras guardar con éxito se cierra el modal y
                    // se ejecuta la acción pendiente (SALIR/ABRIR/CREAR_NUEVA)
                    // que originó la confirmación de 3 opciones.
                    CloseModal(app);
                    AppContinuarAccion(app);
                }
                else if (ok)
                {
                    CloseModal(app);
                }
                // Si el guardado falla se muestra el error (RF-11) y el modal
                // sigue abierto para reintentar o cancelar.
            }
        }
        else
        {
            app->continuarTrasGuardar = false;   // cancelar aborta la acción
            app->accionPendiente = ACCION_NINGUNA;
            CloseModal(app);   // Cancelar o botón X (CE-06)
        }
    }
}

// ---------------------------------------------------------------------------
// Diálogo de apertura (US-17 RF-04..RF-06, US-23 RF-01..RF-12)
// ---------------------------------------------------------------------------

#define ABRIR_MAX_ARCHIVOS 200

typedef struct {
    char nombres[ABRIR_MAX_ARCHIVOS][512];  // archivos .malla del directorio
    int n;
    int scroll;                             // scroll del GuiListView
    int activo;                             // elemento seleccionado en la lista
    char ruta[1024];                        // ruta manual / archivo elegido
    bool rutaEdit;
    char directorio[1024];                  // carpeta desde la que se lista (RF-10)
    DialogNativo nativo;                    // US-23: diálogo gráfico (RF-02)
    bool avisoSinDialogo;                   // US-23 RF-12: sin zenity/kdialog
} AbrirState;

static AbrirState g_abrir;

// Copia 'texto' en 'out' con recorte seguro (termina siempre con '\0').
static void CopiarTexto(char *out, int tam, const char *texto)
{
    int i = 0;
    while (i < tam - 1 && texto[i] != '\0')
    {
        out[i] = texto[i];
        i++;
    }
    out[i] = '\0';
}

// Construye "dir/nombre" con recorte seguro si no cabe. Con dir "." (o vacío)
// solo copia 'nombre'. Sin límites artificiales: si la ruta real excede el
// buffer, se recorta el final sin desbordar.
static void ConcatenarRuta(char *out, int tam, const char *dir,
                           const char *nombre)
{
    int i = 0;
    if (dir != NULL && dir[0] != '\0' && strcmp(dir, ".") != 0)
    {
        while (i < tam - 1 && dir[i] != '\0')
        {
            out[i] = dir[i];
            i++;
        }
        if (i < tam - 1)
            out[i++] = '/';
    }
    while (i < tam - 1 && *nombre != '\0')
    {
        out[i++] = *nombre;
        nombre++;
    }
    out[i] = '\0';
}

void DialogsOpenAbrir(App *app)
{
    // RF-04: la lista se reescanea cada vez que se abre el diálogo. RF-10: la
    // lista se genera desde el espacio de trabajo; sin él, desde el directorio
    // actual (CE-08).
    app->modal = MODAL_ABRIR;
    g_abrir.n = 0;
    g_abrir.scroll = 0;
    g_abrir.activo = -1;
    g_abrir.ruta[0] = '\0';
    g_abrir.rutaEdit = true;
    g_abrir.directorio[0] = '\0';
    g_abrir.avisoSinDialogo = false;
    DialogNativoTerminar(&g_abrir.nativo);

    const char *dir = (app->espacioTrabajo[0] != '\0')
                          ? app->espacioTrabajo : ".";
    DIR *d = opendir(dir);
    if (d == NULL)
    {
        // El espacio de trabajo ya no existe o no es legible → directorio
        // actual (RF-10).
        d = opendir(".");
        dir = ".";
    }
    if (d == NULL)
        return;   // sin directorio legible: solo se puede usar ruta manual

    snprintf(g_abrir.directorio, sizeof(g_abrir.directorio), "%s", dir);

    struct dirent *e;
    while ((e = readdir(d)) != NULL && g_abrir.n < ABRIR_MAX_ARCHIVOS)
    {
        if (e->d_type == DT_DIR)
            continue;
        size_t len = strlen(e->d_name);
        if (len < 7)   // ".malla" son 6 caracteres
            continue;
        if (strcmp(e->d_name + len - 6, ".malla") != 0)
            continue;
        snprintf(g_abrir.nombres[g_abrir.n],
                 sizeof(g_abrir.nombres[g_abrir.n]), "%s", e->d_name);
        g_abrir.n++;
    }
    closedir(d);
}

// Cierra el diálogo de abrir terminando el diálogo nativo si está abierto
// (US-23 CE-11: no se dejan procesos huérfanos).
static void CerrarAbrir(App *app)
{
    DialogNativoTerminar(&g_abrir.nativo);
    CloseModal(app);
}

// RF-06: dibuja y procesa el diálogo; Esc y X cancelan (CE-10).
static void DrawAbrir(App *app)
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        CerrarAbrir(app);
        return;
    }

    Rectangle box = { (GetScreenWidth() - 520.0f) * 0.5f,
                      (GetScreenHeight() - 380.0f) * 0.5f,
                      520.0f, 380.0f };

    if (GuiWindowBox(box, "Abrir malla") != 0)
    {
        CerrarAbrir(app);   // botón X (CE-06/CE-10)
        return;
    }

    char listaLabelText[512];
    if (g_abrir.directorio[0] != '\0' && strcmp(g_abrir.directorio, ".") != 0)
    {
        const char *base = strrchr(g_abrir.directorio, '/');
        base = (base != NULL) ? base + 1 : g_abrir.directorio;
        char baseRecortado[256];
        CopiarTexto(baseRecortado, (int)sizeof(baseRecortado), base);
        snprintf(listaLabelText, sizeof(listaLabelText),
                 "Archivos .malla del espacio de trabajo (%s):", baseRecortado);
    }
    else
    {
        snprintf(listaLabelText, sizeof(listaLabelText),
                 "Archivos .malla en el directorio de trabajo actual:");
    }

    Rectangle listaLabel = { box.x + 16.0f, box.y + 40.0f,
                             box.width - 32.0f, 18.0f };
    GuiLabel(listaLabel, listaLabelText);

    Rectangle lista = { box.x + 16.0f, box.y + 62.0f,
                        box.width - 32.0f, 160.0f };
    char *items[ABRIR_MAX_ARCHIVOS];
    for (int i = 0; i < g_abrir.n; i++)
        items[i] = g_abrir.nombres[i];

    // RNF-03: la lista es desplazable (GuiListViewEx).
    int res = GuiListViewEx(lista, (g_abrir.n > 0) ? items : NULL, g_abrir.n,
                            &g_abrir.scroll, &g_abrir.activo, NULL);
    if (res != 0 && g_abrir.activo >= 0 && g_abrir.activo < g_abrir.n)
    {
        // CE-04: elegir un archivo lo lleva al campo de ruta (fuente única).
        // La lista se genera desde el espacio de trabajo (RF-10): la ruta es
        // directorio + nombre para poder abrirla aunque el directorio actual
        // sea otro (CE-06: elegir de la lista no cambia el espacio de trabajo).
        if (g_abrir.directorio[0] != '\0' && strcmp(g_abrir.directorio, ".") != 0)
            ConcatenarRuta(g_abrir.ruta, (int)sizeof(g_abrir.ruta),
                           g_abrir.directorio, g_abrir.nombres[g_abrir.activo]);
        else
            CopiarTexto(g_abrir.ruta, (int)sizeof(g_abrir.ruta),
                        g_abrir.nombres[g_abrir.activo]);
    }

    Rectangle rutaLabel = { box.x + 16.0f, box.y + 240.0f,
                            box.width - 32.0f, 18.0f };
    GuiLabel(rutaLabel, "O escribe una ruta manual (.malla):");

    // US-23 RF-01: el botón "Buscar…" va a la derecha del campo, en la misma
    // fila.
    Rectangle buscarBtn = { box.x + box.width - 112.0f, box.y + 262.0f,
                            96.0f, 28.0f };
    Rectangle rutaBox = { box.x + 16.0f, box.y + 262.0f,
                          buscarBtn.x - box.x - 16.0f - 8.0f, 28.0f };
    if (GuiTextBox(rutaBox, g_abrir.ruta, (int)sizeof(g_abrir.ruta),
                   g_abrir.rutaEdit))
        g_abrir.rutaEdit = !g_abrir.rutaEdit;

    // US-23 RF-02/RF-03: el diálogo nativo se abre en el espacio de trabajo.
    if (GuiButton(buscarBtn, "Buscar…"))
        DialogNativoIniciar(&g_abrir.nativo, DNATIVO_ABRIR,
                            app->espacioTrabajo);

    Rectangle abrirBtn = { box.x + 16.0f, box.y + box.height - 48.0f,
                           120.0f, 32.0f };
    Rectangle cancelBtn = { box.x + box.width - 136.0f,
                            box.y + box.height - 48.0f, 120.0f, 32.0f };

    if (GuiButton(abrirBtn, "Abrir"))
    {
        if (EsTextoVacio(g_abrir.ruta))
        {
            app->dialogErrorAbrir = true;
            snprintf(app->errorAbrir, sizeof(app->errorAbrir),
                     "Elige un archivo de la lista o escribe una ruta.");
        }
        else
        {
            CerrarAbrir(app);
            AppAbrirEnRuta(app, g_abrir.ruta);
        }
    }

    if (GuiButton(cancelBtn, "Cancelar"))
        CerrarAbrir(app);

    // US-23 RF-07/RNF-01: se consulta el diálogo nativo cada fotograma sin
    // bloquear el bucle (waitpid WNOHANG).
    DialogNativoPoll(&g_abrir.nativo);
    if (g_abrir.nativo.estado == DNATIVO_SELECCION)
    {
        // RF-05: la ruta elegida se rellena en el campo; la carga la inicia el
        // usuario con "Abrir". RF-08/RF-09: la carpeta del archivo se vuelve
        // espacio de trabajo y se persiste.
        snprintf(g_abrir.ruta, sizeof(g_abrir.ruta), "%s",
                 g_abrir.nativo.resultado);
        g_abrir.rutaEdit = true;
        WorkspaceDirDeArchivo(g_abrir.nativo.resultado, app->espacioTrabajo,
                              (int)sizeof(app->espacioTrabajo));
        WorkspaceGuardar(app->espacioTrabajo);
        g_abrir.nativo.estado = DNATIVO_INACTIVO;
    }
    else if (g_abrir.nativo.estado == DNATIVO_NO_DISPONIBLE)
    {
        // RF-12/CE-04: sin zenity ni kdialog se avisa y el campo no cambia.
        g_abrir.avisoSinDialogo = true;
        g_abrir.nativo.estado = DNATIVO_INACTIVO;
    }
    else if (g_abrir.nativo.estado == DNATIVO_CANCELADO)
    {
        // RF-06/CE-01: cancelar deja el campo como estaba.
        g_abrir.nativo.estado = DNATIVO_INACTIVO;
    }

    if (g_abrir.avisoSinDialogo)
    {
        Rectangle mbox = { (GetScreenWidth() - 480.0f) * 0.5f,
                           (GetScreenHeight() - 150.0f) * 0.5f,
                           480.0f, 150.0f };
        int btnActive = -1;
        if (GuiMessageBox(mbox, "Aviso",
                          "No se encontró el selector de archivos del sistema "
                          "(zenity o kdialog). Escribe la ruta manualmente.",
                          "Aceptar", &btnActive) != 0)
            g_abrir.avisoSinDialogo = false;
    }
}

// US-23 CE-11: termina el diálogo nativo si sigue abierto (tanto el de "Abrir
// malla" como el de "Exportar imagen"). Se llama al cerrar la app
// (AppShutdown) para no dejar procesos huérfanos.
void DialogsShutdown(App *app)
{
    (void)app;
    DialogNativoTerminar(&g_abrir.nativo);
    DialogNativoTerminar(&g_exportarNativo);
}

// ---------------------------------------------------------------------------
// Diálogo de exportación de imagen (US-18 RF-02; US-22 RF-13 abre el atajo)
// ---------------------------------------------------------------------------

static char g_exportarRuta[1024];      // ruta del archivo a exportar
static int g_exportarFormato = 0;      // índice del combo: 0 = PNG, 1 = JPG
static bool g_exportarRutaEdit;        // GuiTextBox en modo edición

// Directorio por defecto de exportación: el elegido con "Buscar…" (persistido
// en export.cfg); si nunca se eligió, el mismo directorio por defecto de las
// mallas (espacio de trabajo); si tampoco, el directorio actual.
static const char *DirExportarBase(const App *app)
{
    if (app->espacioExportar[0] != '\0')
        return app->espacioExportar;
    if (app->espacioTrabajo[0] != '\0')
        return app->espacioTrabajo;
    return ".";
}

// Nombre sugerido para la imagen: el del archivo de malla actual (sin ".malla")
// o "malla" si la malla aún no tiene archivo asociado.
static void NombreSugeridoExportar(const App *app, char *out, int tam)
{
    const char *base = MallaGetRuta(&app->malla);
    const char *slash = strrchr(base, '/');
    if (slash != NULL)
        base = slash + 1;
    size_t n = strlen(base);
    if (n >= 6 && strcmp(base + n - 6, ".malla") == 0)
        n -= 6;
    if (n == 0)
    {
        snprintf(out, (size_t)tam, "malla");
        return;
    }
    if (n >= (size_t)tam)
        n = (size_t)tam - 1;
    memcpy(out, base, n);
    out[n] = '\0';
}

void DialogsOpenExportar(App *app)
{
    app->modal = MODAL_EXPORTAR;
    g_exportarFormato = 0;
    g_exportarRutaEdit = true;   // UX: el campo de ruta queda activo al abrir
    g_exportarAvisoSinDialogo = false;
    DialogNativoTerminar(&g_exportarNativo);

    // US-18 (agregado): la ruta por defecto apunta al mismo directorio en que
    // se guardan/abren las mallas (espacio de trabajo), salvo que el usuario
    // haya elegido otra con "Buscar…" (persistida entre sesiones).
    char sugerido[256];
    NombreSugeridoExportar(app, sugerido, (int)sizeof(sugerido));
    ConcatenarRuta(g_exportarRuta, (int)sizeof(g_exportarRuta),
                   DirExportarBase(app), sugerido);
}

// Cierra el diálogo de exportar terminando el diálogo nativo si está abierto
// (US-23 CE-11: no se dejan procesos huérfanos).
static void CerrarExportar(App *app)
{
    DialogNativoTerminar(&g_exportarNativo);
    CloseModal(app);
}

// RF-08: si el nombre no incluye extensión se agrega ".png" o ".jpg" según el
// formato elegido. La extensión se decide por el basename (tras el último '/')
// para no confundir carpetas con puntos (CE-05/CE-06). Si ya hay una extensión
// se respeta la escrita (CE-06).
static void CompletarExtensionExportar(const char *texto, char *salida, int tam,
                                       int formato)
{
    const char *base = strrchr(texto, '/');
    base = (base != NULL) ? base + 1 : texto;
    size_t n = strlen(texto);
    if (n > (size_t)tam - 1)
        n = (size_t)tam - 1;
    memcpy(salida, texto, n);
    salida[n] = '\0';

    if (strrchr(base, '.') != NULL)
        return;

    int restante = tam - (int)strlen(salida) - 1;
    if (restante > 0)
        strncat(salida, (formato == 0) ? ".png" : ".jpg", (size_t)restante);
}

// RF-02/RF-08/CE-07: Esc, X y "Cancelar" no exportan; "Exportar" valida el
// nombre, completa la extensión y genera la imagen fuera de pantalla. RF-10:
// ante fallo se muestra el error y la malla queda intacta (RF-09).
static void DrawExportar(App *app)
{
    // Esc equivale a Cancelar (CE-07).
    if (IsKeyPressed(KEY_ESCAPE))
    {
        CerrarExportar(app);
        return;
    }

    Rectangle box = { (GetScreenWidth() - 500.0f) * 0.5f,
                      (GetScreenHeight() - 210.0f) * 0.5f,
                      500.0f, 210.0f };

    if (GuiWindowBox(box, "Exportar imagen") != 0)
    {
        CerrarExportar(app);   // botón X equivale a Cancelar (CE-07)
        return;
    }

    float bx = box.x + 20.0f;
    float by = box.y + 36.0f;

    GuiLabel((Rectangle){ bx, by, box.width - 40.0f, 18.0f },
             "Ruta del archivo (extensi\xC3\xB3n autom\xC3\xA1tica):");
    by += 24.0f;

    // US-18 (agregado): "Buscar…" abre el gestor de archivos del sistema en el
    // directorio por defecto; la carpeta elegida pasa a ser la nueva default
    // persistida (mismo patrón que "Abrir malla").
    Rectangle buscarBtn = { box.x + box.width - 112.0f, by, 96.0f, 28.0f };
    Rectangle rutaBox = { bx, by, buscarBtn.x - bx - 8.0f, 28.0f };
    if (GuiTextBox(rutaBox, g_exportarRuta, (int)sizeof(g_exportarRuta),
                   g_exportarRutaEdit) == RESULT_PRESSED)
        g_exportarRutaEdit = !g_exportarRutaEdit;
    if (GuiButton(buscarBtn, "Buscar…"))
        DialogNativoIniciar(&g_exportarNativo, DNATIVO_GUARDAR,
                            DirExportarBase(app));
    by += 44.0f;

    GuiLabel((Rectangle){ bx, by, 80.0f, 20.0f }, "Formato");
    GuiComboBox((Rectangle){ bx + 90.0f, by, 200.0f, 28.0f }, "PNG;JPG",
                &g_exportarFormato);
    by += 44.0f;

    Rectangle exportBtn = { bx, box.y + box.height - 54.0f, 120.0f, 34.0f };
    Rectangle cancelBtn = { bx + 130.0f, box.y + box.height - 54.0f,
                            120.0f, 34.0f };

    if (GuiButton(exportBtn, "Exportar"))
    {
        if (EsTextoVacio(g_exportarRuta))
        {
            app->dialogErrorExportar = true;
            snprintf(app->errorExportar, sizeof(app->errorExportar),
                     "Debes indicar el nombre del archivo.");
        }
        else
        {
            char rutaFinal[1024];
            CompletarExtensionExportar(g_exportarRuta, rutaFinal,
                                       (int)sizeof(rutaFinal),
                                       g_exportarFormato);
            char err[256];
            if (ExportMallaImagen(&app->malla, app->font, rutaFinal,
                                  err, (int)sizeof(err)))
            {
                // RF-09: exportar no modifica los datos ni 'modificado'.
                CerrarExportar(app);
                AppShowAviso(app, "Imagen exportada.");
            }
            else
            {
                // RF-10: el diálogo sigue abierto para reintentar o cancelar.
                app->dialogErrorExportar = true;
                snprintf(app->errorExportar, sizeof(app->errorExportar),
                         "%s", err);
            }
        }
    }

    if (GuiButton(cancelBtn, "Cancelar"))
        CerrarExportar(app);

    // US-18 (agregado): el diálogo nativo se consulta cada fotograma sin
    // bloquear el bucle (mismo patrón que "Abrir malla", US-23).
    DialogNativoPoll(&g_exportarNativo);
    if (g_exportarNativo.estado == DNATIVO_SELECCION)
    {
        // La ruta elegida se rellena en el campo; la exportación la inicia el
        // usuario con "Exportar". La carpeta de esa ruta se vuelve la default
        // de exportación y se persiste (sobrevive al cierre de la app).
        snprintf(g_exportarRuta, sizeof(g_exportarRuta), "%s",
                 g_exportarNativo.resultado);
        g_exportarRutaEdit = true;
        WorkspaceDirDeArchivo(g_exportarNativo.resultado, app->espacioExportar,
                              (int)sizeof(app->espacioExportar));
        WorkspaceGuardarExportDir(app->espacioExportar);
        g_exportarNativo.estado = DNATIVO_INACTIVO;
    }
    else if (g_exportarNativo.estado == DNATIVO_NO_DISPONIBLE)
    {
        // Sin zenity ni kdialog se avisa y el campo no cambia (RF-12/CE-04).
        g_exportarAvisoSinDialogo = true;
        g_exportarNativo.estado = DNATIVO_INACTIVO;
    }
    else if (g_exportarNativo.estado == DNATIVO_CANCELADO)
    {
        // Cancelar deja el campo como estaba (RF-06/CE-01).
        g_exportarNativo.estado = DNATIVO_INACTIVO;
    }

    if (g_exportarAvisoSinDialogo)
    {
        Rectangle mbox = { (GetScreenWidth() - 480.0f) * 0.5f,
                           (GetScreenHeight() - 150.0f) * 0.5f,
                           480.0f, 150.0f };
        int btnActive = -1;
        if (GuiMessageBox(mbox, "Aviso",
                          "No se encontró el selector de archivos del sistema "
                          "(zenity o kdialog). Escribe la ruta manualmente.",
                          "Aceptar", &btnActive) != 0)
            g_exportarAvisoSinDialogo = false;
    }
}

static void ResetForm(int id)
{
    memset(&g_area, 0, sizeof(g_area));
    g_area.id = id;
    if (id == SIN_AREA_ID)
        g_area.color = COLOR_CELESTE;   // US-02 RF-04: color por defecto
}

void DialogsOpenCrearArea(App *app)
{
    app->modal = MODAL_CREAR_AREA;
    ResetForm(SIN_AREA_ID);
    g_area.nombreEdit = true;   // UX: el primer campo queda activo al abrir
}

void DialogsOpenEditarArea(App *app)
{
    const Area *area = MallaFindAreaById(&app->malla, app->areaSeleccionadaId);
    if (area == NULL)
        return;

    // US-04 RF-02: campos precargados con los valores actuales.
    app->modal = MODAL_EDITAR_AREA;
    ResetForm(area->id);
    memcpy(g_area.nombre, area->nombre, AREA_NOMBRE_MAX);
    g_area.nombre[AREA_NOMBRE_MAX] = '\0';
    memcpy(g_area.descripcion, area->descripcion, AREA_DESCRIPCION_MAX);
    g_area.descripcion[AREA_DESCRIPCION_MAX] = '\0';
    g_area.color = area->color;
    g_area.nombreEdit = true;   // UX: el primer campo queda activo al abrir
}

static void CloseModal(App *app)
{
    app->modal = MODAL_NONE;
    g_area.nombreEdit = false;
    g_area.descripcionEdit = false;
    g_ramo.editNombre = false;
    g_ramo.editCodigo = false;
    g_ramo.editCreditos = false;
    g_ramo.editSemestre = false;
    g_ramo.editAnio = false;
    g_ramo.editHoras = false;
    g_exportarRutaEdit = false;
    // US-18: si el diálogo nativo de "Buscar…" (exportar) sigue abierto se
    // termina al cerrar el modal (no se dejan procesos huérfanos, CE-11).
    DialogNativoTerminar(&g_exportarNativo);
    g_exportarAvisoSinDialogo = false;
}

// ---------------------------------------------------------------------------
// Modal "Crear ramo" (US-06)
// ---------------------------------------------------------------------------

void DialogsOpenCrearRamo(App *app)
{
    // US-06 RF-03: campos de texto vacíos, numéricos en 0, color gris neutro,
    // área en "Sin área" y prerrequisitos vacíos.
    app->modal = MODAL_CREAR_RAMO;
    memset(&g_ramo, 0, sizeof(g_ramo));
    g_ramo.id = SIN_AREA_ID;
    g_ramo.creditos[0] = '0'; g_ramo.creditos[1] = '\0';
    g_ramo.semestre[0] = '0'; g_ramo.semestre[1] = '\0';
    g_ramo.anio[0] = '0'; g_ramo.anio[1] = '\0';
    g_ramo.horas[0] = '0'; g_ramo.horas[1] = '\0';
    g_ramo.color = COLOR_RAMO_SIN_AREA;
    g_ramo.editNombre = true;   // UX: el primer campo queda activo al abrir
}

void DialogsOpenEditarRamo(App *app)
{
    const Ramo *ramo = MallaFindRamoById(&app->malla, app->ramoSeleccionadoId);
    if (ramo == NULL)
        return;

    // US-08 RF-02: campos precargados con los valores actuales.
    app->modal = MODAL_EDITAR_RAMO;
    memset(&g_ramo, 0, sizeof(g_ramo));
    g_ramo.id = ramo->id;
    memcpy(g_ramo.nombre, ramo->nombre, RAMO_NOMBRE_MAX);
    g_ramo.nombre[RAMO_NOMBRE_MAX] = '\0';
    memcpy(g_ramo.codigo, ramo->codigo, RAMO_CODIGO_MAX);
    g_ramo.codigo[RAMO_CODIGO_MAX] = '\0';
    snprintf(g_ramo.creditos, sizeof(g_ramo.creditos), "%d", ramo->creditos);
    snprintf(g_ramo.semestre, sizeof(g_ramo.semestre), "%d", ramo->semestre);
    snprintf(g_ramo.anio, sizeof(g_ramo.anio), "%d", ramo->anio);
    snprintf(g_ramo.horas, sizeof(g_ramo.horas), "%d", ramo->horas);
    g_ramo.color = ramo->color;

    // Área: índice 1-based del combo, o 0 = "Sin área" (US-07 RF-04).
    int nAreas = MallaGetAreaCount(&app->malla);
    for (int i = 0; i < nAreas; i++)
    {
        if (MallaGetArea(&app->malla, i)->id == ramo->areaId)
        {
            g_ramo.areaSeleccionada = i + 1;
            break;
        }
    }

    // Prerrequisitos (US-08 RF-02): flags por índice en la lista actual.
    for (int k = 0; k < ramo->nPrerrequisitos; k++)
    {
        for (int i = 0; i < MallaGetRamoCount(&app->malla); i++)
        {
            if (MallaGetRamo(&app->malla, i)->id == ramo->prerrequisitos[k])
            {
                g_ramo.prerrequisitos[i] = true;
                break;
            }
        }
    }

    g_ramo.editNombre = true;   // UX: el primer campo queda activo al abrir
}

// ---------------------------------------------------------------------------
// Validación y confirmación del formulario de área
// ---------------------------------------------------------------------------

// Valida el nombre: obligatorio (RF-05 US-02 / RF-04 US-04) y único sin
// distinguir mayúsculas, excluyendo a excluirId (US-04 CE-03).
static bool NombreAreaValido(App *app, int excluirId)
{
    if (EsTextoVacio(g_area.nombre))
    {
        snprintf(g_area.error, sizeof(g_area.error),
                 "El nombre es obligatorio.");
        return false;
    }

    int idx = MallaFindAreaIdxByName(&app->malla, g_area.nombre);
    if (idx >= 0 && app->malla.areas[idx].id != excluirId)
    {
        snprintf(g_area.error, sizeof(g_area.error),
                 "Ya existe un \xC3\xA1rea con ese nombre.");
        return false;
    }
    return true;
}

static void CrearArea(App *app)
{
    g_area.nombreEdit = false;
    g_area.descripcionEdit = false;

    if (!NombreAreaValido(app, SIN_AREA_ID))
        return;

    // US-02 RF-09: crea el área al final de la lista, sin seleccionarla.
    // US-22 RF-05: registrar antes de mutar; si no aplica se descarta.
    AppRegistrarUndo(app);
    if (MallaAddArea(&app->malla, g_area.nombre, g_area.color,
                     g_area.descripcion) < 0)
    {
        AppDescartarUndo();
        snprintf(g_area.error, sizeof(g_area.error),
                 "No se pueden crear m\xC3\xA1s \xC3\xA1reas.");
        return;
    }
    CloseModal(app);
}

static void GuardarArea(App *app)
{
    g_area.nombreEdit = false;
    g_area.descripcionEdit = false;

    if (!NombreAreaValido(app, g_area.id))
        return;

    // US-04 RF-05/RF-10: aplica cambios; el área permanece seleccionada.
    // US-22 RF-05: registrar antes de mutar; si no aplica se descarta.
    AppRegistrarUndo(app);
    if (!MallaUpdateArea(&app->malla, g_area.id, g_area.nombre, g_area.color,
                         g_area.descripcion))
        AppDescartarUndo();
    CloseModal(app);
}

// ---------------------------------------------------------------------------
// Formulario de área (crear / editar comparten layout)
// ---------------------------------------------------------------------------

static void DrawAreaForm(App *app, const char *title, const char *confirmLabel,
                         void (*confirmar)(App *app))
{
    Rectangle box = { (GetScreenWidth() - 520.0f) * 0.5f,
                      (GetScreenHeight() - 460.0f) * 0.5f,
                      520.0f, 460.0f };

    // US-02 RF-10 / US-04 RF-08: Esc cierra sin cambios.
    if (IsKeyPressed(KEY_ESCAPE))
    {
        CloseModal(app);
        return;
    }

    int result = GuiWindowBox(box, title);
    if (result == RESULT_PRESSED)
    {
        CloseModal(app);   // botón X equivale a Cancelar
        return;
    }

    float bx = box.x + 20.0f;
    float by = box.y + 30.0f;

    GuiLabel((Rectangle){ bx, by, 100.0f, 20.0f }, "Nombre");
    if (GuiTextBox((Rectangle){ bx + 100.0f, by, box.width - 120.0f, 28.0f },
                   g_area.nombre, sizeof(g_area.nombre),
                   g_area.nombreEdit) == RESULT_PRESSED)
        g_area.nombreEdit = !g_area.nombreEdit;

    by += 48.0f;
    GuiLabel((Rectangle){ bx, by, 100.0f, 20.0f }, "Color");
    GuiColorPicker((Rectangle){ bx + 100.0f, by, 160.0f, 150.0f }, NULL,
                   &g_area.color);

    // Vista previa del color en tiempo real (US-02 RF-04 / US-04 RF-02).
    Rectangle preview = { bx + 310.0f, by, 150.0f, 50.0f };
    DrawRectangle((int)preview.x, (int)preview.y, (int)preview.width,
                  (int)preview.height, g_area.color);
    DrawRectangleLinesEx(preview, 2.0f, THEME_GRIS_NEUTRO);
    GuiLabel((Rectangle){ preview.x, preview.y + preview.height + 4.0f,
                          preview.width, 20.0f },
             "Vista previa");

    by += 170.0f;
    GuiLabel((Rectangle){ bx, by, 100.0f, 20.0f }, "Descripci\xC3\xB3n");
    // Adaptación: raygui v5.0 no tiene GuiTextBoxMulti funcional; la
    // descripción usa un campo de una sola línea con límite de 200.
    if (GuiTextBox((Rectangle){ bx + 100.0f, by, box.width - 120.0f, 28.0f },
                   g_area.descripcion, sizeof(g_area.descripcion),
                   g_area.descripcionEdit) == RESULT_PRESSED)
        g_area.descripcionEdit = !g_area.descripcionEdit;

    // RNF-04: el error se muestra dentro del modal.
    if (g_area.error[0] != '\0')
    {
        DrawTextEx(app->font, g_area.error, (Vector2){ bx, by + 36.0f },
                   (float)GuiGetStyle(DEFAULT, TEXT_SIZE), 1.0f, RED);
    }

    // UX: Tab alterna el foco entre Nombre y Descripción (con vuelta).
    if (IsKeyPressed(KEY_TAB))
    {
        if (g_area.nombreEdit && !g_area.descripcionEdit)
        {
            g_area.nombreEdit = false;
            g_area.descripcionEdit = true;
        }
        else
        {
            g_area.nombreEdit = true;
            g_area.descripcionEdit = false;
        }
        return;
    }

    Rectangle confirmBtn = { bx, box.y + box.height - 54.0f, 130.0f, 34.0f };
    Rectangle cancelBtn = { bx + 140.0f, box.y + box.height - 54.0f,
                            130.0f, 34.0f };

    if (GuiButton(confirmBtn, confirmLabel))
    {
        confirmar(app);
        return;
    }

    if (GuiButton(cancelBtn, "Cancelar"))
    {
        CloseModal(app);
        return;
    }

    // US-02 RF-11 / US-04 RF-09: Enter equivale a confirmar. GuiTextBox
    // termina la edición con Enter (devuelve RESULT_PRESSED y desactiva el
    // flag), así que comprobamos los flags ya actualizados.
    if (IsKeyPressed(KEY_ENTER) &&
        !g_area.nombreEdit && !g_area.descripcionEdit)
        confirmar(app);
}

// ---------------------------------------------------------------------------
// Validación y confirmación del formulario de ramo (US-06 / US-08)
// ---------------------------------------------------------------------------

// Valida un campo numérico entero dentro de [min, max]; deja el error en el
// formulario y devuelve el valor parseado.
static bool NumeroValido(const char *texto, int min, int max, int *valor,
                         const char *mensaje)
{
    if (!EsNumeroEntero(texto))
    {
        snprintf(g_ramo.error, sizeof(g_ramo.error),
                 "%s debe ser un n\xC3\xBAmero entero.", mensaje);
        return false;
    }
    int v = atoi(texto);
    if (v < min || v > max)
    {
        snprintf(g_ramo.error, sizeof(g_ramo.error),
                 "%s debe estar entre %d y %d.", mensaje, min, max);
        return false;
    }
    *valor = v;
    return true;
}

// Valida los campos del formulario de ramo y rellena los valores parseados.
// excluirId excluye al propio ramo de la unicidad (US-08) y de la lista de
// prerrequisitos (nunca puede ser prerrequisito de sí mismo).
static bool ValidarDatosRamo(App *app, int excluirId, int *creditos,
                             int *semestre, int *anio, int *horas,
                             int *areaId, Color *color,
                             int prereqIds[MAX_RAMOS], int *nPre)
{
    // US-06 RF-04: nombre y código obligatorios y únicos (sin distinguir
    // mayúsculas/minúsculas).
    if (EsTextoVacio(g_ramo.nombre))
    {
        snprintf(g_ramo.error, sizeof(g_ramo.error),
                 "El nombre es obligatorio.");
        return false;
    }
    {
        int idx = MallaFindRamoIdxByName(&app->malla, g_ramo.nombre);
        if (idx >= 0 && app->malla.ramos[idx].id != excluirId)
        {
            snprintf(g_ramo.error, sizeof(g_ramo.error),
                     "Ya existe un ramo con ese nombre.");
            return false;
        }
    }
    if (EsTextoVacio(g_ramo.codigo))
    {
        snprintf(g_ramo.error, sizeof(g_ramo.error),
                 "El c\xC3\xB3" "digo es obligatorio.");
        return false;
    }
    {
        int idx = MallaFindRamoIdxByCodigo(&app->malla, g_ramo.codigo);
        if (idx >= 0 && app->malla.ramos[idx].id != excluirId)
        {
            snprintf(g_ramo.error, sizeof(g_ramo.error),
                     "Ya existe un ramo con ese c\xC3\xB3" "digo.");
            return false;
        }
    }

    // US-06 RF-05: rangos numéricos.
    if (!NumeroValido(g_ramo.creditos, 0, 30, creditos,
                      "Los cr\xC3\xA9" "ditos"))
        return false;
    if (!NumeroValido(g_ramo.semestre, 1, 12, semestre, "El semestre"))
        return false;
    if (!NumeroValido(g_ramo.anio, 1, 6, anio,
                      "El a\xC3\xB1o recomendado"))
        return false;
    if (!NumeroValido(g_ramo.horas, 0, 200, horas,
                      "Las horas directas"))
        return false;

    // US-07 RF-01/RF-03: área opcional; al asignarla, el color del ramo se
    // establece al color del área (sobrescribe el color propio).
    *areaId = SIN_AREA_ID;
    *color = g_ramo.color;
    int nAreas = MallaGetAreaCount(&app->malla);
    if (g_ramo.areaSeleccionada > 0 && g_ramo.areaSeleccionada <= nAreas)
    {
        const Area *a = MallaGetArea(&app->malla, g_ramo.areaSeleccionada - 1);
        *areaId = a->id;
        *color = a->color;
    }

    // US-06 RF-08: prerrequisitos como lista de IDs de ramos. En edición se
    // descarta el propio ramo (nunca puede ser su prerrequisito).
    *nPre = 0;
    for (int i = 0; i < MallaGetRamoCount(&app->malla) && *nPre < MAX_RAMOS; i++)
    {
        if (g_ramo.prerrequisitos[i])
        {
            const Ramo *r = MallaGetRamo(&app->malla, i);
            if (r->id != excluirId)
                prereqIds[(*nPre)++] = r->id;
        }
    }
    return true;
}

static void CrearRamo(App *app)
{
    g_ramo.editNombre = false;
    g_ramo.editCodigo = false;
    g_ramo.editCreditos = false;
    g_ramo.editSemestre = false;
    g_ramo.editAnio = false;
    g_ramo.editHoras = false;

    int creditos, semestre, anio, horas, areaId, nPre;
    int prereqIds[MAX_RAMOS];
    Color color;
    if (!ValidarDatosRamo(app, SIN_AREA_ID, &creditos, &semestre, &anio,
                          &horas, &areaId, &color, prereqIds, &nPre))
        return;

    // US-06 RF-09: posición inicial = centro del lienzo.
    Rectangle canvas = LayoutCanvasRect(GetScreenWidth(), GetScreenHeight());
    Vector2 centro = { canvas.x + canvas.width * 0.5f,
                       canvas.y + canvas.height * 0.5f };
    Vector2 pos = CameraScreenToWorld(app->camara, canvas, centro);

    // US-06 RF-09: registrar antes de mutar; si no aplica se descarta.
    // US-22 RF-05: una sola entrada por creación.
    AppRegistrarUndo(app);
    if (MallaAddRamo(&app->malla, g_ramo.nombre, g_ramo.codigo, creditos,
                     semestre, anio, horas, color, areaId, pos,
                     prereqIds, nPre) < 0)
    {
        AppDescartarUndo();
        snprintf(g_ramo.error, sizeof(g_ramo.error),
                 "No se pueden crear m\xC3\xA1s ramos.");
        return;
    }
    CloseModal(app);
}

static void GuardarRamo(App *app)
{
    g_ramo.editNombre = false;
    g_ramo.editCodigo = false;
    g_ramo.editCreditos = false;
    g_ramo.editSemestre = false;
    g_ramo.editAnio = false;
    g_ramo.editHoras = false;

    int creditos, semestre, anio, horas, areaId, nPre;
    int prereqIds[MAX_RAMOS];
    Color color;
    if (!ValidarDatosRamo(app, g_ramo.id, &creditos, &semestre, &anio,
                          &horas, &areaId, &color, prereqIds, &nPre))
        return;

    // US-08 RF-12: la posición no se modifica al editar.
    // US-22 RF-05: registrar antes de mutar; si no aplica se descarta.
    AppRegistrarUndo(app);
    if (!MallaUpdateRamo(&app->malla, g_ramo.id, g_ramo.nombre, g_ramo.codigo,
                         creditos, semestre, anio, horas, color, areaId,
                         prereqIds, nPre))
        AppDescartarUndo();
    CloseModal(app);
}

// ---------------------------------------------------------------------------
// Formulario de ramo (crear US-06 / editar US-08): 9 campos (RF-02)
// ---------------------------------------------------------------------------

// UX: Tab alterna el foco entre los 6 campos de texto (con vuelta).
static void TabSiguienteCampoRamo(void)
{
    bool *campos[] = {
        &g_ramo.editNombre, &g_ramo.editCodigo,
        &g_ramo.editCreditos, &g_ramo.editSemestre,
        &g_ramo.editAnio, &g_ramo.editHoras,
    };
    const int n = (int)(sizeof(campos) / sizeof(campos[0]));

    int actual = -1;
    for (int i = 0; i < n; i++)
    {
        if (*campos[i])
        {
            actual = i;
            break;
        }
    }

    if (actual < 0)
        *campos[0] = true;
    else
    {
        *campos[actual] = false;
        *campos[(actual + 1) % n] = true;
    }
}

static void DrawRamoForm(App *app, const char *title, const char *confirmLabel,
                         void (*confirmar)(App *app))
{
    Rectangle box = { (GetScreenWidth() - 640.0f) * 0.5f,
                      (GetScreenHeight() - 600.0f) * 0.5f,
                      640.0f, 600.0f };

    // US-06 RF-10: Esc cierra sin crear/guardar.
    if (IsKeyPressed(KEY_ESCAPE))
    {
        CloseModal(app);
        return;
    }

    int result = GuiWindowBox(box, title);
    if (result == RESULT_PRESSED)
    {
        CloseModal(app);   // botón X equivale a Cancelar
        return;
    }

    float bx = box.x + 20.0f;
    float by = box.y + 26.0f;
    float fieldW = box.width - 120.0f;

    // Nombre
    GuiLabel((Rectangle){ bx, by, 100.0f, 20.0f }, "Nombre");
    if (GuiTextBox((Rectangle){ bx + 100.0f, by, fieldW, 28.0f },
                   g_ramo.nombre, sizeof(g_ramo.nombre),
                   g_ramo.editNombre) == RESULT_PRESSED)
        g_ramo.editNombre = !g_ramo.editNombre;
    by += 40.0f;

    // Código / Sigla
    GuiLabel((Rectangle){ bx, by, 110.0f, 20.0f }, "C\xC3\xB3" "digo/Sigla");
    if (GuiTextBox((Rectangle){ bx + 110.0f, by, fieldW, 28.0f },
                   g_ramo.codigo, sizeof(g_ramo.codigo),
                   g_ramo.editCodigo) == RESULT_PRESSED)
        g_ramo.editCodigo = !g_ramo.editCodigo;
    by += 40.0f;

    // Créditos (SCT) + Semestre
    GuiLabel((Rectangle){ bx, by, 150.0f, 20.0f }, "Cr\xC3\xA9" "ditos (SCT)");
    if (GuiTextBox((Rectangle){ bx + 160.0f, by, 70.0f, 28.0f },
                   g_ramo.creditos, sizeof(g_ramo.creditos),
                   g_ramo.editCreditos) == RESULT_PRESSED)
        g_ramo.editCreditos = !g_ramo.editCreditos;
    GuiLabel((Rectangle){ bx + 260.0f, by, 90.0f, 20.0f }, "Semestre");
    if (GuiTextBox((Rectangle){ bx + 360.0f, by, 70.0f, 28.0f },
                   g_ramo.semestre, sizeof(g_ramo.semestre),
                   g_ramo.editSemestre) == RESULT_PRESSED)
        g_ramo.editSemestre = !g_ramo.editSemestre;
    by += 40.0f;

    // Año recomendado + Horas directas
    GuiLabel((Rectangle){ bx, by, 150.0f, 20.0f }, "A\xC3\xB1o recomendado");
    if (GuiTextBox((Rectangle){ bx + 160.0f, by, 70.0f, 28.0f },
                   g_ramo.anio, sizeof(g_ramo.anio),
                   g_ramo.editAnio) == RESULT_PRESSED)
        g_ramo.editAnio = !g_ramo.editAnio;
    GuiLabel((Rectangle){ bx + 260.0f, by, 110.0f, 20.0f },
             "Horas directas");
    if (GuiTextBox((Rectangle){ bx + 380.0f, by, 70.0f, 28.0f },
                   g_ramo.horas, sizeof(g_ramo.horas),
                   g_ramo.editHoras) == RESULT_PRESSED)
        g_ramo.editHoras = !g_ramo.editHoras;
    by += 40.0f;

    // Color (selector con vista previa; RF-02/RF-06)
    GuiLabel((Rectangle){ bx, by, 100.0f, 20.0f }, "Color");
    GuiColorPicker((Rectangle){ bx + 100.0f, by, 140.0f, 120.0f }, NULL,
                   &g_ramo.color);
    Rectangle preview = { bx + 280.0f, by, 150.0f, 60.0f };
    DrawRectangle((int)preview.x, (int)preview.y, (int)preview.width,
                  (int)preview.height, g_ramo.color);
    DrawRectangleLinesEx(preview, 2.0f, THEME_GRIS_NEUTRO);
    GuiLabel((Rectangle){ preview.x, preview.y + preview.height + 4.0f,
                          preview.width, 20.0f }, "Vista previa");
    by += 132.0f;

    // Área del curso (RF-07: opcional; GuiComboBox cíclico en raygui v5.0)
    GuiLabel((Rectangle){ bx, by, 110.0f, 20.0f }, "\xC3\x81rea del curso");
    static char comboAreas[5120];
    {
        char *p = comboAreas;
        p += sprintf(p, "Sin \xC3\xA1rea");
        int nAreas = MallaGetAreaCount(&app->malla);
        for (int i = 0; i < nAreas; i++)
        {
            const Area *a = MallaGetArea(&app->malla, i);
            p += sprintf(p, ";%s", a->nombre);
        }
    }
    int prevSel = g_ramo.areaSeleccionada;
    GuiComboBox((Rectangle){ bx + 120.0f, by, fieldW, 28.0f }, comboAreas,
                &g_ramo.areaSeleccionada);
    // US-06 RF-06: al seleccionar un área, el color se precarga con el suyo.
    // US-06 RF-07: al volver a "Sin área", el color vuelve al gris neutro.
    if (g_ramo.areaSeleccionada != prevSel)
    {
        if (g_ramo.areaSeleccionada > 0)
        {
            int nAreas = MallaGetAreaCount(&app->malla);
            if (g_ramo.areaSeleccionada <= nAreas)
            {
                const Area *a = MallaGetArea(&app->malla,
                                             g_ramo.areaSeleccionada - 1);
                g_ramo.color = a->color;
            }
        }
        else
        {
            g_ramo.color = COLOR_RAMO_SIN_AREA;
        }
    }
    by += 40.0f;

    // Ramos prerrequisitos (selección múltiple RF-08; lista desplazable RNF-02)
    GuiLabel((Rectangle){ bx, by, 220.0f, 20.0f }, "Ramos prerrequisitos");
    by += 24.0f;

    const float rowH = 26.0f;
    int nRamos = MallaGetRamoCount(&app->malla);
    Rectangle listBox = { bx, by, box.width - 40.0f, 130.0f };
    Rectangle content = { listBox.x, listBox.y, listBox.width, nRamos * rowH };
    Rectangle view = { 0 };
    GuiScrollPanel(listBox, NULL, content, &g_ramo.prerreqScroll, &view);

    BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);
    bool esEdicion = (g_ramo.id != SIN_AREA_ID);
    for (int i = 0; i < nRamos; i++)
    {
        const Ramo *r = MallaGetRamo(&app->malla, i);
        bool esSelf = esEdicion && (r->id == g_ramo.id);
        float rowY = content.y + g_ramo.prerreqScroll.y + i * rowH;
        Rectangle row = { content.x, rowY, content.width, rowH };

        if (g_ramo.prerrequisitos[i])
            DrawRectangle((int)row.x, (int)row.y, (int)row.width,
                          (int)row.height, Fade(THEME_ACCENT, 0.12f));

        Rectangle cb = { row.x + 8.0f, row.y + (rowH - 14.0f) * 0.5f,
                         14.0f, 14.0f };
        DrawRectangle((int)cb.x, (int)cb.y, (int)cb.width, (int)cb.height,
                      g_ramo.prerrequisitos[i] ? THEME_ACCENT : WHITE);
        DrawRectangleLinesEx(cb, 1.0f, DARKGRAY);

        // US-08 RF-06: el propio ramo no puede ser su prerrequisito; se
        // muestra deshabilitado.
        Color texto = esSelf ? LIGHTGRAY : DARKGRAY;
        DrawTextEx(app->font, r->nombre,
                   (Vector2){ row.x + 30.0f, row.y + (rowH - 16.0f) * 0.5f },
                   (float)GuiGetStyle(DEFAULT, TEXT_SIZE), 1.0f, texto);
    }
    EndScissorMode();

    // Clic en la lista alterna la selección (RF-08: selección múltiple).
    // US-08 RF-06: se rechazan las relaciones que crearían un ciclo.
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        CheckCollisionPointRec(GetMousePosition(), view))
    {
        Vector2 mp = GetMousePosition();
        int i = (int)((mp.y - (content.y + g_ramo.prerreqScroll.y)) / rowH);
        if (i >= 0 && i < nRamos && !(esEdicion &&
            MallaGetRamo(&app->malla, i)->id == g_ramo.id))
        {
            const Ramo *r = MallaGetRamo(&app->malla, i);
            if (g_ramo.prerrequisitos[i] ||
                !MallaCreaCiclo(&app->malla, g_ramo.id, r->id))
            {
                g_ramo.prerrequisitos[i] = !g_ramo.prerrequisitos[i];
            }
            else
            {
                snprintf(g_ramo.error, sizeof(g_ramo.error),
                         "Esa relaci\xC3\xB3n crear\xC3\xAD"
                         "a un ciclo de prerrequisitos.");
            }
        }
    }

    // CE-05: sin ramos previos la lista está vacía.
    if (nRamos == 0)
        GuiLabel((Rectangle){ listBox.x + 8.0f, listBox.y + 6.0f,
                              listBox.width - 16.0f, 20.0f },
                 "No hay ramos creados");

    by += 154.0f;

    // RNF-04: el error se muestra dentro del modal.
    if (g_ramo.error[0] != '\0')
    {
        DrawTextEx(app->font, g_ramo.error, (Vector2){ bx, by + 4.0f },
                   (float)GuiGetStyle(DEFAULT, TEXT_SIZE), 1.0f, RED);
    }

    // UX: Tab alterna el foco entre los campos de texto.
    if (IsKeyPressed(KEY_TAB))
    {
        TabSiguienteCampoRamo();
        return;
    }

    Rectangle confirmBtn = { bx, box.y + box.height - 54.0f, 130.0f, 34.0f };
    Rectangle cancelBtn = { bx + 140.0f, box.y + box.height - 54.0f,
                            130.0f, 34.0f };

    if (GuiButton(confirmBtn, confirmLabel))
    {
        confirmar(app);
        return;
    }

    if (GuiButton(cancelBtn, "Cancelar"))
    {
        CloseModal(app);
        return;
    }

    // US-06 RF-10: Enter equivale a confirmar (solo si ningún campo de texto
    // está en edición; GuiTextBox termina la edición con Enter).
    if (IsKeyPressed(KEY_ENTER) &&
        !g_ramo.editNombre && !g_ramo.editCodigo &&
        !g_ramo.editCreditos && !g_ramo.editSemestre &&
        !g_ramo.editAnio && !g_ramo.editHoras)
        confirmar(app);
}

// ---------------------------------------------------------------------------
// Eliminación de área (US-05)
// ---------------------------------------------------------------------------

void DialogsOpenEliminarArea(App *app)
{
    if (app->areaSeleccionadaId == SIN_AREA_ID)
        return;
    if (MallaFindAreaById(&app->malla, app->areaSeleccionadaId) == NULL)
        return;
    app->modal = MODAL_CONFIRMAR_ELIMINAR;
}

static void EliminarAreaConfirmada(App *app)
{
    // US-05 RF-04: elimina, desasigna ramos, limpia la selección.
    // US-22 RF-05: registrar antes de mutar.
    AppRegistrarUndo(app);
    MallaRemoveArea(&app->malla, app->areaSeleccionadaId);
    app->areaSeleccionadaId = SIN_AREA_ID;
    CloseModal(app);
}

// US-05 RF-02: "¿Eliminar el área 'X'?" con Eliminar/Cancelar.
static void DrawEliminarArea(App *app)
{
    const Area *area = MallaFindAreaById(&app->malla, app->areaSeleccionadaId);
    if (area == NULL)
    {
        CloseModal(app);
        return;
    }

    // Esc equivale a Cancelar (RF-02).
    if (IsKeyPressed(KEY_ESCAPE))
    {
        CloseModal(app);
        return;
    }

    char msg[256];
    snprintf(msg, sizeof(msg),
             "\xC2\xBF"
             "Eliminar el \xC3\xA1rea '%s'?",
             area->nombre);

    Rectangle box = { (GetScreenWidth() - 460.0f) * 0.5f,
                      (GetScreenHeight() - 160.0f) * 0.5f,
                      460.0f, 160.0f };

    int btnActive = -1;
    if (GuiMessageBox(box, "Confirmar", msg, "Eliminar;Cancelar", &btnActive) != 0)
    {
        if (btnActive == 1)
        {
            if (MallaCountRamosOfArea(&app->malla, area->id) > 0)
                app->modal = MODAL_CONFIRMAR_ELIMINAR_RAMOS;   // RF-03
            else
                EliminarAreaConfirmada(app);                   // RF-04
        }
        else
        {
            CloseModal(app);   // Cancelar o X
        }
    }
}

// US-05 RF-03: confirmación adicional cuando el área tiene ramos.
static void DrawEliminarAreaConRamos(App *app)
{
    // Esc equivale a Cancelar (RF-03).
    if (IsKeyPressed(KEY_ESCAPE))
    {
        CloseModal(app);
        return;
    }

    int n = MallaCountRamosOfArea(&app->malla, app->areaSeleccionadaId);
    char msg[320];
    snprintf(msg, sizeof(msg),
             "El \xC3\xA1rea tiene %d ramos asignados. Al eliminar el \xC3\xA1rea, "
             "estos ramos quedar\xC3\xA1n sin \xC3\xA1rea y con color neutro. "
             "\xC2\xBF"
             "Continuar?",
             n);

    Rectangle box = { (GetScreenWidth() - 520.0f) * 0.5f,
                      (GetScreenHeight() - 160.0f) * 0.5f,
                      520.0f, 160.0f };

    int btnActive = -1;
    if (GuiMessageBox(box, "Confirmar", msg, "Continuar;Cancelar", &btnActive) != 0)
    {
        if (btnActive == 1)
            EliminarAreaConfirmada(app);
        else
            CloseModal(app);
    }
}

// ---------------------------------------------------------------------------
// Eliminación de ramo (US-09)
// ---------------------------------------------------------------------------

void DialogsOpenEliminarRamo(App *app)
{
    g_eliminarRamosN = 0;

    // Selección múltiple (extensión del usuario): se eliminan todos los ramos
    // seleccionados con una sola confirmación y una sola entrada de deshacer.
    if (app->nRamosSeleccionados > 1)
    {
        for (int i = 0; i < app->nRamosSeleccionados; i++)
        {
            if (MallaFindRamoById(&app->malla,
                                  app->ramoSeleccionados[i]) != NULL)
                g_eliminarRamosIds[g_eliminarRamosN++] =
                    app->ramoSeleccionados[i];
        }
    }
    else
    {
        if (app->ramoSeleccionadoId == SIN_AREA_ID)
            return;
        if (MallaFindRamoById(&app->malla, app->ramoSeleccionadoId) == NULL)
            return;
        g_eliminarRamosIds[g_eliminarRamosN++] = app->ramoSeleccionadoId;
    }

    if (g_eliminarRamosN == 0)
        return;
    app->modal = MODAL_CONFIRMAR_ELIMINAR_RAMO;
}

static void EliminarRamoConfirmada(App *app)
{
    // US-09 RF-04: elimina el/los ramo(s) (y sus relaciones) y limpia la
    // selección. US-22 RF-05: una sola entrada de deshacer para la operación.
    AppRegistrarUndo(app);
    for (int i = 0; i < g_eliminarRamosN; i++)
        MallaRemoveRamo(&app->malla, g_eliminarRamosIds[i]);
    AppClearSelection(app);
    g_eliminarRamosN = 0;
    CloseModal(app);
}

// US-09 RF-03: avisa de las flechas de prerrequisito antes de eliminar.
static void DrawEliminarRamo(App *app)
{
    // Esc equivale a Cancelar (RF-02).
    if (IsKeyPressed(KEY_ESCAPE))
    {
        CloseModal(app);
        return;
    }

    if (g_eliminarRamosN == 0)
    {
        CloseModal(app);
        return;
    }

    char msg[400];
    if (g_eliminarRamosN == 1)
    {
        const Ramo *ramo = MallaFindRamoById(&app->malla, g_eliminarRamosIds[0]);
        if (ramo == NULL)
        {
            CloseModal(app);
            return;
        }
        int nFlechas = MallaCountFlechasDeRamo(&app->malla, ramo->id);
        if (nFlechas > 0)
        {
            snprintf(msg, sizeof(msg),
                     "El ramo '%s' tiene %d flecha(s) de prerrequisito "
                     "asociada(s). Al eliminarlo se descartar\xC3\xA1n esas "
                     "relaciones. \xC2\xBF"
                     "Eliminar?",
                     ramo->nombre, nFlechas);
        }
        else
        {
            snprintf(msg, sizeof(msg),
                     "\xC2\xBF"
                     "Eliminar el ramo '%s'?",
                     ramo->nombre);
        }
    }
    else
    {
        int nFlechas = 0;
        for (int i = 0; i < g_eliminarRamosN; i++)
            nFlechas += MallaCountFlechasDeRamo(&app->malla,
                                                g_eliminarRamosIds[i]);
        if (nFlechas > 0)
        {
            snprintf(msg, sizeof(msg),
                     "\xC2\xBF"
                     "Eliminar los %d ramos seleccionados? Se descartar\xC3\xA1n "
                     "sus flechas de prerrequisito asociadas.",
                     g_eliminarRamosN);
        }
        else
        {
            snprintf(msg, sizeof(msg),
                     "\xC2\xBF"
                     "Eliminar los %d ramos seleccionados?",
                     g_eliminarRamosN);
        }
    }

    Rectangle box = { (GetScreenWidth() - 520.0f) * 0.5f,
                      (GetScreenHeight() - 160.0f) * 0.5f,
                      520.0f, 160.0f };

    int btnActive = -1;
    if (GuiMessageBox(box, "Confirmar", msg, "Eliminar;Cancelar", &btnActive) != 0)
    {
        if (btnActive == 1)
            EliminarRamoConfirmada(app);
        else
            CloseModal(app);   // Cancelar o X
    }
}

// ---------------------------------------------------------------------------
// Dispatcher
// ---------------------------------------------------------------------------

void DialogsDraw(App *app)
{
    switch (app->modal)
    {
        case MODAL_CREAR_AREA:
            DrawAreaForm(app, "Crear \xC3\xA1rea", "Crear", CrearArea);
            break;
        case MODAL_EDITAR_AREA:
            DrawAreaForm(app, "Editar \xC3\xA1rea", "Guardar", GuardarArea);
            break;
        case MODAL_CONFIRMAR_ELIMINAR:
            DrawEliminarArea(app);
            break;
        case MODAL_CONFIRMAR_ELIMINAR_RAMOS:
            DrawEliminarAreaConRamos(app);
            break;
        case MODAL_CREAR_RAMO:
            DrawRamoForm(app, "Crear ramo", "Crear", CrearRamo);
            break;
        case MODAL_EDITAR_RAMO:
            DrawRamoForm(app, "Editar ramo", "Guardar", GuardarRamo);
            break;
        case MODAL_CONFIRMAR_ELIMINAR_RAMO:
            DrawEliminarRamo(app);
            break;
        case MODAL_GUARDAR:
            DrawGuardar(app);
            break;
        case MODAL_ABRIR:
            DrawAbrir(app);
            break;
        case MODAL_EXPORTAR:
            DrawExportar(app);
            break;
        case MODAL_NONE:
        default:
            break;
    }
}
