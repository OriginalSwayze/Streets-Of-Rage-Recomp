/* The single-file libraries' code (third_party/single), built once here and without the
 * project's warnings: the music decoders for where Windows' Media Foundation is not there
 * (music.c), and the TrueType rasteriser for the fonts in fonts/ (text.c). */
#ifndef _WIN32
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "stb_vorbis.c"
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
