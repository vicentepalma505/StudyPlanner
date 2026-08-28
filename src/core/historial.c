#include "historial.h"

#include <string.h>

#define HISTORIAL_LIMITE 50

static Malla s_undo[HISTORIAL_LIMITE];
static int s_undoTop = -1;
static Malla s_redo[HISTORIAL_LIMITE];
static int s_redoTop = -1;

void HistorialRegistrar(Malla estado)
{
    // RF-04: al superar el límite se desplaza y descarta la más antigua.
    if (s_undoTop == HISTORIAL_LIMITE - 1)
    {
        memmove(&s_undo[0], &s_undo[1],
                sizeof(s_undo[0]) * (HISTORIAL_LIMITE - 1));
        s_undoTop = HISTORIAL_LIMITE - 2;
    }

    s_undo[++s_undoTop] = estado;

    // CE-04: una acción nueva invalida el rehacer.
    s_redoTop = -1;
}

bool HistorialDeshacer(Malla *malla)
{
    if (s_undoTop < 0)
        return false;   // CE-01/CE-02

    s_redo[++s_redoTop] = *malla;
    *malla = s_undo[s_undoTop--];
    return true;
}

bool HistorialRehacer(Malla *malla)
{
    if (s_redoTop < 0)
        return false;   // CE-03

    s_undo[++s_undoTop] = *malla;
    *malla = s_redo[s_redoTop--];
    return true;
}

bool HistorialHayDeshacer(void)
{
    return s_undoTop >= 0;
}

bool HistorialHayRehacer(void)
{
    return s_redoTop >= 0;
}

void HistorialDescartar(void)
{
    if (s_undoTop >= 0)
        s_undoTop--;
}

void HistorialLimpiar(void)
{
    s_undoTop = -1;
    s_redoTop = -1;
}
