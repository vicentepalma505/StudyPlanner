#ifndef VALIDATE_H
#define VALIDATE_H

#include <stdbool.h>

// Comparación de cadenas sin distinguir mayúsculas/minúsculas (regla de
// unicidad de nombres de las specs). NULL == NULL devuelve true.
bool StrIgualCaseInsensitive(const char *a, const char *b);

// True si la cadena es NULL, vacía o contiene solo espacios/whitespace.
bool EsTextoVacio(const char *s);

// True si la cadena representa un entero: opcionalmente un '-' inicial
// seguido de uno o más dígitos (sin espacios ni otros caracteres).
bool EsNumeroEntero(const char *s);

#endif
