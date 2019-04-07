#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#include <stdlib.h>

const int WINDOW_SIZE = 10;
const int SEQ_SIZE = 128; // 2^8 
const double TIME_OUT = 0.3;

typedef unsigned int seq_nr_t;

void incNum(seq_nr_t& num, int max);

u_int16_t chksum(void *data, u_int8_t len);

bool between(seq_nr_t left, seq_nr_t target, seq_nr_t right);
#endif
