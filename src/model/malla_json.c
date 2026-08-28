#include "malla_json.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

// ---------------------------------------------------------------------------
// Escritor JSON con indentación (US-16 RNF-03: archivo legible y verificable).
// ---------------------------------------------------------------------------

typedef struct {
    char *buf;      // buffer creciente terminado en '\0'
    size_t len;     // longitud usada
    size_t cap;     // capacidad
    bool ok;        // false si falló la reserva de memoria
} JsonWriter;

static void JwReserve(JsonWriter *w, size_t extra)
{
    if (!w->ok)
        return;
    if (w->len + extra + 1 <= w->cap)
        return;

    size_t nc = (w->cap == 0) ? 4096 : w->cap * 2;
    while (nc < w->len + extra + 1)
        nc *= 2;

    char *nb = (char *)realloc(w->buf, nc);
    if (nb == NULL)
    {
        w->ok = false;
        return;
    }
    w->buf = nb;
    w->cap = nc;
}

static void JwChar(JsonWriter *w, char c)
{
    JwReserve(w, 1);
    if (!w->ok)
        return;
    w->buf[w->len++] = c;
    w->buf[w->len] = '\0';
}

static void JwText(JsonWriter *w, const char *s)
{
    size_t n = strlen(s);
    JwReserve(w, n);
    if (!w->ok)
        return;
    memcpy(w->buf + w->len, s, n);
    w->len += n;
    w->buf[w->len] = '\0';
}

static void JwFmt(JsonWriter *w, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0)
    {
        w->ok = false;
        va_end(ap2);
        return;
    }
    JwReserve(w, (size_t)n);
    if (w->ok)
    {
        vsnprintf(w->buf + w->len, (size_t)n + 1, fmt, ap2);
        w->len += (size_t)n;
    }
    va_end(ap2);
}

// Escapa una cadena como literal JSON. El contenido interno es UTF-8 y pasa
// sin cambios (todo byte >= 0x20); solo se escapan comillas, barra y controles.
static void JwString(JsonWriter *w, const char *s)
{
    JwChar(w, '"');
    for (const char *p = s; *p != '\0'; p++)
    {
        unsigned char c = (unsigned char)*p;
        switch (c)
        {
            case '"':  JwText(w, "\\\""); break;
            case '\\': JwText(w, "\\\\"); break;
            case '\n': JwText(w, "\\n"); break;
            case '\r': JwText(w, "\\r"); break;
            case '\t': JwText(w, "\\t"); break;
            case '\b': JwText(w, "\\b"); break;
            case '\f': JwText(w, "\\f"); break;
            default:
                if (c < 0x20)
                    JwFmt(w, "\\u%04x", (unsigned)c);
                else
                    JwChar(w, (char)c);
        }
    }
    JwChar(w, '"');
}

// Escribe la serialización completa de la malla (RF-05..RF-08).
static void MallaASerializar(const Malla *m, JsonWriter *w)
{
    JwText(w, "{\n  \"version\": 1,\n  \"areas\": [");

    for (int i = 0; i < m->nAreas; i++)
    {
        const Area *a = &m->areas[i];
        if (i > 0)
            JwText(w, ",");
        JwText(w, "\n    {\n");
        JwFmt(w, "      \"id\": %d,\n", a->id);
        JwText(w, "      \"nombre\": ");
        JwString(w, a->nombre);
        JwText(w, ",\n");
        JwFmt(w, "      \"color\": { \"r\": %d, \"g\": %d, \"b\": %d, \"a\": %d },\n",
              a->color.r, a->color.g, a->color.b, a->color.a);
        JwText(w, "      \"descripcion\": ");
        JwString(w, a->descripcion);
        JwText(w, "\n    }");
    }

    JwText(w, "\n  ],\n  \"ramos\": [");

    for (int i = 0; i < m->nRamos; i++)
    {
        const Ramo *r = &m->ramos[i];
        if (i > 0)
            JwText(w, ",");
        JwText(w, "\n    {\n");
        JwFmt(w, "      \"id\": %d,\n", r->id);
        JwText(w, "      \"nombre\": ");
        JwString(w, r->nombre);
        JwText(w, ",\n");
        JwText(w, "      \"codigo\": ");
        JwString(w, r->codigo);
        JwText(w, ",\n");
        JwFmt(w, "      \"creditos\": %d,\n", r->creditos);
        JwFmt(w, "      \"semestre\": %d,\n", r->semestre);
        JwFmt(w, "      \"anio\": %d,\n", r->anio);
        JwFmt(w, "      \"horas\": %d,\n", r->horas);
        JwFmt(w, "      \"color\": { \"r\": %d, \"g\": %d, \"b\": %d, \"a\": %d },\n",
              r->color.r, r->color.g, r->color.b, r->color.a);
        JwFmt(w, "      \"areaId\": %d,\n", r->areaId);
        JwFmt(w, "      \"posicion\": { \"x\": %.2f, \"y\": %.2f },\n",
              r->posicion.x, r->posicion.y);
        JwText(w, "      \"prerrequisitos\": [");
        for (int k = 0; k < r->nPrerrequisitos; k++)
        {
            if (k > 0)
                JwText(w, ", ");
            JwFmt(w, "%d", r->prerrequisitos[k]);
        }
        JwText(w, "]\n    }");
    }

    JwText(w, "\n  ]\n}\n");
}

