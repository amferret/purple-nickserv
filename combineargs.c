#include "combineargs.h"
#include <string.h>
#include <malloc.h>

struct lenmalloc_s {
    void* ptr;
    ssize_t amount;
};

void lenmalloc(struct lenmalloc_s* thing, size_t amount) {
    thing->ptr = malloc(amount);
    thing->amount = amount;
    return ;
}

void lenrealloc(struct lenmalloc_s* thing, size_t amount) {
    // FINE :p
    // I'm pretty sure realloc already does this.
    // size_t adjusted = ((int)(amount / 512 + 1)) * 512;
    // only realloc when we need more space in 512 byte chunks
    size_t adjusted = (amount >> 8 + 1) << 8;
    if(adjusted > thing->amount) {
        thing->amount = adjusted;
        thing->ptr = realloc(thing->ptr,adjusted);
    }
}

char* vcombineargs(va_list args) {
  const char* arg;
  struct lenmalloc_s thing = { NULL, 0 };
  ssize_t llen = 0, alen = 0;
  char* line = NULL;
  while((arg = va_arg(args,const char*))) {
    alen = strlen(arg);
    if(llen == 0) {
      // make sure there is one byte at the end for the null
      lenmalloc(&thing,alen+1);
      line = (char*)thing.ptr;
      memcpy(line,arg,alen);
      llen = alen;
    } else {
      // make sure there is one byte at the end for the null
      lenrealloc(&thing,llen+alen+2);
      line[llen] = ' ';
      memcpy(line+llen+1,thing.ptr,alen);
      llen += alen + 1;
    }
  }
  if(line != NULL)
    line[llen] = '\0';
  return line;
}
