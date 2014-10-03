#include <pcre.h>

struct pat {
    pcre* pat;
    pcre_extra* study;
};

void pat_setup(struct pat*, const char* pattern);
void pat_cleanup(struct pat*);

gboolean pat_check(struct pat*, const char* test);

