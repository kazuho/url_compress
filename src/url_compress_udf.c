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
