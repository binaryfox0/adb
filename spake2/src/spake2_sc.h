#ifndef SPAKE2_SC_H
#define SPAKE2_SC_H

#include <stdint.h>

/*
The set of scalars is \Z/l
where l = 2^252 + 27742317777372353535851937790883648493.
*/

void spake2__sc_reduce(uint8_t *s);
void spake2__sc_muladd(uint8_t *s, const uint8_t *a, const uint8_t *b, const uint8_t *c);

#endif
