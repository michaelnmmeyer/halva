#include <string.h>
#include <stdio.h>
#include "cmd.h"
#include "../halva.h"

static const char *read_line(size_t *len_p, size_t *line_no_p)
{
   static char line[HV_MAX_WORD_LEN + 2];    /* word, newline, zero */
   static size_t line_no;

   while (fgets(line, sizeof line, stdin)) {
      size_t len = strlen(line);
      line_no++;
      if (len && line[len - 1] == '\n')
         line[--len] = '\0';
      if (!len)
         continue;
      if (len > HV_MAX_WORD_LEN)
         die("word '%s' too long at line %zu (length limit is %d)", line, line_no, HV_MAX_WORD_LEN);
      *len_p = len;
      *line_no_p = line_no;
      return line;
   }
   if (ferror(stdin))
      die("IO error:");
   return NULL;
}

static void create(int argc, char **argv)
{
   parse_options(NULL, NULL, &argc, &argv);
   if (argc != 1)
      die("wrong number of arguments");

   struct halva_enc enc = HV_ENC_INIT;
   const char *word;
   size_t len, line_no;
   while ((word = read_line(&len, &line_no))) {
      int ret = hv_enc_add(&enc, word, len);
      if (ret)
         die("cannot add word '%s' at line %zu: %s", word, line_no, hv_strerror(ret));
   }

   const char *path = *argv;
   FILE *fp = fopen(path, "wb");
   if (!fp)
      die("cannot open '%s' for writing:", path);
   int ret = hv_enc_dump_file(&enc, fp);
   if (ret)
      die("cannot dump lexicon: %s", hv_strerror(ret));
   if (fclose(fp))
      die("IO error:");
   
   hv_enc_fini(&enc);
}

static void dump(int argc, char **argv)
{
   parse_options(NULL, NULL, &argc, &argv);
   if (argc != 1)
      die("wrong number of arguments");

   const char *path = *argv;
   FILE *fp = fopen(path, "rb");
   if (!fp)
      die("cannot open '%s':", path);
   
   struct halva *hv;
   int ret = hv_load_file(&hv, fp);
   fclose(fp);
   if (ret)
      die("cannot load lexicon: %s", hv_strerror(ret));

   struct halva_iter itor;
   hv_iter_init(&itor, hv);
   const char *word;
   while ((word = hv_iter_next(&itor, NULL)))
      puts(word);

   if (ferror(stdout))
      die("cannot dump lexicon:");

   hv_free(hv);
}

int main(int argc, char **argv)
{
   struct command cmds[] = {
      {"create", create},
      {"dump", dump},
      {0}
   };
   const char *help =
      #include "halva.ih"
   ;
   
   parse_command(cmds, help, argc, argv);
}
