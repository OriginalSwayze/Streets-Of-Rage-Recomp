/* Zip files, unpacked in-process: Setup's own source (the zip appended to Setup.exe), the
 * build tools' zips and zipped ROMs, with no tar.exe (Wine and Proton have none, #28).
 * Stored and deflated entries (the inflate is png.c's), zip64, each checked by its CRC-32.
 * Paths are written through \\?\ so a deep one may pass Windows' 260 characters, and an entry
 * that would land outside the folder (an absolute path, "..") stops the unpacking. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "png.h"
#include "unzip.h"

static uint32_t crc_table[256];

static uint32_t crc32_of(const uint8_t *p, size_t n) {
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crc_table[i] = c;
        }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static uint32_t u16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t u32(const uint8_t *p) { return u16(p) | u16(p + 2) << 16; }
static uint64_t u64(const uint8_t *p) { return u32(p) | (uint64_t)u32(p + 4) << 32; }

typedef struct { FILE *f; long long base, len; } Zip;

/* n bytes at `off` in the zip. */
static bool zip_read(Zip *z, uint64_t off, void *buf, size_t n) {
    return off + n <= (uint64_t)z->len && !_fseeki64(z->f, z->base + (long long)off, SEEK_SET) && fread(buf, 1, n, z->f) == n;
}

/* An absolute folder as a \\?\ path: `out` holds it, without a closing backslash. */
static bool long_path(const char *dir, wchar_t *out, size_t n) {
    wchar_t w[MAX_PATH * 2], full[MAX_PATH * 2];
    if (!MultiByteToWideChar(CP_ACP, 0, dir, -1, w, MAX_PATH * 2)) return false;
    DWORD k = GetFullPathNameW(w, MAX_PATH * 2, full, NULL);
    if (!k || k >= MAX_PATH * 2) return false;
    while (k > 3 && full[k - 1] == L'\\') full[--k] = 0;
    if (!wcsncmp(full, L"\\\\?\\", 4)) swprintf(out, n, L"%ls", full);
    else if (!wcsncmp(full, L"\\\\", 2)) swprintf(out, n, L"\\\\?\\UNC\\%ls", full + 2);   /* \\server\share */
    else swprintf(out, n, L"\\\\?\\%ls", full);
    if (k == 3) out[wcslen(out) - 1] = 0;  /* a drive's root: C:\ */
    return true;
}

/* Makes every folder in `path` (a \\?\ path) up to its last backslash, or all of it with `all`. */
static void make_dirs(wchar_t *path, bool all) {
    size_t start = 4;                   /* past \\?\ and the drive or share */
    if (!wcsncmp(path + 4, L"UNC\\", 4)) {
        start = 8;
        for (int seps = 0; path[start] && seps < 2; start++) seps += path[start] == L'\\';
    } else {
        while (path[start] && path[start] != L'\\') start++;
        if (path[start]) start++;
    }
    for (size_t i = start; path[i]; i++)
        if (path[i] == L'\\') { path[i] = 0; CreateDirectoryW(path, NULL); path[i] = L'\\'; }
    if (all) CreateDirectoryW(path, NULL);
}

static bool write_file(const wchar_t *path, const uint8_t *data, size_t n) {
    SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);   /* a read-only file from the last install */
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    while (ok && n) {
        DWORD part = n > (1u << 30) ? (1u << 30) : (DWORD)n, put = 0;
        ok = WriteFile(h, data, part, &put, NULL) && put == part;
        data += put; n -= put;
    }
    return CloseHandle(h) && ok;
}

/* A name from the zip that is safe to unpack: no drive, no root, no "..". */
static bool safe_name(const char *s) {
    if (!*s || *s == '/' || *s == '\\' || strchr(s, ':')) return false;
    for (const char *p = s; *p;) {
        const char *e = p;
        while (*e && *e != '/' && *e != '\\') e++;
        if (e - p == 2 && p[0] == '.' && p[1] == '.') return false;
        p = *e ? e + 1 : e;
    }
    return true;
}

