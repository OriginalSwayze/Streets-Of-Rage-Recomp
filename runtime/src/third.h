/* A third fighter on a third controller (third.c): Streets of Rage 2. */
#ifndef THIRD_H
#define THIRD_H

#include <stdint.h>

extern uint16_t third_pad;              /* PAD_* bits of the third controller */

void third_hooks(void);                 /* after every machine reset (machine_override) */
void third_frame(void);                 /* before each frame: joining, the frame's graphics */

#endif
