#include "pat.h"

#include <glib.h>
#include <stdio.h>

struct pat {
    gboolean plain;
};

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
    const char* err = NULL;
    int erroffset = 0;

    if(plain==TRUE) {
        struct plain_pat* self = g_new(struct plain_pat,1);
        self->parent.plain = TRUE;
        self->substring = pattern; // assuming this is a string literal
        return (struct pat*)self;
    } else {
        struct pcre_pat* self = g_new(struct pcre_pat,1);
        self->parent.plain = FALSE;
        self->pat = pcre_compile(pattern,0,
                &err,&erroffset,NULL);
        if(!self->pat) {
            fprintf(stderr,"PCRE COMPILE ERROR %s\n",err);
            return NULL;
        }

        self->study = pcre_study(self->pat,0,&err);
        if(err) {
            fprintf(stderr,"Eh, study failed. %s\n",err);
        }
        return (struct pat*)self;
    }
}

void pat_cleanup(struct pat** self) {
    struct pat* doomed = *self;
    *self = NULL;

    if(doomed->plain == TRUE) {
        // no dynamically allocated members just a string literal
    } else {
        struct pcre_pat* pdoom = (struct pcre_pat*) doomed;
        if(pdoom->pat) 
            pcre_free(pdoom->pat);
        if(pdoom->study)
            pcre_free(pdoom->study);
    }

    free(doomed);
}

gboolean pat_check(struct pat* parent, const char* test) {
    if(parent->plain==TRUE) {
        struct plain_pat* self = (struct plain_pat*) parent;
        return g_strstr_len(test,strlen(test),self->substring) == NULL ? FALSE : TRUE;
    }
    struct pcre_pat* self = (struct pcre_pat*) parent;
    int rc = pcre_exec(self->pat,                   /* the compiled pattern */
            self->study,             /* no extra data - we didn't study the pattern */
		     *test,              /* the subject string */
		     g_strlen(*test),       /* the length of the subject */
		     0,                    /* start at offset 0 in the subject */
		     0,                    /* default options */
		     NULL,              /* output vector for substring information */
		     0);           /* number of elements in the output vector */

  if(rc >= 0) {
      return TRUE;
  }
  return FALSE;
}