bool MallaGuardarArchivo(const Malla *m, const char *ruta, char *errMsg,
                         int errMsgSize)
{
    if (errMsg != NULL && errMsgSize > 0)
        errMsg[0] = '\0';

    if (ruta == NULL || ruta[0] == '\0')
    {
        if (errMsg != NULL && errMsgSize > 0)
            snprintf(errMsg, errMsgSize, "Ruta de archivo vac\xC3\xAD" "a.");
        return false;
    }

    JsonWriter w = { 0 };
    w.ok = true;   // el inicializador {0} deja ok=false; se arranca operativo
    MallaASerializar(m, &w);
    if (!w.ok)
    {
        if (errMsg != NULL && errMsgSize > 0)
            snprintf(errMsg, errMsgSize,
                     "Memoria insuficiente al serializar la malla.");
        free(w.buf);
        return false;
    }

    // RF-09: escritura atómica — primero un temporal, luego renombrado sobre
    // el destino. Un fallo a mitad de escritura deja intacto el archivo previo.
    char tmp[2048];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp", ruta);
    if (n < 0 || (size_t)n >= sizeof(tmp))
    {
        if (errMsg != NULL && errMsgSize > 0)
            snprintf(errMsg, errMsgSize, "La ruta es demasiado larga.");
        free(w.buf);
        return false;
    }

    FILE *f = fopen(tmp, "wb");
    if (f == NULL)
    {
        if (errMsg != NULL && errMsgSize > 0)
            snprintf(errMsg, errMsgSize, "%s", strerror(errno));
        free(w.buf);
        return false;
    }

    size_t escrito = fwrite(w.buf, 1, w.len, f);
    bool fallo = (escrito != w.len);
    if (fclose(f) != 0)
        fallo = true;

    if (!fallo && rename(tmp, ruta) != 0)
        fallo = true;

    if (fallo)
    {
        remove(tmp);   // no se deja basura a medio escribir
        if (errMsg != NULL && errMsgSize > 0)
        {
            if (errno != 0)
                snprintf(errMsg, errMsgSize, "%s", strerror(errno));
            else
                snprintf(errMsg, errMsgSize,
                         "No se pudo escribir el archivo.");
        }
        free(w.buf);
        return false;
    }

    free(w.buf);
    return true;
}

// ---------------------------------------------------------------------------
// Lector JSON (US-17 RF-07..RF-09): parser mínimo del formato serializado por
// US-16 (objetos, arrays, strings, números, true/false/null). Construye un
// pequeño DOM y lo valida en una segunda pasada.
// ---------------------------------------------------------------------------

typedef enum {
    JV_NULL = 0,
    JV_BOOL,
    JV_NUMBER,
    JV_STRING,
    JV_ARRAY,
    JV_OBJECT
} JvType;

typedef struct Jv Jv;
typedef struct {
    char *key;
    Jv *value;
} JvMember;

struct Jv {
    JvType type;
    union {
        bool b;
        double num;
        char *str;
        struct { Jv **items; int count; } arr;
        struct { JvMember *items; int count; } obj;
    } u;
};

typedef struct {
    const char *p;      // posición de lectura
    const char *end;    // fin del buffer
    char err[256];      // primer error (el más cercano a la causa raíz)
} Jp;

