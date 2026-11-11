/* Replacement music: see music.h.
 *
 * The song requests (docs/ram*.md, ost.c):
 *   Streets of Rage 3  jsr $1A8004 with d0 = $01xx; 0 pauses the driver (bit 0 of $FF9F18:
 *                      its channels go quiet and hold), 2 resumes it. The game pauses with
 *                      them ($002666, Start in play; $FFF505 bit 0 is set while paused). A
 *                      song with a file is asked for as 0 (no request past the last song is
 *                      silent: they are ignored, the song plays on)
 *   Streets of Rage 2  jsr $040008 with d0 = $01xx; $010C is a silent song
 *   Streets of Rage    a byte in the queue at $FFF00A ($81-$91), read by the update at
 *                      $072914; $E1 stops the music, $E0 fades it
 * A request for a song with a file is changed, before the driver sees it, to the silent
 * one; the driver's sound effects play on as they would over its own music.
 *
 * Files are decoded by Windows (Media Foundation) on a thread of their own, into memory as
 * 16-bit stereo, which playback follows while the rest comes in. */
#include "music.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#define strcasecmp _stricmp
#define SEP "\\"
#else
/* Elsewhere (Linux), MP3, FLAC, WAV and Ogg Vorbis, with the single-file decoders in
 * third_party/single (built in decoders.c). */
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include "dr_flac.h"
#include "dr_mp3.h"
#include "dr_wav.h"
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#define SEP "/"
#endif

#include "ost.h"
#include "recomp_rt.h"

bool music_files_on = true;

static char folder[1024];
static char paths[OST_SONGS][1024];     /* each song's file, or empty */
static int found;

/* Songs that end rather than loop (a round's clear, a game over...), by the player's
 * number: measured with --sound-probe (silent before 200 s). */
#if GAME_SOR == 3
static const bool ends[OST_SONGS] = { [21] = 1, [22] = 1, [28] = 1, [29] = 1, [31] = 1, [32] = 1, [33] = 1, [35] = 1 };
#elif GAME_SOR == 2
static const bool ends[OST_SONGS] = { [11] = 1, [23] = 1 };
#elif GAME_SMPS
static const bool ends[OST_SONGS] = { [10] = 1, [11] = 1, [12] = 1 };
#else
static const bool ends[OST_SONGS] = { 0 };
#endif

/* ---- Finding the files ------------------------------------------------------------------ */
void music_set_folder(const char *dir) { snprintf(folder, sizeof folder, "%s", dir); }
int music_found(void) { return found; }

static bool playable(const char *name) {
    const char *dot = strrchr(name, '.');
#ifdef _WIN32
    static const char *const exts[] = { ".mp3", ".flac", ".wav", ".m4a", ".aac", ".wma", ".ogg", ".opus" };
#else
    static const char *const exts[] = { ".mp3", ".flac", ".wav", ".ogg" };
#endif
    if (!dot) return false;
    for (size_t k = 0; k < sizeof exts / sizeof *exts; k++) if (!strcasecmp(dot, exts[k])) return true;
    return false;
}

/* A file in the music folder: "07.flac", "07 - Go Straight.mp3" is song 7. With two for one
 * song, the first by name (the order Windows lists them in; Linux lists in no order). */
static void consider(const char *name) {
    if (!playable(name)) return;
    int n = 0, digits = 0;
    for (const char *p = name; *p >= '0' && *p <= '9' && digits < 3; p++, digits++) n = n * 10 + (*p - '0');
    if (!digits || n < 1 || n > OST_SONGS) return;
    char path[sizeof paths[0]];
    snprintf(path, sizeof path, "%smusic" SEP "%s", folder, name);
    if (paths[n - 1][0] && strcmp(path, paths[n - 1]) >= 0) return;
    if (!paths[n - 1][0]) found++;
    snprintf(paths[n - 1], sizeof paths[0], "%s", path);
}

