// fork/pipe/waitpid/execvp exigen funciones POSIX, desactivadas por -std=c11
// estricto.
#define _POSIX_C_SOURCE 200809L

#include "dialog_nativo.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Una sola instancia a la vez (los diálogos que la usan son modales
// excluyentes: "Abrir malla" y "Exportar imagen").
static pid_t s_pid = -1;
static int s_pipe = -1;
static int s_intento = 0;            // 0 = zenity, 1 = kdialog (CE-03)
static DialogNativoModo s_modo = DNATIVO_ABRIR;
static char s_carpeta[1024];         // carpeta inicial (RF-03)
static char s_buffer[1024];
static int s_bufferLen = 0;

// Reemplaza el proceso actual por zenity o kdialog. Si execvp falla (el
// programa no está instalado), se sale con 127 para que el padre reintente con
// kdialog (CE-03) o avise al usuario (CE-04).
static void EjecutarHijo(int cual)
{
    if (cual == 0)
    {
        if (s_modo == DNATIVO_GUARDAR)
        {
            // zenity --file-selection --save --confirm-overwrite
            //        --title="Exportar imagen" --file-filter="Imagen | *.png …"
            //        [--filename=<carpeta>/]
            char filename[1100] = "";
            char *argv[9];
            int i = 0;
            argv[i++] = "zenity";
            argv[i++] = "--file-selection";
            argv[i++] = "--save";
            argv[i++] = "--confirm-overwrite";
            argv[i++] = "--title=Exportar imagen";
            argv[i++] = "--file-filter=Imagen | *.png *.PNG *.jpg *.JPG";
            if (s_carpeta[0] != '\0')
            {
                snprintf(filename, sizeof(filename), "--filename=%s/",
                         s_carpeta);
                argv[i++] = filename;
            }
            argv[i] = NULL;
            execvp("zenity", argv);
        }
        else
        {
            // zenity --file-selection --title="Abrir malla"
            //        --file-filter="*.malla" [--filename=<carpeta>/]
            char filename[1100] = "";
            char *argv[6];
            int i = 0;
            argv[i++] = "zenity";
            argv[i++] = "--file-selection";
            argv[i++] = "--title=Abrir malla";
            argv[i++] = "--file-filter=*.malla";
            if (s_carpeta[0] != '\0')
            {
                snprintf(filename, sizeof(filename), "--filename=%s/",
                         s_carpeta);
                argv[i++] = filename;
            }
            argv[i] = NULL;
            execvp("zenity", argv);
        }
    }
    else
    {
        if (s_modo == DNATIVO_GUARDAR)
        {
            // kdialog --getsavefilename <carpeta> "*.png *.jpg"
            char dir[1024];
            char *argv[5];
            int i = 0;
            argv[i++] = "kdialog";
            argv[i++] = "--getsavefilename";
            snprintf(dir, sizeof(dir), "%s",
                     (s_carpeta[0] != '\0') ? s_carpeta : ".");
            argv[i++] = dir;
            argv[i++] = "*.png *.jpg";
            argv[i] = NULL;
            execvp("kdialog", argv);
        }
        else
        {
            // kdialog --getopenfilename <carpeta> "*.malla"
            char dir[1024];
            char *argv[5];
            int i = 0;
            argv[i++] = "kdialog";
            argv[i++] = "--getopenfilename";
            snprintf(dir, sizeof(dir), "%s",
                     (s_carpeta[0] != '\0') ? s_carpeta : ".");
            argv[i++] = dir;
            argv[i++] = "*.malla";
            argv[i] = NULL;
            execvp("kdialog", argv);
        }
    }
    _exit(127);
}