// Guarda solo el primer error: los mensajes anidados no pisan la causa.
static void JpErr(Jp *j, const char *msg)
{
    if (!j->err[0])
        snprintf(j->err, sizeof(j->err), "%s", msg);
}

static void JpSkipWs(Jp *j)
{
    while (j->p < j->end)
    {
        char c = *j->p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            j->p++;
        else
            break;
    }
}

static Jv *JvNew(JvType t)
{
    Jv *v = (Jv *)calloc(1, sizeof(*v));
    if (v != NULL)
        v->type = t;
    return v;
}

static void JvFree(Jv *v)
{
    if (v == NULL)
        return;
    switch (v->type)
    {
        case JV_STRING:
            free(v->u.str);
            break;
        case JV_ARRAY:
            for (int i = 0; i < v->u.arr.count; i++)
                JvFree(v->u.arr.items[i]);
            free(v->u.arr.items);
            break;
        case JV_OBJECT:
            for (int i = 0; i < v->u.obj.count; i++)
            {
                free(v->u.obj.items[i].key);
                JvFree(v->u.obj.items[i].value);
            }
            free(v->u.obj.items);
            break;
        default:
            break;
    }
    free(v);
}

// Codifica un punto de código (≤ U+FFFF, solo BMP) como UTF-8 en 'out'.
static int Utf8Encode(unsigned cp, char out[4])
{
    if (cp < 0x80)
    {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800)
    {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
}

// Anexa 'c' al buffer creciente 'buf'/'cap'/'len'. Devuelve false si no hay
// memoria (en ese caso deja el buffer apuntando a algo liberable y pone err).
static bool JpBufChar(Jp *j, char **buf, size_t *cap, size_t *len, char c)
{
    if (*len + 1 >= *cap)
    {
        size_t nc = (*cap == 0) ? 32 : *cap * 2;
        char *nb = (char *)realloc(*buf, nc);
        if (nb == NULL)
        {
            JpErr(j, "Memoria insuficiente.");
            return false;
        }
        *buf = nb;
        *cap = nc;
    }
    (*buf)[(*len)++] = c;
    (*buf)[*len] = '\0';
    return true;
}

// Lee una cadena JSON (j->p apunta a la comilla de apertura). Devuelve un
// buffer nuevo terminado en '\0' (contenido UTF-8 sin escapar) o NULL.
static char *JpString(Jp *j)
{
    j->p++;   // comilla de apertura

    size_t cap = 0, len = 0;
    char *out = NULL;

    while (j->p < j->end)
    {
        unsigned char c = (unsigned char)*j->p;
        if (c == '"')
        {
            j->p++;
            if (out != NULL)
                return out;
            out = (char *)malloc(1);
            if (out == NULL)
            {
                JpErr(j, "Memoria insuficiente.");
                return NULL;
            }
            out[0] = '\0';
            return out;
        }
        if (c == '\\')
        {
            j->p++;
            if (j->p >= j->end)
            {
                JpErr(j, "Cadena JSON sin cerrar.");
                free(out);
                return NULL;
            }
            char e = *j->p++;
            switch (e)
            {
                case '"':  if (!JpBufChar(j, &out, &cap, &len, '"')) return NULL; break;
                case '\\': if (!JpBufChar(j, &out, &cap, &len, '\\')) return NULL; break;
                case '/':  if (!JpBufChar(j, &out, &cap, &len, '/')) return NULL; break;
                case 'n':  if (!JpBufChar(j, &out, &cap, &len, '\n')) return NULL; break;
                case 't':  if (!JpBufChar(j, &out, &cap, &len, '\t')) return NULL; break;
                case 'r':  if (!JpBufChar(j, &out, &cap, &len, '\r')) return NULL; break;
                case 'b':  if (!JpBufChar(j, &out, &cap, &len, '\b')) return NULL; break;
                case 'f':  if (!JpBufChar(j, &out, &cap, &len, '\f')) return NULL; break;
                case 'u':
                {
                    if (j->end - j->p < 4)
                    {
                        JpErr(j, "Escape \\u inv\u00e1lido en cadena JSON.");
                        free(out);
                        return NULL;
                    }
                    unsigned cp = 0;
                    for (int k = 0; k < 4; k++)
                    {
                        char h = *j->p++;
                        unsigned hv;
                        if (h >= '0' && h <= '9') hv = (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') hv = (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') hv = (unsigned)(h - 'A' + 10);
                        else
                        {
                            JpErr(j, "Escape \\u inv\u00e1lido en cadena JSON.");
                            free(out);
                            return NULL;
                        }
                        cp = cp * 16 + hv;
                    }
                    char utf8[4];
                    int n = Utf8Encode(cp, utf8);
                    for (int k = 0; k < n; k++)
                        if (!JpBufChar(j, &out, &cap, &len, utf8[k]))
                            return NULL;
                    break;
                }
                default:
                    JpErr(j, "Escape desconocido en cadena JSON.");
                    free(out);
                    return NULL;
            }
            continue;
        }
        if (c < 0x20)
        {
            JpErr(j, "Car\u00e1cter de control sin escapar en cadena JSON.");
            free(out);
            return NULL;
        }
        if (!JpBufChar(j, &out, &cap, &len, (char)c))
            return NULL;
        j->p++;
    }

    JpErr(j, "Cadena JSON sin cerrar.");
    free(out);
    return NULL;
}

static Jv *JpValue(Jp *j);

static Jv *JpObject(Jp *j)
{
    j->p++;   // '{'
    Jv *o = JvNew(JV_OBJECT);
    if (o == NULL)
    {
        JpErr(j, "Memoria insuficiente.");
        return NULL;
    }

    JpSkipWs(j);
    if (j->p < j->end && *j->p == '}')
    {
        j->p++;
        return o;
    }

    for (;;)
    {
        JpSkipWs(j);
        if (j->p >= j->end || *j->p != '"')
        {
            JpErr(j, "Se esperaba el nombre de un campo.");
            JvFree(o);
            return NULL;
        }
        char *key = JpString(j);
        if (key == NULL)
        {
            JvFree(o);
            return NULL;
        }

        JpSkipWs(j);
        if (j->p >= j->end || *j->p != ':')
        {
            JpErr(j, "Se esperaba ':' tras el nombre del campo.");
            free(key);
            JvFree(o);
            return NULL;
        }
        j->p++;

        Jv *val = JpValue(j);
        if (val == NULL)
        {
            free(key);
            JvFree(o);
            return NULL;
        }

        JvMember *nm = (JvMember *)realloc(o->u.obj.items,
                                           (size_t)(o->u.obj.count + 1) *
                                               sizeof(*nm));
        if (nm == NULL)
        {
            free(key);
            JvFree(val);
            JvFree(o);
            JpErr(j, "Memoria insuficiente.");
            return NULL;
        }
        o->u.obj.items = nm;
        o->u.obj.items[o->u.obj.count].key = key;
        o->u.obj.items[o->u.obj.count].value = val;
        o->u.obj.count++;

        JpSkipWs(j);
        if (j->p >= j->end)
        {
            JpErr(j, "JSON incompleto.");
            JvFree(o);
            return NULL;
        }
        if (*j->p == ',')
        {
            j->p++;
            continue;
        }
        if (*j->p == '}')
        {
            j->p++;
            return o;
        }
        JpErr(j, "Se esperaba ',' o '}' en el objeto.");
        JvFree(o);
        return NULL;
    }
}

static Jv *JpArray(Jp *j)
{
    j->p++;   // '['
    Jv *a = JvNew(JV_ARRAY);
    if (a == NULL)
    {
        JpErr(j, "Memoria insuficiente.");
        return NULL;
    }

    JpSkipWs(j);
    if (j->p < j->end && *j->p == ']')
    {
        j->p++;
        return a;
    }

    for (;;)
    {
        Jv *val = JpValue(j);
        if (val == NULL)
        {
            JvFree(a);
            return NULL;
        }

        Jv **ni = (Jv **)realloc(a->u.arr.items,
                                  (size_t)(a->u.arr.count + 1) * sizeof(*ni));
        if (ni == NULL)
        {
            JvFree(val);
            JvFree(a);
            JpErr(j, "Memoria insuficiente.");
            return NULL;
        }
        a->u.arr.items = ni;
        a->u.arr.items[a->u.arr.count++] = val;

        JpSkipWs(j);
        if (j->p >= j->end)
        {
            JpErr(j, "JSON incompleto.");
            JvFree(a);
            return NULL;
        }
        if (*j->p == ',')
        {
            j->p++;
            continue;
        }
        if (*j->p == ']')
        {
            j->p++;
            return a;
        }
        JpErr(j, "Se esperaba ',' o ']' en la lista.");
        JvFree(a);
        return NULL;
    }
}

// Lee un número (enteros, flotantes y exponentes). Valida con strtod sobre el
// token copiado (el buffer original no está terminado en '\0').
static double JpNumber(Jp *j)
{
    const char *start = j->p;
    while (j->p < j->end)
    {
        char c = *j->p;
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
            c == 'e' || c == 'E')
            j->p++;
        else
            break;
    }

    char buf[64];
    size_t n = (size_t)(j->p - start);
    if (n == 0 || n >= sizeof(buf))
    {
        JpErr(j, "N\u00famero JSON inv\u00e1lido.");
        return 0.0;
    }
    memcpy(buf, start, n);
    buf[n] = '\0';

    char *endp = NULL;
    double v = strtod(buf, &endp);
    if (endp == buf || *endp != '\0')
    {
        JpErr(j, "N\u00famero JSON inv\u00e1lido.");
        return 0.0;
    }
    return v;
}

