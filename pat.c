#include "pat.h"

gboolean pat_setup(struct pat* self, const char* pattern) {
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

  return TRUE;
}

void pat_cleanup(struct pat* self) {
  if(self->pat) 
    pcre_free(self->pat);
    self->pat = NULL;
  }
  if(self->study) {
    pcre_free(self->study);
    self->study = NULL;
  }
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

