#include "font.h"

#include <stdlib.h>

static const char *FONT_CANDIDATES[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
};

static const char *ES_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 "
    ".,;:!?()[]{}_-+*/\\=<>|&%#@$~^\"'"
    "\xC2\xA1\xC2\xBF"                       // ¡ ¿
    "\xC3\x81\xC3\x89\xC3\x8D\xC3\x93\xC3\x9A\xC3\x9C\xC3\x91"  // Á É Í Ó Ú Ü Ñ
    "\xC3\xA1\xC3\xA9\xC3\xAD\xC3\xB3\xC3\xBA\xC3\xBC\xC3\xB1"  // á é í ó ú ü ñ
    "\xC3\xA0\xC3\xA8\xC3\xAC\xC3\xB2\xC3\xB9"                   // à è ì ò ù
    "\xC3\xA2\xC3\xAA\xC3\xAE\xC3\xB4\xC3\xBB"                   // â ê î ô û
    "\xC3\xA4\xC3\xAB\xC3\xAF\xC3\xB6\xC3\xBC"                   // ä ë ï ö ü
    "\xC3\xA7\xC3\x87"                                           // ç Ç
    "\xE2\x80\x94\xE2\x80\x93\xE2\x80\xA6";                      // — – …

Font LoadAppFont(bool *owned)
{
    *owned = false;

    for (size_t i = 0; i < sizeof(FONT_CANDIDATES) / sizeof(FONT_CANDIDATES[0]); i++)
    {
        const char *path = FONT_CANDIDATES[i];
        if (!FileExists(path))
            continue;

        int count = 0;
        int *cps = LoadCodepoints(ES_CHARS, &count);
        if (cps == NULL || count <= 0)
        {
            if (cps != NULL) UnloadCodepoints(cps);
            continue;
        }

        Font font = LoadFontEx(path, 16, cps, count);
        UnloadCodepoints(cps);

        if (font.texture.id > 0)
        {
            SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
            *owned = true;
            return font;
        }
        UnloadFont(font);
    }

    return GetFontDefault();
}
