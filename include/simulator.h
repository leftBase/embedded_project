#ifndef SIMULATOR_H
#define SIMULATOR_H

#ifdef SIMULATOR

#include "event.h"

void sim_push_m4_button(int button_id, int pressed);
void sim_push_q6_key(int key_code, int pressed);
void sim_autotest_note(const char *message);

#endif

#endif
