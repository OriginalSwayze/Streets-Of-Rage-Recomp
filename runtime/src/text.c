/* Anti-aliased text, as 8-bit coverage masks.
 * Windows: from its own fonts, with GDI drawing white text on black into a DIB (the red
 * channel is the coverage). Where a face is not installed (Wine and Proton have none of
 * Impact, Segoe UI or Bahnschrift) and on Linux: from the fonts built in (runtime/fonts/,
 * open-licence stand-ins of the same shapes and widths), drawn with stb_truetype and
 * measured as GDI measures: each character's own advance plus the tracking. */
#include "text.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "stb_truetype.h"

/* ---- The built-in fonts ---------------------------------------------------------------- */
/* The stand-in for a Windows face: Impact by Anton, Segoe UI by Selawik (Microsoft's own,
 * made to its metrics), Bahnschrift by Barlow. Each is set in the Windows face's place: its
 * ascent and descent (per em, as GDI gives them) and widened or narrowed to its widths, so
 * the layouts made for Windows' fonts hold (measured against them on Windows 11). */
typedef struct { const char *font; float ascent, descent, wide; } StandIn;

static StandIn stand_in(const char *face, int weight) {
    if (!strcmp(face, "Impact")) return (StandIn){ "Anton-Regular", 1.009f, .211f, 1.0214f };
    if (!strncmp(face, "Bahnschrift", 11)) {
        if (strstr(face, "Condensed")) return (StandIn){ "BarlowCondensed-SemiBold", .994f, .206f, .9314f };
        if (strstr(face, "SemiBold") || weight >= 600) return (StandIn){ "Barlow-SemiBold", .994f, .206f, 1.0159f };
        return (StandIn){ "Barlow-Regular", .994f, .206f, 1.0339f };
    }
    if (!strcmp(face, "Palatino Linotype")) return (StandIn){ "Selawik-Bold", 1.05f, .299f, .9893f };
    if (strstr(face, "Semibold") || strstr(face, "SemiBold") || weight >= 600)
        return weight >= 700 ? (StandIn){ "Selawik-Bold", 1.079f, .251f, 1 } : (StandIn){ "Selawik-Semibold", 1.079f, .251f, .9817f };
    return (StandIn){ weight >= 700 ? "Selawik-Bold" : "Selawik-Regular", 1.079f, .251f, 1 };
}

typedef struct { const char *name; stbtt_fontinfo info; bool ok; } Loaded;
static Loaded loaded[16];

static const stbtt_fontinfo *font_named(const char *name) {
    for (int i = 0; font_files[i].name; i++) {
        if (strcmp(font_files[i].name, name)) continue;
        Loaded *l = &loaded[i < 16 ? i : 15];
        if (!l->name) {
            l->name = name;
            l->ok = stbtt_InitFont(&l->info, font_files[i].data, stbtt_GetFontOffsetForIndex(font_files[i].data, 0));
        }
        return l->ok ? &l->info : NULL;
    }
    return NULL;
}

/* The next character of UTF-8 text (a bad byte stands for itself). */
static int next_char(const unsigned char **p) {
    const unsigned char *s = *p;
    int c = *s++, more = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if (more) c &= 0x3F >> more;
    for (; more && (*s & 0xC0) == 0x80; more--) c = c << 6 | (*s++ & 0x3F);
    *p = s;
    return c;
}

