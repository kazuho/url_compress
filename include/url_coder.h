#ifndef URL_CODER_H
#define URL_CODER_H

#ifdef __cplusplus
extern "C" {
#endif

#define URL_COMPRESS_BUFSIZE 767 /* the limit comes from maxkeylen of innodb */

  extern int url_compress(const char *url, size_t url_len, char *comp);
  extern int url_decompress(const char *comp, size_t comp_len, char *url);
  extern int url_compress_nextchar(int ch);

#ifdef __cplusplus
}
#endif

#endif
