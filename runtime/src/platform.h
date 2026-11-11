/* What differs between Windows and Linux in what the player reads. */
#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
#define INSTALLER       "Setup"         /* what installs a game, or adds one */
#define INSTALLER_AGAIN "run Setup again (Settings > Apps > Streets of Rage Collection > Modify)"
#else
#define INSTALLER       "install.sh"
#define INSTALLER_AGAIN "run tools/install.sh again"
#endif

#endif
