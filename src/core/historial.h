#ifndef HISTORIAL_H
#define HISTORIAL_H

#include "malla.h"

// US-22: historial de deshacer/rehacer por instantáneas (RF-04). La struct
// Malla es de tamaño fijo sin punteros internos → copia por valor = copia
// profunda (RNF-01/RNF-04). El límite es 50 acciones: al superarlo se descarta
// la más antigua. Las pilas son estáticas del módulo (≈ 4 MB en memoria
// estática, no en la pila).

// Registra 'estado' (el estado ANTERIOR a una mutación) y descarta el historial
// de rehacer (CE-04). Se llama ANTES de ejecutar la mutación (RF-05).
void HistorialRegistrar(Malla estado);

// Deshace la última acción: restaura en '*malla' el estado anterior y mueve el
// actual al historial de rehacer. Devuelve false si no hay nada que deshacer
// (CE-01/CE-02).
bool HistorialDeshacer(Malla *malla);

// Rehace la última acción deshecha (simétrico). Devuelve false si no hay nada
// que rehacer (CE-03).
bool HistorialRehacer(Malla *malla);

bool HistorialHayDeshacer(void);
bool HistorialHayRehacer(void);

// Revierte el último HistorialRegistrar cuando la mutación posterior no llegó a
// aplicarse (p. ej. una conexión rechazada por ciclo o duplicado).
void HistorialDescartar(void);

// Vacía ambos historiales (p. ej. al abrir o crear una malla nueva).
void HistorialLimpiar(void);

#endif
