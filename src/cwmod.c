/*  cwmod.c */

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


#define CWMOD_DEBUG

#ifdef ARDUINO
#include <Arduino.h>
#endif

#ifdef CWMOD_DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#endif

#include "dspint.h"
#include "cwmod.h"

// const uint8_t cwmod_bit_mask[8] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };

/*  This is formula round(10 * 3^(n/4))  n=1 to 16 */
const uint16_t cwmod_timing_histogram_bins[CWMOD_TIMING_BINS] =
{  13, 17, 23, 30, 39, 52, 68, 90, 118, 156, 205, 270, 355, 468, 615, 810 };


/* FROM ITU-R M.1677-1 */
/* zero dit, 1 dah */
const cwmod_symbol morse_pattern[]  =
{
    { 0b010101,   6, '.' },
    { 0b110011,   6, ',' },
    { 0b111000,   6, ':' },
    { 0b001100,   6, '?' },
    { 0b011110,   6, '\'' },
    { 0b100001,   6, '-' },
    { 0b101101,   6, ']' },
    { 0b010010,   6, '\"' },
    { 0b01111,    5, '1' },
    { 0b00111,    5, '2' },
    { 0b00011,    5, '3' },
    { 0b00001,    5, '4' },
    { 0b00000,    5, '5' },
    { 0b10000,    5, '6' },
    { 0b11000,    5, '7' },
    { 0b11100,    5, '8' },
    { 0b11110,    5, '9' },
    { 0b11111,    5, '0' },
    { 0b10010,    5, '/' },
    { 0b10110,    5, '[' },
    { 0b10001,    5, '=' },
    { 0b00010,    5, '!' },
    { 0b01010,    5, '+' },
    { 0b1000,     4, 'B' },
    { 0b1010,     4, 'C' },
    { 0b0010,     4, 'F' },
    { 0b0000,     4, 'H' },
    { 0b0111,     4, 'J' },
    { 0b0100,     4, 'L' },
    { 0b0110,     4, 'P' },
    { 0b1101,     4, 'Q' },
    { 0b0001,     4, 'V' },
    { 0b1001,     4, 'X' },
    { 0b1011,     4, 'Y' },
    { 0b1100,     4, 'Z' },
    { 0b100,      3, 'D' },
    { 0b110,      3, 'G' },
    { 0b101,      3, 'K' },
    { 0b111,      3, 'O' },
    { 0b010,      3, 'R' },
    { 0b000,      3, 'S' },
    { 0b001,      3, 'U' },
    { 0b011,      3, 'W' },
    { 0b01,       2, 'A' },
    { 0b00,       2, 'I' },
    { 0b11,       2, 'M' },
    { 0b10,       2, 'N' },
    { 0b0,        1, 'E' },
    { 0b1,        1, 'T' }
};

/* insert a timing the fifo.  this is intended to be interrupt safe */
uint8_t cw_insert_into_timing_fifo(uint16_t tim)
{
    uint8_t next = ps.cs.timing_head >= (CWMOD_TIMING_LENGTHS - 1) ? 0 : (ps.cs.timing_head+1);
    if (next == ps.cs.timing_tail) return 0;
    ps.cs.timing_lengths[next] = tim;
    ps.cs.timing_head = next;
    return 1;
}

uint8_t cw_fifo_available(void)
{
    uint8_t head = ps.cs.timing_head;
    return ps.cs.timing_tail > head ? ps.cs.timing_tail - head : ps.cs.timing_tail - head + CWMOD_TIMING_LENGTHS;
}

/* remove a timing from the fifo.  this is intended to be interrupt safe */
uint16_t cw_remove_from_timing_fifo(void)
{
    uint16_t tim;
    uint8_t next;
    if (ps.cs.timing_tail == ps.cs.timing_head) return 0;
    next = ps.cs.timing_tail >= (CWMOD_TIMING_LENGTHS - 1) ? 0 : (ps.cs.timing_tail+1);
    tim = ps.cs.timing_lengths[next];
    ps.cs.timing_tail = next;
    return tim;
}

/* peek from the timing fifo.  this is intended to be interrupt safe */
uint16_t cw_peek_from_timing_fifo(void)
{
    uint16_t tim;
    uint8_t next;
    if (ps.cs.timing_peek_tail == ps.cs.timing_head) return 0;
    next = ps.cs.timing_peek_tail >= (CWMOD_TIMING_LENGTHS - 1) ? 0 : (ps.cs.timing_peek_tail+1);
    tim = ps.cs.timing_lengths[next];
    ps.cs.timing_peek_tail = next;
    return tim;
}

