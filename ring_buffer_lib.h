#ifndef RING_BUFFER_LIB_H
#define RING_BUFFER_LIB_H
#include <stdint.h>

typedef struct ring_buffer_s {
  uint8_t *buf;
  uint16_t bufsize;
  uint16_t in_idx;
  uint16_t out_idx;

  uint16_t num_buffered;

} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *ring_buf, uint8_t *buf, uint16_t buf_len);

void ring_buffer_push_ovr(ring_buffer_t *ring_buf, const uint8_t *vals,
                          uint16_t nvals);

uint16_t ring_buffer_pop(ring_buffer_t *ring_buf, uint8_t *vals,
                         uint16_t maxvals);

#endif