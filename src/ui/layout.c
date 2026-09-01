#include "layout.h"

#include <stddef.h>

#include "theme.h"

Rectangle LayoutToolbarRect(int screenWidth)
{
    return (Rectangle){ 0.0f, 0.0f, (float)screenWidth, (float)TOOLBAR_HEIGHT };
}

Rectangle LayoutCanvasRect(int screenWidth, int screenHeight)
{
    // El lienzo ocupa el espacio restante a la izquierda del panel lateral.
    return (Rectangle){ 0.0f, (float)TOOLBAR_HEIGHT,
                        (float)(screenWidth - PANEL_WIDTH),
                        (float)(screenHeight - TOOLBAR_HEIGHT) };
}

Rectangle LayoutPanelRect(int screenWidth, int screenHeight)
{
    return (Rectangle){ (float)(screenWidth - PANEL_WIDTH), (float)TOOLBAR_HEIGHT,
                        (float)PANEL_WIDTH,
                        (float)(screenHeight - TOOLBAR_HEIGHT) };
}

Rectangle LayoutEliminarRamoBtnRect(void)
{
    Rectangle canvas = LayoutCanvasRect(GetScreenWidth(), GetScreenHeight());
    return (Rectangle){ canvas.x + canvas.width - 128.0f, canvas.y + 10.0f,
                        118.0f, 30.0f };
}

static Rectangle ToolbarButtonRect(int index)
{
    const float btnW = 110.0f;
    const float btnH = 30.0f;
    const float gap = 8.0f;
    const float x = 8.0f + (float)index * (btnW + gap);
    return (Rectangle){ x, 5.0f, btnW, btnH };
}

ToolbarAction LayoutDrawToolbar(int screenWidth, int nRamos,
                                bool conectarActivo,
                                bool conexionSeleccionada)
{
    ToolbarAction action = TOOLBAR_NONE;

    GuiPanel(LayoutToolbarRect(screenWidth), NULL);

    if (GuiButton(ToolbarButtonRect(0), "Crear \xC3\xA1rea"))
        action = TOOLBAR_CREAR_AREA;

    if (GuiButton(ToolbarButtonRect(1), "Crear ramo"))
        action = TOOLBAR_CREAR_RAMO;

    // US-12: "Conectar" es un alternador que refleja el modo conectar activo.
    bool conectarEnabled = (nRamos >= 2);
    if (!conectarEnabled) GuiDisable();
    bool conectar = conectarActivo;
    if (GuiToggle(ToolbarButtonRect(2), "Conectar", &conectar))
        action = TOOLBAR_CONECTAR;
    if (!conectarEnabled) GuiEnable();

    // US-22 RF-15: los tooltips indican el atajo de cada botón (solo texto,
    // sin cambiar la interacción). GuiSetTooltip(NULL) evita que el texto se
    // arrastre a los botones siguientes (guiTooltipPtr no se auto-resetea).
    GuiSetTooltip("Guardar (Ctrl+S)");
    if (GuiButton(ToolbarButtonRect(3), "Guardar"))
        action = TOOLBAR_GUARDAR;
    GuiSetTooltip(NULL);

    GuiSetTooltip("Abrir (Ctrl+O)");
    if (GuiButton(ToolbarButtonRect(4), "Abrir"))
        action = TOOLBAR_ABRIR;
    GuiSetTooltip(NULL);

    GuiSetTooltip("Exportar imagen (Ctrl+E)");
    if (GuiButton(ToolbarButtonRect(5), "Exportar"))
        action = TOOLBAR_EXPORTAR;
    GuiSetTooltip(NULL);

    // US-13 RF-04: botón contextual, habilitado solo con una flecha
    // seleccionada (CE-10: deshabilitado si no la hay).
    Rectangle btnConexion = ToolbarButtonRect(6);
    btnConexion.width = 140.0f;
    if (!conexionSeleccionada) GuiDisable();
    if (GuiButton(btnConexion, "Eliminar conexi\xC3\xB3n"))
        action = TOOLBAR_ELIMINAR_CONEXION;
    if (!conexionSeleccionada) GuiEnable();

    // US-15 RF-06: botones de zoom "−"/"+" alineados a la derecha de la barra
    // (podrán reubicarse en US-20).
    const float zoomW = 44.0f;
    const float zoomH = 30.0f;
    const float zoomGap = 8.0f;
    const float zoomY = 5.0f;
    float zoomX = (float)screenWidth - zoomW * 2.0f - zoomGap - 8.0f;
    if (GuiButton((Rectangle){ zoomX, zoomY, zoomW, zoomH }, "-"))
        action = TOOLBAR_ZOOM_OUT;
    if (GuiButton((Rectangle){ zoomX + zoomW + zoomGap, zoomY, zoomW, zoomH }, "+"))
        action = TOOLBAR_ZOOM_IN;

    return action;
}

StartScreenAction LayoutDrawStartScreen(int screenWidth, int screenHeight)
{
    StartScreenAction action = START_NONE;

    const float panelW = 340.0f;
    const float panelH = 260.0f;
    Rectangle panel = { (screenWidth - panelW) * 0.5f, (screenHeight - panelH) * 0.5f,
                        panelW, panelH };

    // Título (GuiLabel usa la fuente cargada de la aplicación)
    Rectangle titleRec = { panel.x, panel.y - 55.0f, panel.width, 40.0f };
    GuiLabel(titleRec, "Visor de Mallas Universitarias");

    GuiPanel(panel, NULL);

    Rectangle nuevaBtn = { panel.x + 40.0f, panel.y + 50.0f, panel.width - 80.0f, 44.0f };
    Rectangle abrirBtn = { panel.x + 40.0f, panel.y + 120.0f, panel.width - 80.0f, 44.0f };

    if (GuiButton(nuevaBtn, "Nueva malla"))
        action = START_NUEVA_MALLA;

    if (GuiButton(abrirBtn, "Abrir malla"))
        action = START_ABRIR_MALLA;

    return action;
}
