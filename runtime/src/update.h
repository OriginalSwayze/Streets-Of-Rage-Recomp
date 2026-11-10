/* Updates for the collection (update.c): the newest release on GitHub, checked when the
 * collection opens, and on the player's word downloaded and installed. The installer it
 * downloads runs as `Setup.exe --update`: it rebuilds the games already installed from the
 * ROMs Setup kept in the install folder, and keeps the settings and saves. */
#ifndef UPDATE_H
#define UPDATE_H

#include <stdbool.h>

enum { UPDATE_IDLE, UPDATE_CHECKING, UPDATE_NONE, UPDATE_AVAILABLE, UPDATE_DOWNLOADING, UPDATE_READY, UPDATE_FAILED };

void update_check(void);                /* starts the check (a thread of its own) */
int  update_state(void);
const char *update_installed(void);     /* "2.8.3", or "" when Setup did not install this copy */
const char *update_latest(void);        /* the newest release's version, once checked */
float update_progress(void);            /* 0-1 while downloading */
const char *update_error(void);         /* why it failed */
void update_download(void);             /* starts the download (a thread of its own) */
bool update_run(void);                  /* starts the downloaded installer; the caller then quits */
/* Tests: a state to draw (with the release and the download's progress), and the file. */
void update_pretend(int state, const char *installed_version, const char *latest_version, float progress);
const char *update_file(void);

#endif
