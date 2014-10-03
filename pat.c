#include "pat.h"

gboolean pat_setup(struct pat* self, const char* pattern) {
  g_pats.ask_for_register.pat = pcre_compile(pattern,0,
				  &err,&erroffset,NULL);
  if(!g_pats.ask_for_register.pat) {
    fprintf(stderr,"PCRE COMPILE ERROR %s\n",err);
    return FALSE
  }

  g_pats.ask_for_register.study = pcre_study(g_pats.ask_for_register.pat,0,&err);
  if(err) {
    fprintf(stderr,"Eh, study failed. %s\n",err);
    return FALSE;
  }

  return TRUE;
}
void pat_cleanup(struct pat* self) {
}
gboolean pat_check(struct pat* self, const char* test) {
}