static Jv *JpValue(Jp *j)
{
    JpSkipWs(j);
    if (j->p >= j->end)
    {
        JpErr(j, "JSON incompleto.");
        return NULL;
    }

    char c = *j->p;
    if (c == '{')
        return JpObject(j);
    if (c == '[')
        return JpArray(j);
    if (c == '"')
    {
        char *s = JpString(j);
        if (s == NULL)
            return NULL;
        Jv *v = JvNew(JV_STRING);
        if (v == NULL)
        {
            free(s);
            JpErr(j, "Memoria insuficiente.");
            return NULL;
        }
        v->u.str = s;
        return v;
    }
    if (c == '-' || (c >= '0' && c <= '9'))
    {
        Jv *v = JvNew(JV_NUMBER);
        if (v == NULL)
        {
            JpErr(j, "Memoria insuficiente.");
            return NULL;
        }
        v->u.num = JpNumber(j);
        return v;
    }

    // Literales true/false/null
    if (j->end - j->p >= 4 && strncmp(j->p, "true", 4) == 0)
    {
        j->p += 4;
        Jv *v = JvNew(JV_BOOL);
        if (v == NULL) { JpErr(j, "Memoria insuficiente."); return NULL; }
        v->u.b = true;
        return v;
    }
    if (j->end - j->p >= 5 && strncmp(j->p, "false", 5) == 0)
    {
        j->p += 5;
        Jv *v = JvNew(JV_BOOL);
        if (v == NULL) { JpErr(j, "Memoria insuficiente."); return NULL; }
        v->u.b = false;
        return v;
    }
    if (j->end - j->p >= 4 && strncmp(j->p, "null", 4) == 0)
    {
        j->p += 4;
        return JvNew(JV_NULL);
    }

    JpErr(j, "Valor JSON inv\u00e1lido.");
    return NULL;
}

