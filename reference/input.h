#ifndef INPUT_H
#define INPUT_H

#include "types.h"

void input_init(void);
InputState input_read(void);
void input_close(void);

#endif