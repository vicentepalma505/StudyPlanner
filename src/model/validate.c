#include "validate.h"

#include <ctype.h>
#include <stddef.h>

bool StrIgualCaseInsensitive(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
        return a == b;

    while (*a != '\0' && *b != '\0')
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

bool EsTextoVacio(const char *s)
{
    if (s == NULL)
        return true;

    while (*s != '\0')
    {
        if (!isspace((unsigned char)*s))
            return false;
        s++;
    }
    return true;
}

bool EsNumeroEntero(const char *s)
{
    if (s == NULL || *s == '\0')
        return false;

    if (*s == '-')
        s++;
    if (*s == '\0')
        return false;

    while (*s != '\0')
    {
        if (!isdigit((unsigned char)*s))
            return false;
        s++;
    }
    return true;
}
