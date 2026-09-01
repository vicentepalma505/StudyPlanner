#ifndef LAYOUT_H
#define LAYOUT_H

#include "raylib.h"
#include "raygui.h"

typedef enum {
    TOOLBAR_NONE = 0,
    TOOLBAR_CREAR_AREA,
    TOOLBAR_CREAR_RAMO,
    TOOLBAR_CONECTAR,
    TOOLBAR_GUARDAR,
    TOOLBAR_ABRIR,              // US-17 RF-01
    TOOLBAR_EXPORTAR,
    TOOLBAR_ELIMINAR_CONEXION,  // US-13 RF-04 (contextual)
    TOOLBAR_ZOOM_IN,            // US-15 RF-06
    TOOLBAR_ZOOM_OUT,           // US-15 RF-06
} ToolbarAction;

typedef enum {
    START_NONE = 0,
    START_NUEVA_MALLA,
    START_ABRIR_MALLA,
} StartScreenAction;

Rectangle LayoutToolbarRect(int screenWidth);
Rectangle LayoutCanvasRect(int screenWidth, int screenHeight);
Rectangle LayoutPanelRect(int screenWidth, int screenHeight);

// Botón "Eliminar ramo" flotante en la esquina superior derecha del lienzo
// (US-09 RF-01).
Rectangle LayoutEliminarRamoBtnRect(void);

// Dibuja la barra de herramientas (US-01 RF-05/RF-06) y devuelve la acción
// pulsada. 'conectarActivo' refleja el modo conectar (US-12): el botón
// "Conectar" se muestra como alternador presionado. 'conexionSeleccionada'
// habilita el botón contextual "Eliminar conexión" (US-13 RF-04).
ToolbarAction LayoutDrawToolbar(int screenWidth, int nRamos,
                                bool conectarActivo,
                                bool conexionSeleccionada);

// Dibuja la pantalla inicial (US-01 RF-01) y devuelve la acción pulsada.
StartScreenAction LayoutDrawStartScreen(int screenWidth, int screenHeight);

#endif
