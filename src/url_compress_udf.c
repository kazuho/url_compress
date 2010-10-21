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

#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include <mysql/mysql_com.h>
#include "url_coder.h"

#ifdef __cplusplus
extern "C" {
#endif
my_bool my_url_compress_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void my_url_compress_deinit(UDF_INIT *initid);
char *my_url_compress(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);
my_bool my_url_decompress_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void my_url_decompress_deinit(UDF_INIT *initid);
char *my_url_decompress(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);
#ifdef __cplusplus
}
#endif

my_bool my_url_compress_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  switch (args->arg_count) {
  case 1:
  case 2:
    // ok
    break;
  default:
    strcpy(message, "my_url_compress(url[,return_upper_bound]): invalid argument");
    return 1;
  }
  if ((initid->ptr = (char*)malloc(URL_COMPRESS_BUFSIZE)) == NULL) {
    strcpy(message, "no memory\n");
    return 1;
  }
  args->arg_type[0] = STRING_RESULT;
  args->maybe_null[0] = 0;
  if (args->arg_count == 2) {
    args->arg_type[1] = INT_RESULT;
    args->maybe_null[1] = 0;
  }
  return 0;
}

void my_url_compress_deinit(UDF_INIT *initid)
{
  if (initid->ptr != NULL) {
    free(initid->ptr);
    initid->ptr = NULL;
  }
}

char *my_url_compress(UDF_INIT *initid, UDF_ARGS *args, char *result,
		      unsigned long *length, char *is_null, char *error)
{
  int l;
  if (args->arg_count == 2 && *(long long*)args->args[1]) {
    size_t ub_len = args->lengths[0];
    char *ub = (char*)malloc(ub_len);
    memcpy(ub, args->args[0], ub_len);
    ub[ub_len - 1] = url_compress_nextchar(ub[ub_len - 1]);
    l = url_compress(ub, ub_len, initid->ptr);
    free(ub);
  } else {
    l = url_compress(args->args[0], args->lengths[0], initid->ptr);
  }
  if (l == -1) {
    *error = 1;
    return NULL;
  }
  *length = l;
  *is_null = 0;
  return initid->ptr;
}

my_bool my_url_decompress_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  if (args->arg_count != 1) {
    strcpy(message, "my_url_decompress(url): invalid argument");
    return 1;
  }
  if ((initid->ptr = (char*)malloc(URL_COMPRESS_BUFSIZE)) == NULL) {
    strcpy(message, "no memory\n");
    return 1;
  }
  args->arg_type[0] = STRING_RESULT;
  args->maybe_null[0] = 0;
  return 0;
}

void my_url_decompress_deinit(UDF_INIT *initid)
{
  if (initid->ptr != NULL) {
    free(initid->ptr);
    initid->ptr = NULL;
  }
}

char *my_url_decompress(UDF_INIT *initid, UDF_ARGS *args, char *result,
			unsigned long *length, char *is_null, char *error)
{
  int l;
  if ((l = url_decompress(args->args[0], args->lengths[0], initid->ptr))
      == -1) {
    *error = 1;
    return NULL;
  }
  *length = l;
  *is_null = 0;
  return initid->ptr;
}