void cw_initialize(uint8_t wide, uint8_t spaces_from_mark_timing,
                   uint8_t smooth, uint8_t sticky_interval_length)
{
   dsp_initialize_cw(wide);
   memset(&ps.cs,'\000',sizeof(ps.cs));

   ps.cs.protocol = PROTOCOL_CW;
   ps.cs.keydown_threshold = CWMOD_THRESHOLD_MIN;
   ps.cs.keyup_threshold = CWMOD_THRESHOLD_MIN >> 1;
   ps.cs.spaces_from_mark_timing = spaces_from_mark_timing;
   smooth = smooth > CWMOD_SMOOTH_SHIFT_LENGTH ? CWMOD_SMOOTH_SHIFT_LENGTH : smooth;
   ps.cs.ct_smooth = smooth;
   ps.cs.ct_smooth_ind_max = 1 << smooth;
   ps.cs.sticky_interval_length = sticky_interval_length;
   ps.cs.ct_min_val = 0xFFFF;
}

void cw_reset_threshold(uint16_t thresh)
{
   if ((thresh >> 5) > ps.cs.ct_min_val)
        thresh >>= 1;
   //printf("thresh=%d %d\n",thresh,ps.cs.ct_min_val);
   thresh = (thresh < CWMOD_THRESHOLD_MIN) ? CWMOD_THRESHOLD_MIN : thresh;
   ps.cs.keydown_threshold = thresh;
   ps.cs.keyup_threshold = ps.cs.keydown_threshold >> 1;
   ps.cs.ct_average = 0;
   ps.cs.ct_sum = 0;
   ps.cs.ct_min_val = 0xFFFF;
}

void cw_new_sample(void)
{
    uint16_t mag_sample, c1, ctr;
    int16_t edge_val;

    /* only proceed if we have new magnitude samples */
    ctr = ds.sample_ct;
    if (ctr == ps.cs.last_sample_ct)
        return;
    ps.cs.last_sample_ct = ctr;

    ps.cs.total_ticks++;  /* increment the total ticks counter */

    mag_sample = ds.mag_value_12;
    ps.cs.ct_sum += mag_sample;
    if (ps.cs.ct_smooth)
    {
      ps.cs.ct_smooth_sum += mag_sample - ps.cs.ct_smooth_mag[ps.cs.ct_smooth_ind];
      ps.cs.ct_smooth_mag[ps.cs.ct_smooth_ind] = mag_sample;
      if ((++ps.cs.ct_smooth_ind) >= ps.cs.ct_smooth_ind_max)
        ps.cs.ct_smooth_ind = 0;
      switch (ps.cs.ct_smooth)
      {
          case 1: mag_sample = ps.cs.ct_smooth_sum >> 1; break;
          case 2: mag_sample = ps.cs.ct_smooth_sum >> 2; break;
          case 3: mag_sample = ps.cs.ct_smooth_sum >> 3; break;
          case 4: mag_sample = ps.cs.ct_smooth_sum >> 4; break;
      }
    }
    if (ps.cs.ct_min_val > mag_sample)
        ps.cs.ct_min_val = mag_sample;

    if (ps.cs.keydown_threshold <= CWMOD_THRESHOLD_MIN)
    {
       if ((++ps.cs.ct_average) >= (1 << (CWMOD_AVG_CT_PWR2-2)))
          cw_reset_threshold( (ps.cs.ct_sum) >> (CWMOD_AVG_CT_PWR2-2) );
    } else
    {
       if ((++ps.cs.ct_average) >= (1 << (CWMOD_AVG_CT_PWR2)))
          cw_reset_threshold( (ps.cs.ct_sum) >> (CWMOD_AVG_CT_PWR2) );
    }
    ps.cs.state_ctr++;
    if (ps.cs.key_state)
    {
       if (mag_sample < ps.cs.keyup_threshold)
       {
          if ((++ps.cs.sticky_interval) >= ps.cs.sticky_interval_length)
          {
            ps.cs.key_state = 0;
            //printf("key up %d %d\n",ps.cs.state_ctr, mag_sample);
            cw_insert_into_timing_fifo(ps.cs.state_ctr);
            ps.cs.state_ctr = 0;
            ps.cs.sticky_interval = 0;
          }
       } else ps.cs.sticky_interval = 0;
    } else
    {
       if (mag_sample > ps.cs.keydown_threshold)
       {
          if ((++ps.cs.sticky_interval) >= ps.cs.sticky_interval_length)
          {
            ps.cs.key_state = 1;
            //printf("key down %d %d\n",ps.cs.state_ctr, mag_sample);
            cw_insert_into_timing_fifo(((uint16_t)ps.cs.state_ctr) | 0x8000);
            ps.cs.state_ctr = 0;
            ps.cs.sticky_interval = 0;
            ps.cs.keyup_threshold = mag_sample >> 1;
          }
       } else ps.cs.sticky_interval = 0;
    }
}