// Parsea el documento completo. Devuelve el valor raíz o NULL con 'err'.
static Jv *JsonParse(const char *text, size_t len, char *err, int errSize)
{
    if (err != NULL && errSize > 0)
        err[0] = '\0';

    Jp j = { text, text + len, { 0 } };
    Jv *root = JpValue(&j);
    if (root == NULL)
    {
        if (err != NULL && errSize > 0)
            snprintf(err, errSize, "%s", j.err[0] ? j.err : "JSON inv\u00e1lido.");
        return NULL;
    }

    JpSkipWs(&j);
    if (j.p != j.end)
    {
        JvFree(root);
        if (err != NULL && errSize > 0)
            snprintf(err, errSize,
                     "JSON inv\u00e1lido: hay contenido tras el documento.");
        return NULL;
    }
    return root;
}

// ---------------------------------------------------------------------------
// Acceso tipado al DOM
// ---------------------------------------------------------------------------

static const Jv *JvObjFind(const Jv *o, const char *key)
{
    for (int i = 0; i < o->u.obj.count; i++)
    {
        if (strcmp(o->u.obj.items[i].key, key) == 0)
            return o->u.obj.items[i].value;
    }
    return NULL;
}

static bool JvIsNum(const Jv *v) { return v != NULL && v->type == JV_NUMBER; }
static bool JvIsStr(const Jv *v) { return v != NULL && v->type == JV_STRING; }
static bool JvIsArr(const Jv *v) { return v != NULL && v->type == JV_ARRAY; }
static bool JvIsObj(const Jv *v) { return v != NULL && v->type == JV_OBJECT; }

