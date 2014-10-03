#include "pat.h"

gboolean pat_setup(struct pat* self, const char* pattern) {
  self->pat = pcre_compile(pattern,0,
				  &err,&erroffset,NULL);
  if(!self->pat) {
    fprintf(stderr,"PCRE COMPILE ERROR %s\n",err);
    return FALSE
  }

  self->study = pcre_study(g_pats.ask_for_register.pat,0,&err);
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
}