bool unzip(const char *path, long long base, long long len, const char *dest, bool strip,
           void (*tick)(int, int), char *why, size_t why_n) {
    char dummy[8];
    if (!why) { why = dummy; why_n = sizeof dummy; }
    #define FAIL(...) do { snprintf(why, why_n, __VA_ARGS__); goto done; } while (0)
    bool ok = false;
    uint8_t *cd = NULL, *tail = NULL;
    Zip z = { NULL, base, len };
    wchar_t wpath[MAX_PATH * 2], root[MAX_PATH * 2 + 16];
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH * 2);
    z.f = _wfopen(wpath, L"rb");
    if (!z.f) FAIL("Could not open %s.", path);
    if (z.len < 0) {
        _fseeki64(z.f, 0, SEEK_END);
        z.len = _ftelli64(z.f) - base;
    }
    if (z.len < 22) FAIL("Not a zip.");

    /* The end record: in the last 64 KB (its comment can be that long). */
    size_t tn = z.len < 65557 ? (size_t)z.len : 65557;
    tail = malloc(tn);
    if (!tail || !zip_read(&z, (uint64_t)z.len - tn, tail, tn)) FAIL("Could not read the zip.");
    long long e = (long long)tn - 22;
    while (e >= 0 && u32(tail + e) != 0x06054b50) e--;
    if (e < 0) FAIL("Not a zip.");
    uint64_t count = u16(tail + e + 10), cd_size = u32(tail + e + 12), cd_off = u32(tail + e + 16);
    if (count == 0xFFFF || cd_size == 0xFFFFFFFF || cd_off == 0xFFFFFFFF) {   /* zip64 */
        uint8_t loc[20], rec[56];
        uint64_t at = (uint64_t)z.len - tn + (uint64_t)e;
        if (at < 20 || !zip_read(&z, at - 20, loc, 20) || u32(loc) != 0x07064b50 ||
            !zip_read(&z, u64(loc + 8), rec, 56) || u32(rec) != 0x06064b50) FAIL("Could not read the zip.");
        count = u64(rec + 32); cd_size = u64(rec + 40); cd_off = u64(rec + 48);
    }
    if (cd_size > (256u << 20) || !(cd = malloc(cd_size ? cd_size : 1)) || !zip_read(&z, cd_off, cd, cd_size))
        FAIL("Could not read the zip's contents.");

    /* With strip: the one top folder, if every entry is in it. */
    char top[256] = "";
    size_t top_n = 0;
    if (strip) {
        bool one = true;
        size_t p = 0;
        for (uint64_t i = 0; i < count && one; i++) {
            if (p + 46 > cd_size || u32(cd + p) != 0x02014b50) FAIL("The zip is damaged.");
            size_t nl = u16(cd + p + 28);
            const char *name = (const char *)cd + p + 46, *slash = memchr(name, '/', nl);
            if (p + 46 + nl > cd_size) FAIL("The zip is damaged.");
            if (!slash || slash == name || (size_t)(slash - name) >= sizeof top) one = false;
            else if (!top_n) { top_n = (size_t)(slash - name) + 1; memcpy(top, name, top_n); }
            else if ((size_t)(slash - name) + 1 != top_n || memcmp(name, top, top_n)) one = false;
            p += 46 + nl + u16(cd + p + 30) + u16(cd + p + 32);
        }
        if (!one) top_n = 0;
    }

    if (!long_path(dest, root, MAX_PATH * 2)) FAIL("The folder %s is not one Setup can use.", dest);
    make_dirs(root, true);
    size_t p = 0;
    for (uint64_t i = 0; i < count; i++) {
        if (p + 46 > cd_size || u32(cd + p) != 0x02014b50) FAIL("The zip is damaged.");
        const uint8_t *h = cd + p;
        uint32_t flags = u16(h + 8), method = u16(h + 10), crc = u32(h + 16);
        uint64_t csize = u32(h + 20), usize = u32(h + 24), off = u32(h + 42);
        size_t nl = u16(h + 28), xl = u16(h + 30), cl = u16(h + 32);
        if (p + 46 + nl + xl > cd_size) FAIL("The zip is damaged.");
        for (size_t x = 0; x + 4 <= xl;) {      /* zip64: the sizes and offset too big for 32 bits */
            const uint8_t *ex = h + 46 + nl + x;
            size_t sz = u16(ex + 2), q = 4;
            if (u16(ex) == 1) {
                if (usize == 0xFFFFFFFF && q + 8 <= sz + 4) { usize = u64(ex + q); q += 8; }
                if (csize == 0xFFFFFFFF && q + 8 <= sz + 4) { csize = u64(ex + q); q += 8; }
                if (off == 0xFFFFFFFF && q + 8 <= sz + 4) { off = u64(ex + q); q += 8; }
            }
            x += 4 + sz;
        }
        char name[1024];
        if (nl >= sizeof name) FAIL("A name in the zip is too long.");
        memcpy(name, h + 46, nl);
        name[nl] = 0;
        p += 46 + nl + xl + cl;
        const char *rel = name + (top_n && nl >= top_n ? top_n : 0);
        if (!*rel) continue;                /* the top folder itself */
        if (!safe_name(rel)) FAIL("The zip has a file outside its folder (%s): not unpacking it.", name);
        if (flags & 1) FAIL("The zip is encrypted.");
        bool dir = name[nl - 1] == '/' || name[nl - 1] == '\\';
        wchar_t wrel[1024], full[MAX_PATH * 2 + 1100];
        if (!MultiByteToWideChar(flags & 0x800 ? CP_UTF8 : 437, 0, rel, -1, wrel, 1024) &&
            !MultiByteToWideChar(CP_ACP, 0, rel, -1, wrel, 1024)) FAIL("A name in the zip is not readable.");
        for (wchar_t *c = wrel; *c; c++) if (*c == L'/') *c = L'\\';
        swprintf(full, sizeof full / sizeof *full, L"%ls\\%ls", root, wrel);
        if (dir) { make_dirs(full, true); continue; }

        uint8_t lh[30];
        if (!zip_read(&z, off, lh, 30) || u32(lh) != 0x04034b50) FAIL("The zip is damaged (%s).", name);
        if (csize > (1ull << 31) || usize > (1ull << 31)) FAIL("%s is too big to unpack.", name);
        uint8_t *packed = malloc(csize ? (size_t)csize : 1), *data = NULL;
        size_t got = 0;
        bool read = packed && zip_read(&z, off + 30 + u16(lh + 26) + u16(lh + 28), packed, (size_t)csize);
        if (read && method == 0) { data = packed; got = (size_t)csize; packed = NULL; }
        else if (read && method == 8) data = png_inflate(packed, (size_t)csize, (size_t)usize, &got);
        free(packed);
        if (!read) { free(data); FAIL("Could not read %s from the zip.", name); }
        if (method != 0 && method != 8) FAIL("%s is packed in a way Setup cannot unpack (method %u).", name, method);
        bool good = data && got == usize && crc32_of(data, got) == crc;
        make_dirs(full, false);
        bool put = good && write_file(full, data, got);
        free(data);
        if (!good) FAIL("%s is damaged in the zip.", name);
        if (!put) FAIL("Could not write %s (is the folder writable, and no program running from it?).", rel);
        if (tick && (i % 64 == 0 || i + 1 == count)) tick((int)(i + 1), (int)count);
    }
    ok = true;
done:
    #undef FAIL
    free(cd); free(tail);
    if (z.f) fclose(z.f);
    return ok;
}
