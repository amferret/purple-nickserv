#include <pcre.h>
#include <glib.h>

struct pat;

struct pat* pat_setup(const char* pattern, gboolean plain);
void pat_cleanup(struct pat*);

gboolean pat_check(struct pat*, const char* test);