void cw_find_two_greatest(uint8_t array[], uint8_t length, uint8_t sep,
                          uint8_t *e1, uint8_t *e2)
{
    uint8_t gr_val, i, gr, gr2;

    gr_val = 0;
    for (i=1;i<length;i++)
    {
        uint8_t val = array[i-1] + array[i];
        if (val > gr_val)
        {
            gr_val = val;
            gr = i;
        }
    }

    gr_val = 0;
    for (i=1;i<length;i++)
    {
        if ((i > (gr+sep)) || ((sep+i) < gr))
        {
            uint8_t val = array[i-1] + array[i];
            if (val > gr_val)
            {
                gr_val = val;
                gr2 = i;
            }
        }
    }

    if (gr < gr2)
    {
        *e1 = gr;
        *e2 = gr2;
    } else
    {
        *e1 = gr2;
        *e2 = gr;
    }
}

void cw_decode_process(void)
{
    uint8_t is_mark, bin, decode_now;
    uint16_t dly, tim = cw_peek_from_timing_fifo();
    is_mark = (tim & 0x8000) == 0;
    tim = tim & 0x7FFF;

    dly = ps.cs.total_ticks - ps.cs.last_tick;
    /* assume a really short timing is a glitch */
    decode_now = (dly >= CWMOD_PROCESS_DELAY) || (cw_fifo_available() < CWMOD_FIFO_DECODE_THRESHOLD);
    if ((!decode_now) && (tim < 10)) return;
    ps.cs.last_tick = ps.cs.total_ticks;

    if (tim > 0)
    {
        for (bin=0; bin < CWMOD_TIMING_BINS; bin++)   /* search for histogram bin */
            if (tim < *(&cwmod_timing_histogram_bins[bin])) break;
        if (bin < CWMOD_TIMING_BINS)
        {
            if (is_mark) ps.cs.histogram_marks[bin]++;
                else ps.cs.histogram_spaces[bin]++;
        }
    }

    if (!decode_now) return;

#if 0
    printf("\nmark bin:  ");
    for (bin=0; bin<(CWMOD_TIMING_BINS); bin++)
        printf("%02d,",ps.cs.histogram_marks[bin]);
    printf("\nspace bin: ");
    for (bin=0; bin<(CWMOD_TIMING_BINS); bin++)
        printf("%02d,",ps.cs.histogram_spaces[bin]);
    printf("\n");
#endif


    uint8_t g1m, g2m, g1s, g2s;

    cw_find_two_greatest(ps.cs.histogram_marks, sizeof(ps.cs.histogram_marks)/sizeof(ps.cs.histogram_marks[0]),
                        2, &g1m, &g2m);


    ps.cs.dit_dah_threshold = *(&cwmod_timing_histogram_bins[(g1m+g2m)/2]);

    if (ps.cs.spaces_from_mark_timing)
    {
        g1s = g1m;
        g2s = g2m;
    } else
    {
        cw_find_two_greatest(ps.cs.histogram_spaces, sizeof(ps.cs.histogram_spaces)/sizeof(ps.cs.histogram_spaces[0]),
                         2, &g1s, &g2s);
        int8_t temp1 = g1s-g2m;
        int8_t temp2 = g2s-g1m;
        if (temp1 < 0) temp1 = -temp1;
        if (temp2 < 0) temp2 = -temp2;
        if ((temp1 < 2) || (temp2 < 2))
        {
            g1s = g1m;
            g2s = g2m;
        }
    }
   // printf(" lm: %d/%d %d/%d\n",g1m,g2m,g1s,g2s);

    ps.cs.intrainterspace_threshold = *(&cwmod_timing_histogram_bins[(g1s+g2s)/2]);
    ps.cs.interspaceword_threshold = 3*ps.cs.intrainterspace_threshold;


    // printf("ls: %d/%d %d/%d\n",g1,ps.cs.histogram_spaces[g1],g2,ps.cs.histogram_spaces[g2]);

    //  printf("x: %d %d %d\n",ps.cs.dit_dah_threshold,ps.cs.intrainterspace_threshold,ps.cs.interspaceword_threshold);

    for (;;)
    {
        tim = cw_remove_from_timing_fifo();
        if (tim == 0) break;
        is_mark = (tim & 0x8000) == 0;
        tim = tim & 0x7FFF;
        if (is_mark)
        {
            ps.cs.num_ditdahs++;
            ps.cs.cur_ditdahs <<= 1;
            if (tim > ps.cs.dit_dah_threshold)
                ps.cs.cur_ditdahs |= 1;
      //      printf("%c",tim > ps.cs.dit_dah_threshold ? '-' : '.');
        } else
        {
            if (tim > ps.cs.intrainterspace_threshold)
            {
               const cwmod_symbol *cws = &morse_pattern[0];
               while (cws < (&morse_pattern[(sizeof(morse_pattern)/sizeof(cwmod_symbol))]))
               {
                   int8_t diff = ps.cs.num_ditdahs - *(&cws->num);
                   if (diff >= 0)
                   {
                       uint8_t test = ps.cs.cur_ditdahs >> diff;
                       if (*(&cws->cwbits) == test)
                       {
                            //printf("%c",cws->symbol);
                            decode_insert_into_fifo(*(&cws->symbol));
                            break;
                       }
                   }
                   cws++;
               }
               ps.cs.num_ditdahs = 0;
               ps.cs.cur_ditdahs = 0;
             //  if (tim > ps.cs.interspaceword_threshold)
             //      printf(" ");
               decode_insert_into_fifo(' ');
            }
        }
    }
    // printf("!!!\n");


    for (g1m=0;g1m<(sizeof(ps.cs.histogram_marks)/sizeof(ps.cs.histogram_marks[0]));g1m++)
    {
        uint16_t temp = ps.cs.histogram_marks[g1m];
        ps.cs.histogram_marks[g1m] = (temp+temp+temp) >> 2;
    }
    for (g1m=0;g1m<(sizeof(ps.cs.histogram_spaces)/sizeof(ps.cs.histogram_spaces[0]));g1m++)
    {
        uint16_t temp = ps.cs.histogram_spaces[g1m];
        ps.cs.histogram_spaces[g1m] = (temp+temp+temp) >> 2;

    }
}

