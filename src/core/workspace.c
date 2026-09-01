// mkdir exige funciones POSIX, desactivadas por -std=c11 estricto.
#define _POSIX_C_SOURCE 200809L

#include "workspace.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int RutaConfig(char *buf, int tam, const char *nombre)
{
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0')
        return -1;
    return snprintf(buf, (size_t)tam, "%s/.config/visor_malla/%s", home,
                    nombre);
}

static int MkdirP(const char *ruta)
{
    char buf[WORKSPACE_TAM];
    size_t n = strlen(ruta);
    if (n >= sizeof(buf))
        return -1;
    memcpy(buf, ruta, n + 1);

    for (char *p = buf + 1; *p != '\0'; p++)
    {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(buf, 0755) != 0 && errno != EEXIST)
            return -1;
        *p = '/';
    }
    if (mkdir(buf, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

// Lee el archivo de configuración 'nombre' (workspace.cfg o export.cfg) en
// 'buf'. Sin archivo o fallo → 'buf' vacío.
static void CargarCfg(const char *nombre, char *buf, int tam)
{
    if (buf == NULL || tam <= 0)
        return;
    buf[0] = '\0';

    char ruta[WORKSPACE_TAM];
    if (RutaConfig(ruta, (int)sizeof(ruta), nombre) < 0)
        return;

    FILE *f = fopen(ruta, "r");
    if (f == NULL)
        return;
    size_t leidos = fread(buf, 1, (size_t)tam - 1, f);
    buf[leidos] = '\0';
    fclose(f);

    // Se recorta el salto de línea final (GuardarCfg escribe "\n").
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
}

// Escribe 'ruta' (una línea) en el archivo de configuración 'nombre',
// creando ~/.config/visor_malla si hace falta (RNF-02: si falla, se continúa).
static void GuardarCfg(const char *nombre, const char *ruta)
{
    if (ruta == NULL)
        return;

    char cfg[WORKSPACE_TAM];
    if (RutaConfig(cfg, (int)sizeof(cfg), nombre) < 0)
        return;

    char dir[WORKSPACE_TAM];
    snprintf(dir, sizeof(dir), "%s", cfg);
    char *slash = strrchr(dir, '/');
    if (slash != NULL)
    {
        *slash = '\0';
        MkdirP(dir);
    }

    FILE *f = fopen(cfg, "w");
    if (f == NULL)
        return;
    fprintf(f, "%s\n", ruta);
    fclose(f);
}

void WorkspaceCargar(char *buf, int tam)
{
    CargarCfg("workspace.cfg", buf, tam);
}

void WorkspaceGuardar(const char *ruta)
{
    GuardarCfg("workspace.cfg", ruta);
}

void WorkspaceCargarExportDir(char *buf, int tam)
{
    CargarCfg("export.cfg", buf, tam);
}

void WorkspaceGuardarExportDir(const char *ruta)
{
    GuardarCfg("export.cfg", ruta);
}

void WorkspaceDirDeArchivo(const char *archivo, char *dir, int tam)
{
    if (dir == NULL || tam <= 0)
        return;
    if (archivo == NULL)
    {
        snprintf(dir, tam, ".");
        return;
    }

    const char *slash = strrchr(archivo, '/');
    if (slash == NULL)
    {
        snprintf(dir, tam, ".");
        return;
    }

    int n = (int)(slash - archivo);
    if (n == 0)
        n = 1;   // "/archivo" → "/"
    if (n >= tam)
        n = tam - 1;
    memcpy(dir, archivo, (size_t)n);
    dir[n] = '\0';
}
