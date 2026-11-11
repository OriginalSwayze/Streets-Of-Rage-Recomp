/* Updates for the collection: see update.h.
 *
 * The installed version is the one Setup wrote to the Installed apps entry (DisplayVersion).
 * The newest release comes from GitHub's API (releases/latest of the collection's own
 * repository): its tag ("v2.8.4") and its one asset, the installer, with its size and
 * SHA-256. The download goes to the temporary folder and is used only when both match. */
#include "update.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
/* Linux: no Setup to hand over to (a copy is built from a release's source by
 * tools/install.sh, which is also how it is updated), so no check: as a copy Setup did not
 * install. update_pretend still works, for the sign's screenshots. */
static int pretend_state = UPDATE_NONE;
static char pretend_have[32], pretend_latest[32];
static float pretend_progress;
void update_check(void) {}
int update_state(void) { return pretend_state; }
const char *update_installed(void) { return pretend_have; }
const char *update_latest(void) { return pretend_latest; }
float update_progress(void) { return pretend_progress; }
const char *update_error(void) { return pretend_state == UPDATE_FAILED ? "The download did not finish. Try again." : ""; }
void update_download(void) {}
bool update_run(void) { return false; }
const char *update_file(void) { return ""; }
void update_pretend(int s, const char *have, const char *v, float k) {
    snprintf(pretend_have, sizeof pretend_have, "%s", have);
    snprintf(pretend_latest, sizeof pretend_latest, "%s", v);
    pretend_progress = k;
    pretend_state = s;
}
#else

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>

#define REPO       "OriginalSwayze/Streets-Of-Rage-Recomp"
#define UNINST_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\StreetsOfRageCollection"
#define AGENT      L"Streets-of-Rage-Collection-updater"

static SDL_atomic_t state;
static char installed[32], latest[32], error[200];
static char url[600], digest[65], file[MAX_PATH];
static long long size;
static SDL_atomic_t got_k, size_k;      /* download progress, in KB */

int update_state(void) { return SDL_AtomicGet(&state); }
const char *update_installed(void) { return installed; }
const char *update_latest(void) { return latest; }
const char *update_error(void) { return error; }
float update_progress(void) {
    int total = SDL_AtomicGet(&size_k);
    return total > 0 ? (float)SDL_AtomicGet(&got_k) / total : 0;
}

static void fail(const char *why) {
    snprintf(error, sizeof error, "%s", why);
    SDL_AtomicSet(&state, UPDATE_FAILED);
}

/* "2.8.3" or "v2.8.3" as a number to compare: each part up to 999. */
static long long version_number(const char *v) {
    if (*v == 'v' || *v == 'V') v++;
    long long n = 0;
    for (int part = 0; part < 4; part++) {
        n = n * 1000 + strtol(v, (char **)&v, 10);
        if (*v == '.') v++;
    }
    return n;
}

/* An https GET on github.com and the hosts it sends downloads to. `out` NULL: into a malloc'd
 * buffer (*body); else into that file, counting progress. */
