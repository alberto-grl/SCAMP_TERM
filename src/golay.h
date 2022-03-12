/*  golay.h */

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

#ifndef __GOLAY_H
#define __GOLAY_H

uint32_t golay_encode(uint16_t wd_enc);
uint16_t golay_decode(uint32_t codeword, uint8_t *biterrs);
uint8_t golay_hamming_weight_16(uint16_t n);
uint8_t count_reversals(uint32_t);

#ifdef __cplusplus
}
#endif

#endif /* __GOLAY_H */
