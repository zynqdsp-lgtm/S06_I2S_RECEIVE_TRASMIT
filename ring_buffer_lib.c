#include "ring_buffer_lib.h"
#include <assert.h>

void ring_buffer_init(ring_buffer_t *ring_buf, uint8_t *buf, uint16_t buf_len) {
  assert(ring_buf);
  assert(buf);
  assert(buf_len > 0);

  ring_buf->buf = buf;
  ring_buf->bufsize = buf_len;
  ring_buf->in_idx = 0;
  ring_buf->out_idx = 0;
  ring_buf->num_buffered = 0;
}

static void ring_buffer_push_ovr_core(ring_buffer_t *ring_buf,
                                      const uint8_t *vals, uint16_t nvals) {
  assert(ring_buf);
  assert(vals);
  for (uint16_t idx = 0; idx < nvals; idx++) {
    ring_buf->buf[ring_buf->in_idx] = vals[idx];
    ring_buf->in_idx = ((ring_buf->in_idx + 1) % ring_buf->bufsize);
    if (ring_buf->num_buffered < ring_buf->bufsize) {
      ++ring_buf->num_buffered;
    } else {
      // buffer full
      ring_buf->out_idx = ring_buf->in_idx;
    }
  }
}

void ring_buffer_push_ovr(ring_buffer_t *ring_buf, const uint8_t *vals,
                          uint16_t nvals) {

  ring_buffer_push_ovr_core(ring_buf, vals, nvals);
}

static uint16_t ring_buffer_pop_core(ring_buffer_t *ring_buf, uint8_t *vals,
                                     uint16_t maxvals) {
  assert(ring_buf);
  assert(vals);
  uint16_t npopped = 0;
  while (ring_buf->num_buffered > 0 && npopped < maxvals) {
    vals[npopped++] = ring_buf->buf[ring_buf->out_idx];
    ring_buf->out_idx = (ring_buf->out_idx + 1) % ring_buf->bufsize;
    --ring_buf->num_buffered;
  }
  return npopped;
}

uint16_t ring_buffer_pop(ring_buffer_t *ring_buf, uint8_t *vals,
                         uint16_t maxvals) {

  uint16_t result = ring_buffer_pop_core(ring_buf, vals, maxvals);

  return result;
}