static bool get(const char *u, char **body, size_t *len, FILE *out) {
    wchar_t wu[600], host[256], path[600];
    MultiByteToWideChar(CP_UTF8, 0, u, -1, wu, 600);
    URL_COMPONENTS c = { .dwStructSize = sizeof c, .lpszHostName = host, .dwHostNameLength = 256,
                         .lpszUrlPath = path, .dwUrlPathLength = 600 };
    if (!WinHttpCrackUrl(wu, 0, 0, &c) || c.nScheme != INTERNET_SCHEME_HTTPS) return false;
    bool ok = false;
    HINTERNET s = WinHttpOpen(AGENT, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET con = s ? WinHttpConnect(s, host, c.nPort, 0) : NULL;
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
    DWORD status = 0, sl = sizeof status;
    if (req && WinHttpSendRequest(req, L"Accept: application/vnd.github+json\r\n", (DWORD)-1, NULL, 0, 0, 0) &&
        WinHttpReceiveResponse(req, NULL) &&
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &sl, NULL) && status == 200) {
        size_t n = 0, cap = 0;
        char *buf = NULL, chunk[1 << 15];
        DWORD got = 0;
        ok = true;
        while (ok && WinHttpReadData(req, chunk, sizeof chunk, &got) && got) {
            if (out) {
                ok = fwrite(chunk, 1, got, out) == got;
                n += got;
                SDL_AtomicSet(&got_k, (int)(n >> 10));
                continue;
            }
            if (n + got + 1 > cap) {
                cap = (n + got + 1) * 2;
                char *more = realloc(buf, cap);
                if (!more) { ok = false; break; }
                buf = more;
            }
            memcpy(buf + n, chunk, got);
            n += got;
        }
        if (!out) {
            if (ok && buf) { buf[n] = 0; *body = buf; } else { free(buf); ok = false; }
        }
        if (len) *len = n;
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    if (s) WinHttpCloseHandle(s);
    return ok;
}

/* The string value after "key": in a JSON text, from `from` on. */
static bool json_string(const char *from, const char *key, char *out, size_t n) {
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(from, pat);
    if (!p) return false;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return false;
    p++;
    size_t k = 0;
    while (*p && *p != '"' && k + 1 < n) out[k++] = *p++;
    out[k] = 0;
    return *p == '"';
}

static int SDLCALL check_thread(void *arg) {
    (void)arg;
    char *body = NULL;
    if (!get("https://api.github.com/repos/" REPO "/releases/latest", &body, NULL, NULL)) {
        fail("Could not reach GitHub to check for updates.");
        return 0;
    }
    const char *assets = strstr(body, "\"assets\"");
    char tag[32] = "";
    bool ok = json_string(body, "tag_name", tag, sizeof tag) && assets;
    /* The installer among the assets (a release also has the source, for Linux): each asset's
     * fields come before its download link, so the .exe's own are read from where the asset
     * before it ended. */
    const char *at = assets;
    bool found = false;
    while (ok && at && !found) {
        const char *link = strstr(at, "\"browser_download_url\"");
        if (!link || !json_string(link, "browser_download_url", url, sizeof url)) break;
        if (strstr(url, ".exe")) { found = true; assets = at; }
        else at = link + 22;
    }
    ok = ok && found;
    if (ok) {                           /* the asset's size and SHA-256 ("digest": "sha256:...") */
        const char *p = strstr(assets, "\"size\"");
        if (p) { p += 6; while (*p == ' ' || *p == ':') p++; size = strtoll(p, NULL, 10); }
        char d[80] = "";
        if (json_string(assets, "digest", d, sizeof d) && !strncmp(d, "sha256:", 7)) snprintf(digest, sizeof digest, "%s", d + 7);
    }
    free(body);
    if (!ok || size <= 0) { fail("GitHub's answer had no installer in it."); return 0; }
    snprintf(latest, sizeof latest, "%s", tag[0] == 'v' ? tag + 1 : tag);
    SDL_AtomicSet(&state, version_number(latest) > version_number(installed) ? UPDATE_AVAILABLE : UPDATE_NONE);
    return 0;
}

void update_check(void) {
    HKEY k;
    DWORD n = sizeof installed;
    installed[0] = 0;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, UNINST_KEY, 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExA(k, "DisplayVersion", NULL, NULL, (BYTE *)installed, &n) != ERROR_SUCCESS) installed[0] = 0;
        RegCloseKey(k);
    }
    if (!installed[0]) { SDL_AtomicSet(&state, UPDATE_NONE); return; }   /* not installed by Setup (a build of one's own) */
    SDL_AtomicSet(&state, UPDATE_CHECKING);
    SDL_Thread *t = SDL_CreateThread(check_thread, "update check", NULL);
    if (t) SDL_DetachThread(t); else SDL_AtomicSet(&state, UPDATE_NONE);
}

static bool sha256_file(const char *path, char out[65]) {
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE h;
    UCHAR hash[32];
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0;
    if (ok && BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0) == 0) {
        static UCHAR buf[1 << 16];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) BCryptHashData(h, buf, (ULONG)n, 0);
        ok = BCryptFinishHash(h, hash, 32, 0) == 0;
        BCryptDestroyHash(h);
    } else ok = false;
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    fclose(f);
    if (ok) for (int i = 0; i < 32; i++) snprintf(out + i * 2, 3, "%02x", hash[i]);
    return ok;
}

static int SDLCALL download_thread(void *arg) {
    (void)arg;
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    snprintf(file, sizeof file, "%sStreets-of-Rage-Collection-Setup-v%s.exe", tmp, latest);
    SDL_AtomicSet(&got_k, 0);
    SDL_AtomicSet(&size_k, (int)(size >> 10));
    FILE *f = fopen(file, "wb");
    if (!f) { fail("Could not save the download in the temporary folder."); return 0; }
    size_t n = 0;
    bool ok = get(url, NULL, &n, f);
    ok = fclose(f) == 0 && ok;
    if (!ok) { DeleteFileA(file); fail("The download did not finish. Try again."); return 0; }
    char sum[65] = "";
    if ((long long)n != size || (digest[0] && (!sha256_file(file, sum) || _stricmp(sum, digest)))) {
        DeleteFileA(file);
        fail("The download did not match the release. Try again.");
        return 0;
    }
    SDL_AtomicSet(&state, UPDATE_READY);
    return 0;
}

void update_download(void) {
    int s = update_state();
    if (s != UPDATE_AVAILABLE && !(s == UPDATE_FAILED && latest[0])) return;
    SDL_AtomicSet(&state, UPDATE_DOWNLOADING);
    SDL_Thread *t = SDL_CreateThread(download_thread, "update download", NULL);
    if (t) SDL_DetachThread(t); else fail("Could not start the download.");
}

void update_pretend(int s, const char *have, const char *v, float k) {
    snprintf(installed, sizeof installed, "%s", have);
    snprintf(latest, sizeof latest, "%s", v);
    size = 1 << 20;
    SDL_AtomicSet(&size_k, 1024);
    SDL_AtomicSet(&got_k, (int)(k * 1024));
    if (s == UPDATE_FAILED) snprintf(error, sizeof error, "The download did not finish. Try again.");
    SDL_AtomicSet(&state, s);
}

const char *update_file(void) { return file; }

bool update_run(void) {
    if (update_state() != UPDATE_READY) return false;
    char cmd[MAX_PATH + 64];
    snprintf(cmd, sizeof cmd, "\"%s\" --update --wait %lu", file, (unsigned long)GetCurrentProcessId());
    STARTUPINFOA si = { .cb = sizeof si };
    PROCESS_INFORMATION pi;
    if (!CreateProcessA(file, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { fail("Could not start the installer."); return false; }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}
#endif