#ifdef CWMOD_DEBUG

#define CWDIR "e:\\nov-15-2021-backup\\projects\\RFBitBangerNew\\Ignore\\processed-cw\\"

//const char filename[]=CWDIR"kw4ti-msg.wav";
//const char filename[]=CWDIR"wnu-processed.wav";
//const char filename[]=CWDIR"n1ea-processed.wav";
//const char filename[]=CWDIR"vix-processed.wav";
//const char filename[]=CWDIR"px-processed.wav";
//const char filename[]=CWDIR"kfs-processed.wav";
//const char filename[]=CWDIR"cootie-processed.wav";
//const char filename[]=CWDIR"wxdewcc-processed.wav";
//const char filename[]=CWDIR"ejm8-processed.wav";
//const char filename[]=CWDIR"offair1-processed.wav";
//const char filename[]=CWDIR"offair2-processed.wav";
//const char filename[]=CWDIR"offair3-processed.wav";
const char filename[]=CWDIR"offair4-processed.wav";

void test_cwmod_decode()
{
   FILE *fp = fopen(filename,"rb");
   cw_initialize(0, 0, 2, 6);
   ds.slow_samp_num = 4;
   int samplecount = 0;

   if (fp == NULL)
   {
       printf("Could not open file %s\n",filename);
       return;
   }

   fseek(fp,44*sizeof(uint8_t),SEEK_SET);
   while (!feof(fp))
   {
      // if ((ftell(fp) % 16000) == 0) sleep(1);
       int16_t sample;
       fread((void *)&sample,1,sizeof(int16_t),fp);
       sample = sample / 32 + 512;
//       if ((sample<200) || (sample > 800)) printf("sample=%d\n",sample);
       /* if ((samplecount/1024) & 0x1) sample = 512;
        else
       sample = 512 - 64 * cos(2*M_PI*samplecount/64.0); */
       dsp_interrupt_sample(sample);
       cw_new_sample();
       cw_decode_process();
       samplecount++;
   }
   for (samplecount=0;samplecount<360000;samplecount++)
   {
       dsp_interrupt_sample(512);
       cw_new_sample();
       cw_decode_process();
   }
}
#endif /* CWMOD_DEBUG */