static double JvNum(const Jv *v) { return v->u.num; }
static int JvInt(const Jv *v) { return (int)v->u.num; }
static const char *JvStr(const Jv *v) { return v->u.str; }

static bool MallaRamoExiste(const Malla *m, int id)
{
    for (int i = 0; i < m->nRamos; i++)
    {
        if (m->ramos[i].id == id)
            return true;
    }
    return false;
}

// Valida y reconstruye la malla a partir del DOM (RF-07..RF-08, CE-08/CE-09).
static bool MallaFromJson(Malla *m, const Jv *root, char *errMsg, int errMsgSize)
{
    if (!JvIsObj(root))
    {
        snprintf(errMsg, errMsgSize,
                 "El archivo no es un proyecto de malla v\u00e1lido.");
        return false;
    }

    const Jv *ver = JvObjFind(root, "version");
    if (!JvIsNum(ver))
    {
        snprintf(errMsg, errMsgSize,
                 "El archivo no incluye el campo \"version\".");
        return false;
    }
    if (JvNum(ver) > 1.0)
    {
        // RF-07: formatos futuros se rechazan sin tocar la malla actual.
        snprintf(errMsg, errMsgSize,
                 "Formato no soportado: la versi\u00f3n del archivo es mayor "
                 "que la soportada (1).");
        return false;
    }
    if (JvNum(ver) != 1.0)
    {
        snprintf(errMsg, errMsgSize, "Versi\u00f3n de archivo no reconocida.");
        return false;
    }

    const Jv *areas = JvObjFind(root, "areas");
    if (!JvIsArr(areas))
    {
        snprintf(errMsg, errMsgSize,
                 "Falta el campo \"areas\" o no es una lista.");
        return false;
    }
    const Jv *ramos = JvObjFind(root, "ramos");
    if (!JvIsArr(ramos))
    {
        snprintf(errMsg, errMsgSize,
                 "Falta el campo \"ramos\" o no es una lista.");
        return false;
    }

    if (areas->u.arr.count > MAX_AREAS || ramos->u.arr.count > MAX_RAMOS)
    {
        snprintf(errMsg, errMsgSize,
                 "El archivo excede el l\u00edmite de elementos soportado.");
        return false;
    }

    // Primer pase: áreas (RF-08: campos, tipos e IDs únicos).
    for (int i = 0; i < areas->u.arr.count; i++)
    {
        const Jv *a = areas->u.arr.items[i];
        if (!JvIsObj(a))
        {
            snprintf(errMsg, errMsgSize, "El \u00e1rea %d es inv\u00e1lida.",
                     i + 1);
            return false;
        }
        const Jv *idv = JvObjFind(a, "id");
        const Jv *nom = JvObjFind(a, "nombre");
        const Jv *col = JvObjFind(a, "color");
        const Jv *des = JvObjFind(a, "descripcion");
        if (!JvIsNum(idv) || !JvIsStr(nom) || !JvIsObj(col) ||
            (des != NULL && !JvIsStr(des)))
        {
            snprintf(errMsg, errMsgSize,
                     "El \u00e1rea %d tiene campos inv\u00e1lidos o "
                     "faltantes.", i + 1);
            return false;
        }
        int id = JvInt(idv);
        for (int k = 0; k < m->nAreas; k++)
        {
            if (m->areas[k].id == id)
            {
                snprintf(errMsg, errMsgSize,
                         "ID de \u00e1rea duplicado: %d.", id);
                return false;
            }
        }

        const Jv *r = JvObjFind(col, "r");
        const Jv *g = JvObjFind(col, "g");
        const Jv *b = JvObjFind(col, "b");
        const Jv *al = JvObjFind(col, "a");
        if (!JvIsNum(r) || !JvIsNum(g) || !JvIsNum(b))
        {
            snprintf(errMsg, errMsgSize,
                     "El color del \u00e1rea %d es inv\u00e1lido.", i + 1);
            return false;
        }

        Area *dst = &m->areas[m->nAreas++];
        dst->id = id;
        strncpy(dst->nombre, JvStr(nom), AREA_NOMBRE_MAX);
        dst->nombre[AREA_NOMBRE_MAX] = '\0';
        dst->color.r = (unsigned char)JvInt(r);
        dst->color.g = (unsigned char)JvInt(g);
        dst->color.b = (unsigned char)JvInt(b);
        dst->color.a = JvIsNum(al) ? (unsigned char)JvInt(al) : 255;
        if (JvIsStr(des))
        {
            strncpy(dst->descripcion, JvStr(des), AREA_DESCRIPCION_MAX);
            dst->descripcion[AREA_DESCRIPCION_MAX] = '\0';
        }
        else
        {
            dst->descripcion[0] = '\0';
        }
    }

    // Primer pase de ramos: campos, tipos e IDs únicos. Los prerrequisitos se
    // guardan tal cual (rawCount) y se validan en un segundo pase (los ramos
    // pueden aparecer en cualquier orden; CE-08).
    int rawCount[MAX_RAMOS] = { 0 };

    for (int i = 0; i < ramos->u.arr.count; i++)
    {
        const Jv *r = ramos->u.arr.items[i];
        if (!JvIsObj(r))
        {
            snprintf(errMsg, errMsgSize, "El ramo %d es inv\u00e1lido.",
                     i + 1);
            return false;
        }
        const Jv *idv = JvObjFind(r, "id");
        const Jv *nom = JvObjFind(r, "nombre");
        const Jv *cod = JvObjFind(r, "codigo");
        const Jv *cred = JvObjFind(r, "creditos");
        const Jv *sem = JvObjFind(r, "semestre");
        const Jv *anio = JvObjFind(r, "anio");
        const Jv *horas = JvObjFind(r, "horas");
        const Jv *col = JvObjFind(r, "color");
        const Jv *areaId = JvObjFind(r, "areaId");
        const Jv *pos = JvObjFind(r, "posicion");
        const Jv *prereq = JvObjFind(r, "prerrequisitos");

        if (!JvIsNum(idv) || !JvIsStr(nom) ||
            (cod != NULL && !JvIsStr(cod)) ||
            !JvIsNum(cred) || !JvIsNum(sem) || !JvIsNum(anio) ||
            !JvIsNum(horas) || !JvIsObj(col) || !JvIsNum(areaId) ||
            !JvIsObj(pos) || !JvIsArr(prereq))
        {
            snprintf(errMsg, errMsgSize,
                     "El ramo %d tiene campos inv\u00e1lidos o faltantes.",
                     i + 1);
            return false;
        }
        int id = JvInt(idv);
        for (int k = 0; k < m->nRamos; k++)
        {
            if (m->ramos[k].id == id)
            {
                snprintf(errMsg, errMsgSize,
                         "ID de ramo duplicado: %d.", id);
                return false;
            }
        }

        const Jv *cr = JvObjFind(col, "r");
        const Jv *cg = JvObjFind(col, "g");
        const Jv *cb = JvObjFind(col, "b");
        const Jv *ca = JvObjFind(col, "a");
        const Jv *px = JvObjFind(pos, "x");
        const Jv *py = JvObjFind(pos, "y");
        if (!JvIsNum(cr) || !JvIsNum(cg) || !JvIsNum(cb) ||
            !JvIsNum(px) || !JvIsNum(py))
        {
            snprintf(errMsg, errMsgSize,
                     "El color o la posici\u00f3n del ramo %d es "
                     "inv\u00e1lido.", i + 1);
            return false;
        }

        Ramo *dst = &m->ramos[m->nRamos++];
        dst->id = id;
        strncpy(dst->nombre, JvStr(nom), RAMO_NOMBRE_MAX);
        dst->nombre[RAMO_NOMBRE_MAX] = '\0';
        if (JvIsStr(cod))
        {
            strncpy(dst->codigo, JvStr(cod), RAMO_CODIGO_MAX);
            dst->codigo[RAMO_CODIGO_MAX] = '\0';
        }
        else
        {
            dst->codigo[0] = '\0';
        }
        dst->creditos = JvInt(cred);
        dst->semestre = JvInt(sem);
        dst->anio = JvInt(anio);
        dst->horas = JvInt(horas);
        dst->color.r = (unsigned char)JvInt(cr);
        dst->color.g = (unsigned char)JvInt(cg);
        dst->color.b = (unsigned char)JvInt(cb);
        dst->color.a = JvIsNum(ca) ? (unsigned char)JvInt(ca) : 255;
        dst->posicion.x = (float)JvNum(px);
        dst->posicion.y = (float)JvNum(py);

        // CE-09: areaId inexistente pasa a -1 (ramo sin área, US-07).
        int aid = JvInt(areaId);
        dst->areaId = (MallaFindAreaById(m, aid) != NULL) ? aid : SIN_AREA_ID;

        dst->nPrerrequisitos = 0;
        for (int k = 0; k < prereq->u.arr.count && k < MAX_RAMOS; k++)
        {
            const Jv *pv = prereq->u.arr.items[k];
            if (!JvIsNum(pv))
            {
                snprintf(errMsg, errMsgSize,
                         "La lista de prerrequisitos del ramo %d es "
                         "inv\u00e1lida.", i + 1);
                return false;
            }
            rawCount[m->nRamos - 1]++;
            dst->prerrequisitos[k] = JvInt(pv);
        }
        dst->nPrerrequisitos = rawCount[m->nRamos - 1];
    }

    // Segundo pase: prerrequisitos válidos (CE-08: IDs inexistentes o el
    // propio ramo se descartan).
    for (int i = 0; i < m->nRamos; i++)
    {
        Ramo *r = &m->ramos[i];
        int n = 0;
        for (int k = 0; k < rawCount[i]; k++)
        {
            int p = r->prerrequisitos[k];
            if (p != r->id && MallaRamoExiste(m, p))
                r->prerrequisitos[n++] = p;
        }
        r->nPrerrequisitos = n;
    }

    // RF-13: el contador de IDs se reanuda desde el máximo existente (mín. 1).
    int maxA = 0, maxR = 0;
    for (int i = 0; i < m->nAreas; i++)
        if (m->areas[i].id > maxA) maxA = m->areas[i].id;
    for (int i = 0; i < m->nRamos; i++)
        if (m->ramos[i].id > maxR) maxR = m->ramos[i].id;
    m->siguienteIdArea = maxA + 1;
    m->siguienteIdRamo = maxR + 1;

    m->modified = false;   // RF-10
    m->rutaArchivo[0] = '\0';   // la fija MallaCargarArchivo al reemplazar

    return true;
}

