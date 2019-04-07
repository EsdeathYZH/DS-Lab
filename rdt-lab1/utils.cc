#include "utils.h"

void incNum(seq_nr_t& num, int max){ 
    num = (num + 1) % max;
}

u_int16_t chksum(void *data, u_int8_t len){
  u_int32_t acc = 0;
  u_int16_t src;
  u_int8_t *octetptr = (u_int8_t*)data;
  while (len > 1) {
    src = (*octetptr) << 8;
    octetptr++;
    src |= (*octetptr);
    octetptr++;
    acc += src;
    len -= 2;
  }
  if (len > 0) {
    src = (*octetptr) << 8;
    acc += src;
  }
  acc = (acc >> 16) + (acc & 0x0000ffffUL);
  if ((acc & 0xffff0000UL) != 0) {
    acc = (acc >> 16) + (acc & 0x0000ffffUL);
  }
  src = (u_int16_t)acc;
  return ~src;
}

bool between(seq_nr_t left, seq_nr_t target, seq_nr_t right){
    return ((left <= target && target < right) 
            || (right < left && left <= target) 
            || (target < right && right < left));
}