// Crea el pipe y el proceso hijo (su stdout queda conectado al pipe; por ahí
// llega la ruta elegida).
static bool IniciarHijo(int cual)
{
    int fds[2];
    if (pipe(fds) != 0)
        return false;

    pid_t pid = fork();
    if (pid < 0)
    {
        close(fds[0]);
        close(fds[1]);
        return false;
    }

    if (pid == 0)
    {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        EjecutarHijo(cual);
        /* no retorna */
    }

    close(fds[1]);
    s_pid = pid;
    s_pipe = fds[0];
    return true;
}

void DialogNativoIniciar(DialogNativo *d, DialogNativoModo modo,
                         const char *carpeta)
{
    if (s_pid >= 0)
        DialogNativoTerminar(d);   // defensivo: se reinicia el diálogo

    d->estado = DNATIVO_INACTIVO;
    d->resultado[0] = '\0';

    s_modo = modo;
    s_carpeta[0] = '\0';
    if (carpeta != NULL)
        snprintf(s_carpeta, sizeof(s_carpeta), "%s", carpeta);
    s_intento = 0;
    s_bufferLen = 0;
    s_buffer[0] = '\0';

    if (IniciarHijo(s_intento))
        d->estado = DNATIVO_ACTIVO;
    else
        d->estado = DNATIVO_NO_DISPONIBLE;   // no se pudo lanzar el proceso
}

void DialogNativoPoll(DialogNativo *d)
{
    if (d->estado != DNATIVO_ACTIVO || s_pid < 0)
        return;

    int st = 0;
    pid_t r = waitpid(s_pid, &st, WNOHANG);
    if (r == 0)
        return;   // sigue abierto: la app continúa dibujando (RF-07/RNF-01)

    bool ok = false;
    bool noDisponible = false;
    if (r == s_pid)
    {
        ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
        noDisponible = WIFEXITED(st) && WEXITSTATUS(st) == 127;
    }
    // r < 0 (el hijo ya desapareció) → se trata como cancelación.

    // El hijo terminó: su extremo del pipe está cerrado, así que la lectura no
    // bloquea (RNF-01). Se drena el resultado.
    for (;;)
    {
        ssize_t n = read(s_pipe, s_buffer + s_bufferLen,
                         sizeof(s_buffer) - 1 - (size_t)s_bufferLen);
        if (n <= 0)
            break;
        s_bufferLen += (int)n;
    }
    close(s_pipe);
    s_pipe = -1;
    s_pid = -1;
    s_buffer[s_bufferLen] = '\0';

    // zenity/kdialog terminan la ruta con '\n' (RNF-04: se lee tal cual).
    size_t n = strlen(s_buffer);
    while (n > 0 && (s_buffer[n - 1] == '\n' || s_buffer[n - 1] == '\r' ||
                     s_buffer[n - 1] == ' '))
        s_buffer[--n] = '\0';

    if (noDisponible && s_intento == 0)
    {
        // zenity no está instalado → se intenta con kdialog (CE-03).
        s_intento = 1;
        if (IniciarHijo(s_intento))
        {
            d->estado = DNATIVO_ACTIVO;
            return;
        }
        d->estado = DNATIVO_NO_DISPONIBLE;
        return;
    }

    // Semántica (sección 11 de la spec): exit 0 + ruta no vacía → selección
    // (RF-05); cualquier otro caso → cancelación (RF-06).
    if (ok && s_buffer[0] != '\0')
    {
        snprintf(d->resultado, sizeof(d->resultado), "%s", s_buffer);
        d->estado = DNATIVO_SELECCION;
        return;
    }

    d->estado = noDisponible ? DNATIVO_NO_DISPONIBLE : DNATIVO_CANCELADO;
}

void DialogNativoTerminar(DialogNativo *d)
{
    if (s_pid >= 0)
    {
        kill(s_pid, SIGTERM);
        waitpid(s_pid, NULL, 0);   // se espera al hijo (CE-11)
        if (s_pipe >= 0)
            close(s_pipe);
        s_pid = -1;
        s_pipe = -1;
    }
    d->estado = DNATIVO_INACTIVO;
    d->resultado[0] = '\0';
}
