/*
 * Copyright (c) 2008, Cybozu Labs, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the Cybozu Labs, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

extern "C" {
#include <limits.h>
}
#include  <algorithm>
#include <cstdio>
#include "url_coder.h"
#include "range_coder.hpp"
#include "rctable.c"
#ifdef USE_CHARMAP
#include "chreorder.c"
#endif

using namespace std;

inline unsigned decpc(const char *p)
{
  int hi = p[0];
  if ('8' <= hi && hi <= '9') {
    hi -= '0';
  } else if ('A' <= hi && hi <='F') {
    hi = hi - 'A' + 0xa;
  } else if ('a' <= hi && hi <= 'f') {
    hi = hi - 'a' + 0xa;
  } else {
    return 0;
  }
  int lo = p[1];
  if ('8' <= lo && lo <= '9') {
    lo -= '0';
  } else if ('A' <= lo && lo <='F') {
    lo = lo - 'A' + 0xa;
  } else if ('a' <= lo && lo <= 'f') {
    lo = lo - 'a' + 0xa;
  } else {
    return 0;
  }
  return hi * 16 + lo;
}

class writer_t {
  char **p, *max;
public:
  struct overrun_t {
  };
  writer_t(char **_p, char *_max) : p(_p), max(_max) {}
  writer_t &operator=(char c) {
    if (*p == max) {
      throw overrun_t();
    }
    *(*p)++ = c;
    return *this;
  }
  writer_t &operator*() { return *this; }
  writer_t &operator++() { return *this; }
  writer_t &operator++(int) { return *this; }
};

inline int get_pred_map(int p3, int p2, int p1)
{
  int idx = predidx1[p1];
  if (idx < NUM_PRED_MAPS) {
    return idx;
  }
  int idx2 = predidx2[idx - NUM_PRED_MAPS + p2];
  if (idx2 < NUM_PRED_MAPS) {
    return idx2;
  }
  return predidx3[idx2 - NUM_PRED_MAPS + p3];
}
  
int url_compress(const char *url, size_t url_len, char *comp)
{
  try {
    char *comp_pos = comp;
    rc_encoder_t<writer_t>
      enc(writer_t(&comp_pos, comp + URL_COMPRESS_BUFSIZE));
    int prev3 = 0, prev2 = 0, prev1 = 0;
    const char *p = url, *pmax = url + url_len;
    
    while (p != pmax) {
      const pred_map_t &m = predmaps[get_pred_map(prev3, prev2, prev1)];
      int pch = (unsigned char)*p++, percch;
      if (pch == '%' && pmax - p >= 2 && (percch = decpc(p)) != 0) {
	pch = percch;
	p += 2;
      }
#ifdef USE_CHARMAP
      pch = charmap_to_ordered[pch];
#endif
      if (m[pch] == m[pch + 1]) {
	fprintf(stderr, "predmaps[%d][%d]==0\n",
		get_pred_map(prev3, prev2, prev1), pch);
	return -1;
      }
      enc.encode(m[pch] - PRED_BASE, m[pch + 1] - PRED_BASE, PRED_MAX);
      prev3 = prev2, prev2 = prev1, prev1 = pch;
    }
    
    const pred_map_t &m = predmaps[get_pred_map(prev3, prev2, prev1)];
    enc.encode(m[0] - PRED_BASE, m[1] - PRED_BASE, PRED_MAX);
    enc.final();
    
    return comp_pos - comp;
  } catch (writer_t::overrun_t e) {
    return -1;
  }
}

int url_decompress(const char *comp, size_t comp_len, char *url)
{
  rc_decoder_t<const char*, rc_decoder_search_t<short, 256, PRED_BASE> >
    dec(comp, comp + comp_len);
  char *d = url, *max = url + URL_COMPRESS_BUFSIZE;
  int prev3 = 0, prev2 = 0, prev1 = 0;
  while (1) {
    const pred_map_t &m = predmaps[get_pred_map(prev3, prev2, prev1)];
    unsigned ch = dec.decode(PRED_MAX, m);
    prev3 = prev2, prev2 = prev1, prev1 = ch;
#ifdef USE_CHARMAP
    ch = charmap_from_ordered[ch];
#endif
    if (ch == '\0') {
      break;
    } else if (ch >= 0x80) {
      if (d + 3 > max) {
	goto Error;
      }
      *d++ = '%';
      *d++ = ("0123456789ABCDEF")[ch >> 4];
      *d++ = ("0123456789ABCDEF")[ch & 15];
    } else {
      if (d == max) {
	goto Error;
      }
      *d++ = ch;
    }
  }
  return d - url;
 Error:
  return -1;
}

int url_compress_nextchar(int ch)
{
#ifdef USE_CHARMAP
  return charmap_from_ordered[(unsigned char)(charmap_to_ordered[ch] + 1)];
#else
  return (ch + 1) & 255;
#endif
}
