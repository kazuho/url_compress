#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "url_coder.h"

#ifdef BENCHMARK_USE_RDTSC
static unsigned long long rdtsc(void)
{
  unsigned long long x;
  __asm__("rdtsc" : "=A" (x));
  return x;
}
#define BENCHMARK 1
#elif defined(BENCHMARK_USE_MACH)
#include <mach/mach_time.h>
#define rdtsc() mach_absolute_time()
#define BENCHMARK 1
#endif

int main(int argc, char **argv)
{
  char buf[512*1024], cbuf[512*1024], rbuf[512*1024];
  int buflen, cbuflen, rbuflen;
  
  {
    size_t i;
    printf("compressor overrun test.. ");
    fflush(stdout);
    for (i = 0; i < sizeof(buf) - 1; i++) {
      buf[i] = (i % 255) + 1;
    }
    buf[sizeof(buf) - 1] = '\0';
    if (url_compress(buf, sizeof(buf) - 1, cbuf) != -1) {
      printf("failed\n");
    } else {
      printf("ok\n");
    }
  }
  
  {
    size_t i;
    printf("decompressor overrun test.. ");
    fflush(stdout);
    for (i = 0; i < sizeof(cbuf); i++) {
      cbuf[i] = 255;
    }
    if (url_decompress(cbuf, sizeof(cbuf), rbuf) != -1) {
      printf("failed, got: %s\n", rbuf);
    } else {
      printf("ok\n");
    }
  }
  
  {
    size_t orig_size = 0, compressed_size = 0;
    int failed = 0;
#ifdef BENCHMARK
    unsigned long long start, encoder_elapsed = 0, decoder_elapsed = 0;
#endif
    FILE *fp;
    printf("compression test.. ");
    fflush(stdout);
    if (argc != 2 || (fp = fopen(argv[1], "rt")) == NULL) {
      fprintf(stderr, "Usage: %s <url-file>\n", argv[0]);
      exit(1);
    }
    while (fgets(buf, sizeof(buf), fp) != NULL) {
      /* read from file */
      buflen = strlen(buf);
      if (buflen == 0) {
	continue;
      }
      if (buf[buflen - 1] == '\n') {
	buf[--buflen] = '\0';
      }
      /* compress */
#ifdef BENCHMARK
      start = rdtsc();
#endif
      if ((cbuflen = url_compress(buf, buflen, cbuf)) == -1) {
	printf("buffer full (soft error) for: %s\n", buf);
	continue;
      }
#ifdef BENCHMARK
      encoder_elapsed += rdtsc() - start;
#endif
      /* decompress */
#ifdef BENCHMARK
      start = rdtsc();
#endif
      if ((rbuflen = url_decompress(cbuf, cbuflen, rbuf)) == -1) {
	printf("failed to decompress: %s\n", buf);
	failed = 1;
	break;
      }
      rbuf[rbuflen] = '\0';
#ifdef BENCHMARK
      decoder_elapsed += rdtsc() - start;
#endif
      /* compare */
      if (buflen != rbuflen || strcasecmp(buf, rbuf) != 0) {
	printf("result mismatch:\n %s\n %s\n", buf, rbuf);
	failed = 1;
	break;
      }
      orig_size += buflen;
      compressed_size += cbuflen;
    }
    fclose(fp);
    printf(failed ? "\n" : "ok\n");
    printf("\ncompression ratio: %.1f%% (%lu / %lu)\n",
	   100.0 * compressed_size / orig_size + 0.05, compressed_size,
	   orig_size);
#ifdef BENCHMARK
    printf("encoder: %ld Mticks\n", (long)(encoder_elapsed / 1000000));
    printf("decoder: %ld Mticks\n", (long)(decoder_elapsed / 1000000));
#endif
  }
  
  return 0;
}
