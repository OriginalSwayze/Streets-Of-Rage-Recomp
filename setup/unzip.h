/* Zip files, unpacked in-process (unzip.c). */
#ifndef UNZIP_H
#define UNZIP_H

#include <stdbool.h>
#include <stddef.h>

/* Unpacks the zip that is `len` bytes of `path` from `base` (len < 0: to the end of the file)
 * into the folder `dest`, made if need be. With `strip`, a top folder every entry is in is
 * left out (as tar's --strip-components=1). The files get the time of unpacking, not the
 * zip's. `tick`, if given, is told how many entries are done out of how many. False on any
 * failure, with why in `why`. */
bool unzip(const char *path, long long base, long long len, const char *dest, bool strip,
           void (*tick)(int done, int total), char *why, size_t why_n);

#endif
