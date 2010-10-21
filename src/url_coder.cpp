extern "C" {
#include <assert.h>
}
#include <algorithm>
#include "url_coder.h"
#include "range_coder.hpp"
#include  "rctable.c"

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

int url_compress(const char *url, char *comp)
{
  try {
    char *comp_pos = comp;
    rc_encoder_t<writer_t>
      enc(writer_t(&comp_pos, comp + URL_COMPRESS_BUFSIZE));
    unsigned short ctx = 0;
    const char *p = url;
    
    while (*p != '\0') {
      const pred_map_t &m = predmaps[predidx[ctx]];
      int pch = (unsigned char)*p++, percch;
      if (pch == '%' && (percch = decpc(p)) != 0) {
	pch = percch;
	p += 2;
      }
      assert(m[pch] != m[pch + 1]);
      enc.encode(m[pch], m[pch + 1], m[256]);
      ctx = ctx * 256 + pch;
    }
    
    const pred_map_t &m = predmaps[predidx[ctx]];
    enc.encode(m[0], m[1], m[256]);
    enc.final();
    
    return comp_pos - comp;
  } catch (writer_t::overrun_t e) {
    return -1;
  }
}

int url_decompress(const char *comp, size_t comp_len, char *url)
{
  rc_decoder_t<const char*, 256> dec(comp, comp + comp_len);
  char *d = url, *max = url + URL_COMPRESS_BUFSIZE - 1;
  unsigned short ctx = 0;
  while (1) {
    const pred_map_t &m = predmaps[predidx[ctx]];
    unsigned ch = dec.decode(m[256], m);
    ctx = ctx * 256 + ch;
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
  *d = '\0';
  return d - url;
 Error:
  return -1;
}