bool MallaCargarArchivo(Malla *m, const char *ruta, char *errMsg, int errMsgSize)
{
    if (errMsg != NULL && errMsgSize > 0)
        errMsg[0] = '\0';

    if (ruta == NULL || ruta[0] == '\0')
    {
        if (errMsg != NULL && errMsgSize > 0)
            snprintf(errMsg, errMsgSize, "Ruta de archivo vac\u00eda.");
        return false;
    }

    FILE *f = fopen(ruta, "rb");
    if (f == NULL)
    {
        if (errMsg != NULL && errMsgSize > 0)
            snprintf(errMsg, errMsgSize, "%s", strerror(errno));
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        if (errMsg != NULL && errMsgSize > 0)
            snprintf(errMsg, errMsgSize, "No se pudo leer el archivo.");
        return false;
    }
    long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        if (errMsg != NULL && errMsgSize > 0)
            snprintf(errMsg, errMsgSize, "No se pudo leer el archivo.");
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        if (errMsg != NULL && errMsgSize > 0)
            snprintf(errMsg, errMsgSize, "No se pudo leer el archivo.");
        return false;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (buf == NULL)
    {
        fclose(f);
        if (errMsg != NULL && errMsgSize > 0)
            snprintf(errMsg, errMsgSize, "Memoria insuficiente.");
        return false;
    }

    size_t leido = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (leido != (size_t)sz)
    {
        free(buf);
        if (errMsg != NULL && errMsgSize > 0)
            snprintf(errMsg, errMsgSize, "No se pudo leer el archivo.");
        return false;
    }
    buf[sz] = '\0';

    Jv *root = JsonParse(buf, (size_t)sz, errMsg, errMsgSize);
    free(buf);
    if (root == NULL)
        return false;

    // Se construye sobre una malla nueva: un fallo no toca la malla actual
    // (RF-09/RNF-02).
    Malla nueva;
    InitMalla(&nueva);
    bool ok = MallaFromJson(&nueva, root, errMsg, errMsgSize);
    JvFree(root);
    if (!ok)
        return false;

    *m = nueva;
    MallaSetRuta(m, ruta);
    return true;
}
