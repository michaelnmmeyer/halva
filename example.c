#include <string.h>
#include "halva.h"

static const char *const words[] = {
   "greenish",
   "greenness",
   "greens",
   "greet",
   "greeting",
   "greets",
   "gregarious",
   "gregariously",
   NULL,
};

static const char *lexicon_path = "lexicon.dat";

/* Error handling omitted for brevity! */
int main(void)
{
   /* Create an lexicon encoding words in the above array. */
   struct halva_enc enc = HV_ENC_INIT;
   for (size_t i = 0; words[i]; i++)
      hv_enc_add(&enc, words[i], strlen(words[i]));
   FILE *fp = fopen(lexicon_path, "wb");
   hv_enc_dump_file(&enc, fp);
   hv_enc_fini(&enc);

   /* Load the automaton we just created. */
   fp = freopen(lexicon_path, "rb", fp);
   struct halva *lexicon;
   hv_load_file(&lexicon, fp);
   fclose(fp);

   /* Print all words >= "greet". */
   struct halva_iter itor;
   hv_iter_inits(&itor, lexicon, "greet", sizeof "greet" - 1);
   const char *word;
   while ((word = hv_iter_next(&itor, NULL)))
      puts(word);

   /* Cleanup. */
   hv_free(lexicon);
   remove(lexicon_path);
}