static TextMask render_built_in(const char *utf8, const char *face, int px, int weight, int tracking) {
    TextMask m = { 0, 0, NULL, 0 };
    StandIn in = stand_in(face, weight);
    const stbtt_fontinfo *main = font_named(in.font), *symbols = font_named("DejaVuSans-Symbols");
    if (!main || px <= 0) return m;
    int cps[512], n = 0;
    for (const unsigned char *p = (const unsigned char *)utf8; *p && n < 512;) cps[n++] = next_char(&p);
    if (!n) return m;
    /* Each character from the font that has it: the face's, else the symbols'. */
    const stbtt_fontinfo *by[512];
    int dx[512], width = 0;
    for (int i = 0; i < n; i++) {
        by[i] = !stbtt_FindGlyphIndex(main, cps[i]) && symbols && stbtt_FindGlyphIndex(symbols, cps[i]) ? symbols : main;
        int adv, lsb;
        stbtt_GetCodepointHMetrics(by[i], cps[i], &adv, &lsb);
        dx[i] = (int)(adv * stbtt_ScaleForMappingEmToPixels(by[i], (float)px) * in.wide + .5f) + tracking;
        width += dx[i];
    }
    int top = (int)(in.ascent * px + .5f), h = top + (int)(in.descent * px + .5f) + 2;
    int w = (int)(width * 1.1f) + 4 + px * 3;
    uint8_t *a = calloc((size_t)w * h, 1);
    if (!a) return m;
    int base = top + 1;                 /* drawn one row down, as GDI's is */
    for (int i = 0, x = 2; i < n; x += dx[i++]) {
        float s = stbtt_ScaleForMappingEmToPixels(by[i], (float)px), sx = s * in.wide;
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(by[i], cps[i], sx, s, &x0, &y0, &x1, &y1);
        int gw = x1 - x0, gh = y1 - y0;
        if (gw <= 0 || gh <= 0 || gw > 4096 || gh > 4096) continue;
        uint8_t *g = calloc((size_t)gw * gh, 1);
        if (!g) continue;
        stbtt_MakeCodepointBitmap(by[i], g, gw, gh, gw, sx, s, cps[i]);
        for (int y = 0; y < gh; y++) {
            int ty = base + y0 + y;
            if (ty < 0 || ty >= h) continue;
            for (int xx = 0; xx < gw; xx++) {
                int tx = x + x0 + xx;
                if (tx < 0 || tx >= w) continue;
                uint8_t v = g[y * gw + xx], *d = &a[(size_t)ty * w + tx];
                if (v > *d) *d = v;
            }
        }
        free(g);
    }
    /* Cropped to the ink, as the GDI path is. */
    int last = 0;
    for (int y = 0; y < h; y++)
        for (int x = w - 1; x > last; x--)
            if (a[(size_t)y * w + x]) { last = x; break; }
    int cw = last + 3 > width + 4 ? last + 3 : width + 4;
    if (cw > w) cw = w;
    m.w = cw; m.h = h; m.baseline = base;
    m.a = malloc((size_t)cw * h);
    if (m.a) for (int y = 0; y < h; y++) memcpy(m.a + (size_t)y * cw, a + (size_t)y * w, (size_t)cw);
    free(a);
    return m;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static int CALLBACK found_face(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM out) {
    (void)lf; (void)tm; (void)type;
    *(bool *)out = true;
    return 0;
}

/* Whether Windows has this face (remembered: asked once each). SOR_BUILT_IN_FONTS=1 in the
 * environment says none, to see the built-in fonts on Windows as Linux and Wine show them. */
static bool installed(const wchar_t *face) {
    static struct { wchar_t face[64]; bool has; } seen[24];
    static int nseen;
    const char *force = getenv("SOR_BUILT_IN_FONTS");
    if (force && *force == '1') return false;
    for (int i = 0; i < nseen; i++) if (!wcscmp(seen[i].face, face)) return seen[i].has;
    LOGFONTW lf = { .lfCharSet = DEFAULT_CHARSET };
    wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
    bool has = false;
    HDC dc = GetDC(NULL);
    EnumFontFamiliesExW(dc, &lf, found_face, (LPARAM)&has, 0);
    ReleaseDC(NULL, dc);
    if (nseen < 24) { wcsncpy(seen[nseen].face, face, 63); seen[nseen++].has = has; }
    return has;
}

TextMask text_render(const char *utf8, const char *face, int px, int weight, int tracking) {
    TextMask m = { 0, 0, NULL, 0 };
    wchar_t wtext[512], wface[64];
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wtext, 512) - 1;
    MultiByteToWideChar(CP_UTF8, 0, face, -1, wface, 64);
    if (n <= 0) return m;
    if (!installed(wface)) return render_built_in(utf8, face, px, weight, tracking);

    HDC dc = CreateCompatibleDC(NULL);
    HFONT font = CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, wface);
    HGDIOBJ old_font = SelectObject(dc, font);
    /* Tracking: each character's advance plus the extra, given to ExtTextOut explicitly
     * (SetTextCharacterExtra is not reliably honoured when drawing). */
    int dx[512];
    SIZE size = { 0, 0 };
    for (int i = 0; i < n; i++) {
        SIZE one;
        GetTextExtentPoint32W(dc, wtext + i, 1, &one);
        dx[i] = one.cx + tracking;
        size.cx += dx[i];
    }
    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    /* Drawn into a wider bitmap, then cropped to the ink: a character the font lacks is
     * drawn from another font, which can be wider than it measures. */
    int w = size.cx + 4 + px * 3, h = tm.tmHeight + 2;

    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;             /* top-down */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ old_bmp = SelectObject(dc, bmp);
    memset(bits, 0, (size_t)w * h * 4);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    /* One character at a time: GDI can stop partway through a string given explicit
     * spacing when some characters come from another font. */
    for (int i = 0, x = 2; i < n; x += dx[i++]) TextOutW(dc, x, 1, wtext + i, 1);
    GdiFlush();

    const uint8_t *src = bits;
    int last = 0;
    for (int y = 0; y < h; y++)
        for (int x = w - 1; x > last; x--)
            if (src[((size_t)y * w + x) * 4 + 2]) { last = x; break; }
    int cw = last + 3 > size.cx + 4 ? last + 3 : size.cx + 4;
    if (cw > w) cw = w;
    m.w = cw; m.h = h;
    m.a = malloc((size_t)cw * h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < cw; x++) m.a[y * cw + x] = src[((size_t)y * w + x) * 4 + 2];
    m.baseline = tm.tmAscent + 1;

    SelectObject(dc, old_bmp);
    SelectObject(dc, old_font);
    DeleteObject(bmp);
    DeleteObject(font);
    DeleteDC(dc);
    return m;
}
#else
TextMask text_render(const char *utf8, const char *face, int px, int weight, int tracking) {
    return render_built_in(utf8, face, px, weight, tracking);
}
#endif

void text_free(TextMask *m) { free(m->a); m->a = NULL; }
