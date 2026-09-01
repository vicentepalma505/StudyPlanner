#ifndef TYPES_H
#define TYPES_H

#include "raylib.h"

#define MAX_AREAS 100
#define MAX_RAMOS 100

#define AREA_NOMBRE_MAX        40
#define AREA_DESCRIPCION_MAX   200
#define RAMO_NOMBRE_MAX        60
#define RAMO_CODIGO_MAX        10

// -1 = sin área (US-05/US-07)
#define SIN_AREA_ID -1

// Color neutro (gris) para ramos sin área (US-05 RF-04, US-14)
#define COLOR_RAMO_SIN_AREA (Color){ 156, 156, 156, 255 }

typedef struct {
    int id;
    char nombre[AREA_NOMBRE_MAX + 1];
    Color color;
    char descripcion[AREA_DESCRIPCION_MAX + 1];
} Area;

typedef struct {
    int id;
    char nombre[RAMO_NOMBRE_MAX + 1];
    char codigo[RAMO_CODIGO_MAX + 1];
    int creditos;
    int semestre;
    int anio;
    int horas;
    Color color;
    int areaId;
    Vector2 posicion;
    int prerrequisitos[MAX_RAMOS];
    int nPrerrequisitos;
} Ramo;

typedef struct {
    Area areas[MAX_AREAS];
    int nAreas;
    Ramo ramos[MAX_RAMOS];
    int nRamos;
    int siguienteIdArea;
    int siguienteIdRamo;
    bool modified;
    char rutaArchivo[1024];
} Malla;

#endif
