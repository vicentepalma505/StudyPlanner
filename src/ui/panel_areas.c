#include "panel_areas.h"

#include <stddef.h>

#include "dialogs.h"
#include "layout.h"
#include "malla.h"
#include "theme.h"

static const float ROW_HEIGHT = 34.0f;
static const float BUTTON_BAND_H = 54.0f;   // banda inferior para botones
static const float DBL_CLICK_TIME = 0.35f;

void PanelAreasDraw(App *app)
{
    Rectangle panel = LayoutPanelRect(GetScreenWidth(), GetScreenHeight());

    // GuiPanel con texto dibuja una cabecera estilo statusbar (US-03 RF-01).
    GuiPanel(panel, "Areas");

    float headerH = (float)(GuiGetStyle(DEFAULT, TEXT_SIZE) +
                            2 * GuiGetStyle(STATUSBAR, TEXT_PADDING));
    float borderW = (float)GuiGetStyle(DEFAULT, BORDER_WIDTH);

    float innerX = panel.x + borderW;
    float innerY = panel.y + borderW + headerH;
    float innerW = panel.width - 2.0f * borderW;
    float listH = panel.height - 2.0f * borderW - headerH - BUTTON_BAND_H;

    int n = MallaGetAreaCount(&app->malla);

    // US-03 RF-05: lista con scroll cuando excede el alto del panel.
    Rectangle content = { innerX, innerY, innerW, n * ROW_HEIGHT };
    Rectangle view = { 0 };
    GuiScrollPanel((Rectangle){ innerX, innerY, innerW, listH }, NULL,
                   content, &app->panelScroll, &view);

    BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);
    for (int i = 0; i < n; i++)
    {
        const Area *area = MallaGetArea(&app->malla, i);
        float rowY = content.y + app->panelScroll.y + i * ROW_HEIGHT;
        bool sel = (area->id == app->areaSeleccionadaId);

        // US-03 RF-03: resaltado de la fila seleccionada.
        if (sel)
            DrawRectangle((int)innerX, (int)rowY, (int)innerW,
                          (int)ROW_HEIGHT, Fade(THEME_ACCENT, 0.12f));

        // US-03 RF-02: muestra de color + nombre por fila.
        DrawRectangle((int)(innerX + 4.0f), (int)(rowY + 6.0f), 22, 22,
                      area->color);
        DrawTextEx(app->font, area->nombre,
                   (Vector2){ innerX + 32.0f, rowY + 9.0f },
                   (float)GuiGetStyle(DEFAULT, TEXT_SIZE), 1.0f, DARKGRAY);

        if (sel)
            DrawRectangleLinesEx((Rectangle){ innerX, rowY, innerW, ROW_HEIGHT },
                                 2.0f, THEME_ACCENT);
    }
    EndScissorMode();

    // US-03 RF-06: estado vacío.
    if (n == 0)
    {
        GuiLabel((Rectangle){ innerX + 8.0f, innerY + 8.0f, innerW - 16.0f, 20.0f },
                 "No hay \xC3\xA1reas creadas");
    }

    // Selección con clic manual (US-03 RF-03/RF-04) y doble clic para editar
    // (US-04 RF-01). Bloqueado bajo modal.
    if (app->modal == MODAL_NONE && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 mp = GetMousePosition();

        if (CheckCollisionPointRec(mp, view))
        {
            // US-13 RF-02: al seleccionar un área se limpia la flecha
            // (selección única y exclusiva). Solo aquí: un clic sobre el
            // lienzo no debe tocar la selección de flecha.
            app->conexionSelOrigenId = SIN_AREA_ID;
            app->conexionSelDestinoId = SIN_AREA_ID;

            int i = (int)((mp.y - (content.y + app->panelScroll.y)) / ROW_HEIGHT);
            if (i >= 0 && i < n)
            {
                const Area *area = MallaGetArea(&app->malla, i);
                double now = GetTime();

                // Detección de doble clic sobre la misma fila (US-04 RF-01).
                static int s_lastRow = -1;
                static double s_lastClick = 0.0;
                bool doble = (i == s_lastRow && (now - s_lastClick) < DBL_CLICK_TIME);
                s_lastRow = i;
                s_lastClick = now;

                if (doble)
                {
                    app->areaSeleccionadaId = area->id;
                    DialogsOpenEditarArea(app);
                }
                else
                {
                    // RF-03/RF-04: clic en la fila seleccionada la deselecciona.
                    app->areaSeleccionadaId = (area->id == app->areaSeleccionadaId)
                                                  ? SIN_AREA_ID
                                                  : area->id;
                }
            }
        }
        else
        {
            // Clic en el espacio vacío del panel (sin tocar el scrollbar).
            bool inBand = (mp.x >= innerX && mp.x <= view.x + view.width &&
                           mp.y >= innerY && mp.y <= innerY + listH);
            if (inBand)
            {
                // RF-02: deseleccionar el área también limpia la flecha.
                app->areaSeleccionadaId = SIN_AREA_ID;
                app->conexionSelOrigenId = SIN_AREA_ID;
                app->conexionSelDestinoId = SIN_AREA_ID;
            }
        }
    }

    // US-05 RF-01: botón "Eliminar", habilitado con un área seleccionada.
    Rectangle delBtn = { innerX + 8.0f, innerY + listH + 10.0f,
                         innerW - 16.0f, 34.0f };
    bool haySeleccion = (app->areaSeleccionadaId != SIN_AREA_ID);
    if (!haySeleccion) GuiDisable();
    if (GuiButton(delBtn, "Eliminar"))
        DialogsOpenEliminarArea(app);
    if (!haySeleccion) GuiEnable();
}