int music_scan(void) {
    memset(paths, 0, sizeof paths);
    found = 0;
    char pattern[1100];
#ifdef _WIN32
    snprintf(pattern, sizeof pattern, "%smusic\\*", folder);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) consider(fd.cFileName);
    while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    snprintf(pattern, sizeof pattern, "%smusic", folder);
    DIR *d = opendir(pattern);
    if (!d) return 0;
    for (struct dirent *e; (e = readdir(d));) {
        char full[1400];
        struct stat st;
        snprintf(full, sizeof full, "%s/%s", pattern, e->d_name);
        if (!stat(full, &st) && S_ISREG(st.st_mode)) consider(e->d_name);
    }
    closedir(d);
#endif
    return found;
}

/* ---- Decoding ---------------------------------------------------------------------------- */
typedef struct {
    char path[1024];
    int16_t *pcm;                       /* stereo frames */
    int capacity, rate;
    SDL_atomic_t decoded;               /* frames ready */
    SDL_atomic_t done;                  /* 1 when the whole file is in (or it failed) */
    SDL_atomic_t cancel, refs;
} Track;

static void track_release(Track *t) {
    if (SDL_AtomicAdd(&t->refs, -1) == 1) { free(t->pcm); free(t); }
}

#ifdef _WIN32
static int decode(void *arg) {
    Track *t = arg;
    HRESULT co = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    static SDL_atomic_t started;
    if (SDL_AtomicCAS(&started, 0, 1)) MFStartup(MF_VERSION, MFSTARTUP_LITE);
    wchar_t wpath[1024];
    MultiByteToWideChar(CP_UTF8, 0, t->path, -1, wpath, 1024);
    IMFSourceReader *r = NULL;
    IMFMediaType *want = NULL, *got = NULL;
    if (FAILED(MFCreateSourceReaderFromURL(wpath, NULL, &r))) goto out;
    IMFSourceReader_SetStreamSelection(r, (DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    IMFSourceReader_SetStreamSelection(r, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    MFCreateMediaType(&want);
    IMFMediaType_SetGUID(want, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(want, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    IMFMediaType_SetUINT32(want, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    if (FAILED(IMFSourceReader_SetCurrentMediaType(r, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, want))) goto out;
    if (FAILED(IMFSourceReader_GetCurrentMediaType(r, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &got))) goto out;
    UINT32 channels = 0, rate = 0;
    IMFMediaType_GetUINT32(got, &MF_MT_AUDIO_NUM_CHANNELS, &channels);
    IMFMediaType_GetUINT32(got, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
    if (!channels || !rate) goto out;
    PROPVARIANT dur;
    PropVariantInit(&dur);
    double seconds = 600;               /* at most ten minutes when the file does not say */
    if (SUCCEEDED(IMFSourceReader_GetPresentationAttribute(r, (DWORD)MF_SOURCE_READER_MEDIASOURCE, &MF_PD_DURATION, &dur)))
        seconds = (double)dur.uhVal.QuadPart / 1e7 + 1;
    PropVariantClear(&dur);
    if (seconds > 900) seconds = 900;
    t->capacity = (int)(seconds * rate);
    t->pcm = malloc((size_t)t->capacity * 4);
    if (!t->pcm) goto out;
    t->rate = (int)rate;
    int have = 0;
    while (!SDL_AtomicGet(&t->cancel)) {
        DWORD flags = 0;
        IMFSample *sample = NULL;
        if (FAILED(IMFSourceReader_ReadSample(r, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, NULL, &sample))) break;
        if (sample) {
            IMFMediaBuffer *buf = NULL;
            if (SUCCEEDED(IMFSample_ConvertToContiguousBuffer(sample, &buf))) {
                BYTE *p; DWORD len;
                if (SUCCEEDED(IMFMediaBuffer_Lock(buf, &p, NULL, &len))) {
                    const int16_t *s = (const int16_t *)p;
                    int frames = (int)(len / (2 * channels));
                    for (int i = 0; i < frames && have < t->capacity; i++, have++) {
                        t->pcm[have * 2] = s[i * channels];
                        t->pcm[have * 2 + 1] = s[i * channels + (channels > 1)];
                    }
                    IMFMediaBuffer_Unlock(buf);
                }
                IMFMediaBuffer_Release(buf);
            }
            IMFSample_Release(sample);
            SDL_AtomicSet(&t->decoded, have);
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM || have >= t->capacity) break;
    }
out:
    if (got) IMFMediaType_Release(got);
    if (want) IMFMediaType_Release(want);
    if (r) IMFSourceReader_Release(r);
    if (SUCCEEDED(co)) CoUninitialize();
    SDL_AtomicSet(&t->done, 1);
    track_release(t);
    return 0;
}
#else
/* One of the four kinds of file, opened by its extension. */
typedef struct {
    int kind;                           /* 1 MP3, 2 FLAC, 3 WAV, 4 Ogg Vorbis */
    drmp3 mp3;
    drwav wav;
    drflac *flac;
    stb_vorbis *ogg;
    unsigned channels, rate;
    uint64_t frames;                    /* 0 if unknown */
} Reader;

static bool reader_open(Reader *r, const char *path) {
    const char *dot = strrchr(path, '.');
    memset(r, 0, sizeof *r);
    if (!dot) return false;
    if (!strcasecmp(dot, ".mp3") && drmp3_init_file(&r->mp3, path, NULL)) {
        r->kind = 1; r->channels = r->mp3.channels; r->rate = r->mp3.sampleRate;
        r->frames = drmp3_get_pcm_frame_count(&r->mp3);   /* (back at the start after) */
    } else if (!strcasecmp(dot, ".flac") && (r->flac = drflac_open_file(path, NULL))) {
        r->kind = 2; r->channels = r->flac->channels; r->rate = r->flac->sampleRate; r->frames = r->flac->totalPCMFrameCount;
    } else if (!strcasecmp(dot, ".wav") && drwav_init_file(&r->wav, path, NULL)) {
        r->kind = 3; r->channels = r->wav.channels; r->rate = r->wav.sampleRate; r->frames = r->wav.totalPCMFrameCount;
    } else if (!strcasecmp(dot, ".ogg")) {
        int err = 0;
        if (!(r->ogg = stb_vorbis_open_filename(path, &err, NULL))) return false;
        stb_vorbis_info info = stb_vorbis_get_info(r->ogg);
        r->kind = 4; r->channels = (unsigned)info.channels; r->rate = info.sample_rate;
        r->frames = stb_vorbis_stream_length_in_samples(r->ogg);
    }
    return r->kind && r->channels && r->channels <= 8 && r->rate;
}

/* Up to n frames of interleaved 16-bit samples; how many came. */
static uint64_t reader_read(Reader *r, int16_t *out, uint64_t n) {
    switch (r->kind) {
    case 1: return drmp3_read_pcm_frames_s16(&r->mp3, n, out);
    case 2: return drflac_read_pcm_frames_s16(r->flac, n, out);
    case 3: return drwav_read_pcm_frames_s16(&r->wav, n, out);
    case 4: return (uint64_t)stb_vorbis_get_samples_short_interleaved(r->ogg, (int)r->channels, out, (int)(n * r->channels));
    }
    return 0;
}

static void reader_close(Reader *r) {
    if (r->kind == 1) drmp3_uninit(&r->mp3);
    if (r->flac) drflac_close(r->flac);
    if (r->kind == 3) drwav_uninit(&r->wav);
    if (r->ogg) stb_vorbis_close(r->ogg);
}

static int decode(void *arg) {
    Track *t = arg;
    Reader r;
    if (!reader_open(&r, t->path)) goto out;
    double seconds = r.frames ? (double)r.frames / r.rate + 1 : 600;   /* at most ten minutes when the file does not say */
    if (seconds > 900) seconds = 900;
    t->capacity = (int)(seconds * r.rate);
    t->pcm = malloc((size_t)t->capacity * 4);
    if (!t->pcm) goto out;
    t->rate = (int)r.rate;
    int have = 0;
    int16_t *s = malloc(4096 * 8 * sizeof *s);   /* 4096 frames of up to 8 channels */
    while (s && !SDL_AtomicGet(&t->cancel) && have < t->capacity) {
        uint64_t frames = reader_read(&r, s, 4096);
        if (!frames) break;
        for (uint64_t i = 0; i < frames && have < t->capacity; i++, have++) {
            t->pcm[have * 2] = s[i * r.channels];
            t->pcm[have * 2 + 1] = s[i * r.channels + (r.channels > 1)];
        }
        SDL_AtomicSet(&t->decoded, have);
    }
    free(s);
out:
    reader_close(&r);
    SDL_AtomicSet(&t->done, 1);
    track_release(t);
    return 0;
}
#endif

/* ---- Playing ------------------------------------------------------------------------------ */
static SDL_mutex *lock;
static Track *now;                      /* the playing file, or NULL */
static int now_song = -1;
static double pos;                      /* in the file's frames */
static float fade = 1, fade_step;       /* a fade-out: per output frame */
static bool paused;                     /* the game is paused: the file holds its place */

static void ensure_lock(void) { if (!lock) lock = SDL_CreateMutex(); }

static void play_locked(int song, double at) {
    if (now) { SDL_AtomicSet(&now->cancel, 1); track_release(now); now = NULL; }
    now_song = -1;
    fade = 1; fade_step = 0;
    paused = false;
    if (song < 0 || !paths[song][0]) return;
    Track *t = calloc(1, sizeof *t);
    if (!t) return;
    snprintf(t->path, sizeof t->path, "%s", paths[song]);
    SDL_AtomicSet(&t->refs, 2);         /* the player's, the decoder's */
    SDL_Thread *th = SDL_CreateThread(decode, "music", t);
    if (!th) { free(t); return; }
    SDL_DetachThread(th);
    now = t;
    now_song = song;
    pos = at;
}

void music_stop(void) {
    ensure_lock();
    SDL_LockMutex(lock);
    play_locked(-1, 0);
    SDL_UnlockMutex(lock);
}

/* The game's song is silenced for a file (which may have ended since): until the next song. */
static bool replaced;

bool music_playing(void) { return replaced || now != NULL; }

static void start_fade(void) {
    ensure_lock();
    SDL_LockMutex(lock);
    if (now && !fade_step) fade_step = 1.f / (48000 * 2.5f);    /* about the driver's fade */
    SDL_UnlockMutex(lock);
}

/* A song request: the request the driver should get instead. */
static uint16_t on_request(uint16_t request, uint16_t silent) {
    int song = ost_song_of_request(request);
    ensure_lock();
    SDL_LockMutex(lock);
    bool ours = music_files_on && song >= 0 && paths[song][0];
    replaced = ours;
    if (ours) { if (song != now_song) play_locked(song, 0); }   /* asked again while it plays: it plays on */
    else play_locked(-1, 0);
    SDL_UnlockMutex(lock);
    return ours ? silent : request;
}

void music_mix(int16_t *out, int frames, int rate, float gain) {
    if (!now || !lock || paused) return;
    SDL_LockMutex(lock);
    Track *t = now;
    if (t && t->rate) {
        int have = SDL_AtomicGet(&t->decoded);
        bool all = SDL_AtomicGet(&t->done);
        double step = (double)t->rate / rate;
        float g = gain * .6f;           /* about the level of the game's own music */
        for (int i = 0; i < frames; i++) {
            int a = (int)pos;
            if (a + 1 >= have) {
                if (!all) break;        /* still decoding: a moment late rather than a jump */
                if (have < 2 || ends[now_song]) { play_locked(-1, 0); break; }
                pos = 0; a = 0;         /* loops from the start */
            }
            float f = (float)(pos - a);
            for (int c = 0; c < 2; c++) {
                float s = t->pcm[a * 2 + c] + (t->pcm[(a + 1) * 2 + c] - t->pcm[a * 2 + c]) * f;
                float v = out[i * 2 + c] + s * g * fade;
                out[i * 2 + c] = (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
            }
            pos += step;
            if (fade_step && (fade -= fade_step * (48000.f / rate)) <= 0) { play_locked(-1, 0); break; }
        }
    }
    SDL_UnlockMutex(lock);
}

/* ---- The hooks ------------------------------------------------------------------------------ */
#if GAME_SOR == 3
#define REQUEST_PC (ost_driver() + 4)     /* $1A8004; Bare Knuckle III DX's at $0BBED8 (ost.c) */
#elif GAME_SOR == 2
#define REQUEST_PC 0x040008
#elif GAME_SMPS
#define UPDATE_PC  0x072914
#define QUEUE      0xF00A
#endif

static uint32_t run_original(uint32_t pc) {
    RecompBlock fn = machine_generated(pc);
    return fn ? fn(pc) : (pc | BAIL);   /* not recompiled: the interpreter takes it from here */
}

#ifdef REQUEST_PC
static uint32_t request_hook(uint32_t pc) {
    uint16_t req = (uint16_t)D(0);
    if ((req >> 8) == 1) setw(&D(0), on_request(req, GAME_SOR == 2 ? 0x010C : 0x0000));
#if GAME_SOR == 3
    else if (req == 0 || req == 2) {
        /* The game's pause (0) and its end (2): the file holds and goes on with it. While a
         * file plays the driver stays paused (its song was asked for as 0), so the resume is
         * not passed on: it would bring back the song the driver had before. */
        ensure_lock();
        SDL_LockMutex(lock);
        paused = req == 0 && now;
        bool keep_quiet = req == 2 && replaced;
        SDL_UnlockMutex(lock);
        if (keep_quiet) setw(&D(0), 0x0007);    /* a request the driver ignores */
    }
#else
    else if (!req) music_stop();        /* stops everything */
#endif
    return run_original(pc);
}
#elif defined(UPDATE_PC)
static uint32_t update_hook(uint32_t pc) {
    for (int k = 0; k < 3; k++) {
        uint8_t req = bus.ram[QUEUE + k];
        if (req >= 0x81 && req <= 0x9F) bus.ram[QUEUE + k] = (uint8_t)on_request(req, 0xE1);
        else if (req == 0xE1 || req == 0xE4) music_stop();
        else if (req == 0xE0) start_fade();
    }
    /* The drums' DAC holds its last sample after the stop: a constant level under the file,
     * which the mix's DC filter takes out (sound_end_frame; in Original too while a file
     * plays). The DAC stays on: the voices play through it. */
    return run_original(pc);
}
#endif

void music_hooks(void) {
    replaced = false;
#ifdef REQUEST_PC
    machine_override(REQUEST_PC, request_hook);
#elif defined(UPDATE_PC)
    machine_override(UPDATE_PC, update_hook);
#endif
}

/* ---- Save states ------------------------------------------------------------------------------ */
void music_state(StateIO *s) {
    int32_t song = now_song;
    double at = pos;
    if (!s->saving && s->pos + sizeof song + sizeof at > s->cap) { music_stop(); return; }   /* an older state */
    state_io(s, &song, sizeof song);
    state_io(s, &at, sizeof at);
    if (s->saving || !s->ok) return;
    ensure_lock();
    SDL_LockMutex(lock);
    if (song != now_song) play_locked(song >= 0 && song < OST_SONGS ? song : -1, at);
#if GAME_SOR == 3
    paused = now && (bus.ram[0xF505] & 1);   /* saved while the game was paused (RAM is loaded first) */
#endif
    SDL_UnlockMutex(lock);
}
