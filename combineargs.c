#include "combineargs.h"
#include <string.h>
#include <malloc.h>

char* vcombineargs(va_list args) {
  const char* arg;
  char* line = NULL;
  ssize_t llen = 0, alen = 0;
  while((arg = va_arg(args,const char*))) {
    alen = strlen(arg);
    if(llen == 0) {
      // make sure there is one byte at the end for the null
      line = malloc(alen+1);
      memcpy(line,arg,alen);
      llen = alen;
    } else {
      // make sure there is one byte at the end for the null
      line = realloc(line,llen+alen+2);
      line[llen] = ' ';
      memcpy(line+llen+1,arg,alen);
      llen += alen + 1;
    }
  }
  if(line != NULL)
    line[llen] = '\0';
  return line;
}

char* combineargs(int derp, ...) {
    va_list args;
    va_start(args,derp);
    char* ret = vcombineargs(args);
    va_end(args);
    return ret;
}

