
/*  dspint.h */

/*
 * Copyright (c) 2021 Daniel Marks

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
 */

#ifndef __DSPINT_H
#define __DSPINT_H

#ifdef __cplusplus
extern "C" {
#endif

#define SCAMP_PROTOCOL
#define CW_PROTOCOL
#define RTTY_PROTOCOL

#ifdef SCAMP_PROTOCOL
#include "scamp.h"
#endif /* SCAMP_PROTOCOL */
#ifdef CW_PROTOCOL
#include "cwmod.h"
#endif /* CW_PROTOCOL */
#ifdef RTTY_PROTOCOL
#include "rtty.h"
#endif /* RTTY_PROTOCOL */

#ifdef SCAMP_VERY_SLOW_MODES
#define DSPINT_MAX_SAMPLEBUFFER 128
#else
#define DSPINT_MAX_SAMPLEBUFFER 64
#endif



#define SCAMP_SYNC_CODEWORD 0b111110110100011001110100011110ul
                              /* 0x3ED19D1E */

#define SCAMP_BLANK_CODEWORD 0xAAAAAAAA

#define PROTOCOL_SCAMP   0
#define PROTOCOL_CW      1
#define PROTOCOL_RTTY    2

#define MOD_TEST 5

#if MOD_TEST==1
#define MOD_TYPE SCAMP_OOK_FAST
#define MOD_REP 32
#define MOD_CHAN1 16.0
#define MOD_CHAN2 99999999999.0
#elif MOD_TEST==1
#define MOD_TYPE SCAMP_OOK
#define MOD_REP 64
#define MOD_CHAN1 16.0
#define MOD_CHAN2 99999999999.0
#elif MOD_TEST==2
#define MOD_TYPE SCAMP_FSK
#define MOD_REP 60
#define MOD_CHAN1 12.0
#define MOD_CHAN2 20.0
#elif MOD_TEST==3
#define MOD_TYPE SCAMP_FSK_FAST
#define MOD_REP 24
#define MOD_CHAN1 8.0
#define MOD_CHAN2 12.0
#elif MOD_TEST==4
#define MOD_TYPE SCAMP_OOK_SLOW
#define MOD_REP 128
#define MOD_CHAN1 16.0
#define MOD_CHAN2 9999999999.0
#elif MOD_TEST==5
#define MOD_TYPE SCAMP_FSK_SLOW
#define MOD_REP 120
#define MOD_CHAN1 12.0
#define MOD_CHAN2 20.0
#endif




/* this is stuff that is initialized when the modulation mode
   is changed and doesn't change otherwise */
typedef struct _dsp_state_fixed
{
  uint8_t   buffer_size;

  uint8_t   dly_8;
  uint8_t   dly_12;
  uint8_t   dly_16;
  uint8_t   dly_20;
  uint8_t   dly_24;

} dsp_state_fixed;

/* this is the current state of the demodulator and is designed
   so that is might be reset quickly without disturbing the
   dsp_state_fixed state */
typedef struct _dsp_state
{
  uint8_t   slow_samp;
  uint8_t   slow_samp_num;
  uint16_t  total_num;

  uint8_t   sample_no;
  uint8_t   sample_ct;

  uint16_t  ct_average;
  uint32_t  ct_sum;

  int8_t    current_bit_no;
  uint32_t  current_word;

  uint8_t   count_8;
  uint8_t   count_12;
  uint8_t   count_16;
  uint8_t   count_20;
  uint8_t   count_24;

  int32_t  state_i_8;
  int32_t  state_i_12;
  int32_t  state_i_16;
  int32_t  state_i_20;
  int32_t  state_i_24;

  int32_t  state_q_8;
  int32_t  state_q_12;
  int32_t  state_q_16;
  int32_t  state_q_20;
  int32_t  state_q_24;

  uint16_t  mag_value_8;
  uint16_t  mag_value_12;
  uint16_t  mag_value_16;
  uint16_t  mag_value_20;
  uint16_t  mag_value_24;
  uint16_t  sample_buffer[DSPINT_MAX_SAMPLEBUFFER];

} dsp_state;

typedef struct OutData_t
{
  uint32_t   OutCodes[100];
  uint32_t   OutLength;
} OutData_t;


extern dsp_state       ds;
extern dsp_state_fixed df;

typedef union _protocol_state
{
#ifdef SCAMP_PROTOCOL
    scamp_state  ss;
#endif
#ifdef CW_PROTOCOL
    cwmod_state  cs;
#endif
#ifdef RTTY_PROTOCOL
    rtty_state   rs;
#endif
} protocol_state;

extern protocol_state  ps;

void dsp_interrupt_sample(uint16_t sample);
void dsp_new_sample(void);
void dsp_initialize_scamp(uint8_t mod_type);
void dsp_initialize_cw(uint8_t wide);
void dsp_initialize_rtty(void);
void dsp_reset_state(void);
uint16_t dsp_get_signal_magnitude(void);

void dsp_initialize_protocol(uint8_t protocol);

void dsp_initialize_frame_fifo(volatile scamp_frame_fifo *dff);

#define DECODE_FIFO_LENGTH 16

/* structure for decode fifo */
typedef struct _decode_fifo
{
    uint8_t   head;
    uint8_t   tail;
    uint8_t   decodes[DECODE_FIFO_LENGTH];
} decode_fifo;

void decode_initialize_fifo(void);
uint8_t decode_insert_into_fifo(uint8_t c);
uint16_t decode_remove_from_fifo(void);

void DecodeSample(float);


#ifdef __cplusplus
}
#endif

#endif  /* __DSPINT_H */
