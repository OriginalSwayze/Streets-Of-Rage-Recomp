/* Text rendering for the launcher and menus (text.c). */
#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>

typedef struct {
    int w, h;
    uint8_t *a;                         /* coverage, w*h, 0-255 */
    int baseline;                       /* rows from the top to the baseline */
} TextMask;

/* weight: 100-900 (400 regular, 700 bold); tracking: extra pixels between letters. */
TextMask text_render(const char *utf8, const char *face, int px, int weight, int tracking);
void     text_free(TextMask *m);

/* The fonts in runtime/fonts/, built in (fonts_data.c, made by cmake/fonts.cmake); the last
 * entry's name is NULL. */
typedef struct { const char *name; const unsigned char *data; unsigned len; } FontFile;
extern const FontFile font_files[];

#endif
