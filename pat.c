#include "pat.h"

struct pat {
    gboolean plain;
}

struct pcre_pat {
    struct pat parent; 
    pcre* pat;
    pcre_extra* study;
};

struct plain_pat {
    struct pat parent;
    const char* substring;
};

struct pat* pat_setup(const char* pattern, gboolean plain) {
    if(plain==TRUE) {
        struct plain_pat* self = G_NEW(struct plain_pat);
        self->parent.plain = TRUE;
        self->substring = pattern; // assuming this is a string literal
        return self
    } else {
        struct pcre_pat* self = G_NEW(struct pcre_pat);
        self->parent.plain = FALSE;
        self->pat = pcre_compile(pattern,0,
                &err,&erroffset,NULL);
        if(!self->pat) {
            fprintf(stderr,"PCRE COMPILE ERROR %s\n",err);
            return FALSE
        }

        self->study = pcre_study(self->pat,0,&err);
        if(err) {
            fprintf(stderr,"Eh, study failed. %s\n",err);
        }
        return self
    }
}

  return TRUE;
}

void pat_cleanup(struct pat** self) {
    struct pat* doomed = *self;
    *self = NULL;

    if(doomed->plain == TRUE) {
        // no dynamically allocated members just a string literal
    } else {
        struct pcre_pat* pdoom = doomed;
        if(pdoom->pat) 
            pcre_free(pdoom->pat);
        if(pdoom->study)
            pcre_free(pdoom->study);
    }

    free(doomed);
}

gboolean pat_check(struct pat* self, const char* test) {
    int rc = pcre_exec(self->pat,                   /* the compiled pattern */
            self->study,             /* no extra data - we didn't study the pattern */
		     *test,              /* the subject string */
		     strlen(*test),       /* the length of the subject */
		     0,                    /* start at offset 0 in the subject */
		     0,                    /* default options */
		     NULL,              /* output vector for substring information */
		     0);           /* number of elements in the output vector */

  if(rc >= 0) {
      return TRUE;
  }
  return FALSE;
}

