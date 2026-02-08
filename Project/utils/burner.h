#ifndef BURNER_H
#define BURNER_H

#include <stdint.h>

/* Calibration done by the supervisor task*/
void Burn_Calibrate(void);

/* Burn CPU cycles for 'ulMillis' milliseconds */
void Burn(uint32_t ulMillis);

#endif