#include <pcre.h>
#include <glib.h>

enum pat_mode {
    plain,
    pcre,
    match
};

struct pat;

struct pat* pat_setup(const char* pattern, enum pat_mode mode);
void pat_cleanup(struct pat**);

gboolean pat_check(struct pat*, const char* test);

