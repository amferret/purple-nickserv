struct pat {
    pcre* pat;
    pcre_extra* study;
};



struct pats {
    pat ask_for_register;
    pat was_identified;
    pat use_recover;
    pat was_ghosted;
    pat was_recovered;
} g_pats = {};
