#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <arpa/inet.h>  /* htonl(), ntohl(). */
#include "halva.h"

/* Largest number that can be represented in a nibble. */
#define HV_NIBBLE_SIZE 15

static const uint32_t hv_magic = 1751938657;
static const uint32_t hv_version = 1;

#define HV_DIV_ROUNDUP(a, b) (((a) + (b) - 1) / (b))

static int lmemcmp(const void *restrict str1, size_t len1,
                   const void *restrict str2, size_t len2)
{
   int cmp = memcmp(str1, str2, len1 < len2 ? len1 : len2);
   if (cmp)
      return cmp;
   return len1 < len2 ? -1 : len1 > len2;
}

const char *hv_strerror(int err)
{
   static const char *const tbl[] = {
      [HV_OK] = "no error",
      [HV_EWORD] = "attempt to add the empty string or a too long word",
      [HV_EORDER] = "word added out of order",
      [HV_EMAGIC] = "magic identifier mismatch",
      [HV_EVERSION] = "version mismatch",
      [HV_EFREEZED] = "attempt to add a word to a freezed lexicon",
      [HV_E2BIG] = "lexicon has grown too large",
      [HV_EIO] = "IO error",
      [HV_ENOMEM] = "out of memory",
   };

   if (err >= 0 && (size_t)err < sizeof tbl / sizeof *tbl)
      return tbl[err];
   return "unknown error";
}


/*******************************************************************************
 * Encoder
 ******************************************************************************/

#define HV_MAX_SIZE (3 * 1024 * 1024)  /* Be conservative. */

#define HV_DEF_GROW(NAME)                                                      \
static int hv_enc_grow_##NAME(struct halva_enc *enc, size_t incr)              \
{                                                                              \
   size_t need = enc->NAME##_size + incr;                                      \
   if (need < enc->NAME##_size)                                                \
      return HV_ENOMEM;                                                        \
   if (need <= enc->NAME##_alloc)                                              \
      return HV_OK;                                                            \
                                                                               \
   size_t new_alloc = enc->NAME##_alloc + (enc->NAME##_alloc >> 1) + 16;       \
   if (new_alloc < need)                                                       \
      new_alloc = need;                                                        \
   if (new_alloc > SIZE_MAX / sizeof *enc->NAME)                               \
      return HV_ENOMEM;                                                        \
                                                                               \
   void *tmp = realloc(enc->NAME, new_alloc * sizeof *enc->NAME);              \
   if (!tmp)                                                                   \
      return HV_ENOMEM;                                                        \
                                                                               \
   enc->NAME = tmp;                                                            \
   enc->NAME##_alloc = new_alloc;                                              \
   return HV_OK;                                                               \
}

HV_DEF_GROW(header)
HV_DEF_GROW(body)

int hv_enc_add(struct halva_enc *enc, const void *word, size_t len)
{
   if (enc->finished)
      return HV_EFREEZED;

   if (enc->header_size * sizeof *enc->header + enc->body_size > HV_MAX_SIZE)
      return HV_E2BIG;
   
   if (len == 0 || len > HV_MAX_WORD_LEN)
      return HV_EWORD;

   if (lmemcmp(enc->prev, enc->prev_len, word, len) >= 0)
      return HV_EORDER;
   
   if (!(enc->num_words & (HV_BLOCKING_FACTOR - 1))) {
      if (hv_enc_grow_header(enc, 1) || hv_enc_grow_body(enc, 1 + len))
         return HV_ENOMEM;
      uint32_t pos = enc->body_size;
      enc->header[enc->header_size++] = pos;
      enc->body[enc->body_size++] = len;
      memcpy(&enc->body[enc->body_size], word, len);
      enc->body_size += len;
   } else {
      const uint8_t *wordp = word;
      const uint8_t *prevp = enc->prev;
      size_t pref_len = 0;
      size_t min_len = len < enc->prev_len ? len : enc->prev_len;
      if (min_len > HV_NIBBLE_SIZE)
         min_len = HV_NIBBLE_SIZE;
      while (pref_len < min_len && *wordp == *prevp) {
         prevp++;
         wordp++;
         pref_len++;
      }
      size_t suff_len = len - pref_len;
      if (hv_enc_grow_body(enc, 2 + suff_len))
         return HV_ENOMEM;
      if (suff_len > HV_NIBBLE_SIZE) {
         enc->body[enc->body_size++] = pref_len;
         enc->body[enc->body_size++] = suff_len;
      } else {
         enc->body[enc->body_size++] = pref_len | (suff_len << 4);
      }
      memcpy(&enc->body[enc->body_size], wordp, suff_len);
      enc->body_size += suff_len;
   }
   memcpy(enc->prev, word, len);
   enc->prev_len = len;
   enc->num_words++;
   
   return HV_OK;
}

int hv_enc_dump(struct halva_enc *enc,
                int (*write)(void *arg, const void *data, size_t size),
                void *arg)
{
   if (!enc->finished) {
      for (uint32_t i = 0; i < enc->header_size; i++)
         enc->header[i] = htonl(enc->header[i]);
      enc->finished = true;
   }

   uint32_t header[] = {
      htonl(hv_magic),
      htonl(hv_version),
      htonl(enc->num_words),
      htonl(enc->body_size),
   };
   if (write(arg, header, sizeof header)
      || write(arg, enc->header, enc->header_size * sizeof *enc->header)
      || write(arg, enc->body, enc->body_size))
      return HV_EIO;
   return HV_OK;
}

static int hv_write(void *fp, const void *data, size_t size)
{
   if (fwrite(data, 1, size, fp) == size)
      return 0;
   return -1;
}

int hv_enc_dump_file(struct halva_enc *enc, FILE *fp)
{
   int ret = hv_enc_dump(enc, hv_write, fp);
   if (ret)
      return ret;

   return fflush(fp) ? HV_EIO : HV_OK;
}

void hv_enc_clear(struct halva_enc *enc)
{
   enc->num_words = enc->header_size = enc->body_size = 0;
   enc->prev_len = 0;
   enc->finished = false;
}

void hv_enc_fini(struct halva_enc *enc)
{
   free(enc->header);
   free(enc->body);
}


/*******************************************************************************
 * Decoder
 ******************************************************************************/

struct halva {
   uint32_t num_words;     /* Number of words. */
   uint32_t num_bkts;      /* Number of buckets. */
   const uint8_t *body;    /* Body section. */
   uint32_t header[];      /* Bucket pointers. */
};

/* Number of words in a bucket. */
static uint32_t hv_limit(const struct halva *hv, uint32_t bkt)
{
   assert(bkt < hv->num_bkts);
   
   if (bkt + 1 == hv->num_bkts) {
      uint32_t high = hv->num_words & (HV_BLOCKING_FACTOR - 1);
      if (high)
         return high;
   }
   return HV_BLOCKING_FACTOR;
}

int hv_load(struct halva **hvp, int (*read)(void *arg, void *buf, size_t size),
            void *arg)
{
   *hvp = NULL;

   uint32_t header[4];
   if (read(arg, header, sizeof header))
      return HV_EIO;
   for (size_t i = 0; i < sizeof header / sizeof *header; i++)
      header[i] = ntohl(header[i]);

   if (header[0] != hv_magic)
      return HV_EMAGIC;
   if (header[1] != hv_version)
      return HV_EVERSION;
   
   uint32_t num_words = header[2];
   uint32_t body_size = header[3];
   uint32_t num_bkts = HV_DIV_ROUNDUP(num_words, HV_BLOCKING_FACTOR);

   size_t to_read = num_bkts * sizeof(uint32_t) + body_size;
   struct halva *hv = malloc(offsetof(struct halva, header) + to_read);
   if (!hv)
      return HV_ENOMEM;
   if (to_read && read(arg, hv->header, to_read)) {
      free(hv);
      return HV_EIO;
   }
   
   hv->num_words = num_words;
   hv->num_bkts = num_bkts;
   for (uint32_t i = 0; i < num_bkts; i++)
      hv->header[i] = ntohl(hv->header[i]);
   hv->body = (const uint8_t *)(&hv->header[hv->num_bkts]);
   
   *hvp = hv;
   return HV_OK;
}

static int hv_read(void *fp, void *buf, size_t size)
{
   if (fread(buf, 1, size, fp) == size)
      return 0;
   return -1;
}

int hv_load_file(struct halva **hv, FILE *fp)
{
   int ret = hv_load(hv, hv_read, fp);
   if (ret)
      return ret;
   
   return ferror(fp) ? HV_EIO : HV_OK;
}

size_t hv_size(const struct halva *hv)
{
   return hv->num_words;
}

static uint32_t hv_find_bkt(const struct halva *hv,
                            const uint8_t *term1, size_t len1)
{
   uint32_t low = 0, high = hv->num_bkts;
   
   while (low < high) {
      uint32_t mid = (low + high) >> 1;
      const uint8_t *term2 = hv->body + hv->header[mid];
      size_t len2 = *term2++;
      if (lmemcmp(term1, len1, term2, len2) < 0)
         high = mid;
      else
         low = mid + 1;
   }
   return low;
}

uint32_t hv_locate(const struct halva *hv, const void *term, size_t len1)
{
   const uint8_t *term1 = term;
   uint32_t bkt = hv_find_bkt(hv, term1, len1);
   if (!bkt)
      return 0;
   
   const uint8_t *term2 = hv->body + hv->header[--bkt];
   size_t len2 = *term2++;
   
   if (!lmemcmp(term1, len1, term2, len2))
      return bkt * HV_BLOCKING_FACTOR + 1;
   
   uint8_t term2b[HV_MAX_WORD_LEN + 1];
   memcpy(term2b, term2, len2);
   term2 += len2;
   
   uint32_t high = hv_limit(hv, bkt);
   for (uint32_t pos = 1; pos < high; pos++) {
      size_t pref_len = *term2 & HV_NIBBLE_SIZE;
      size_t suff_len = *term2++ >> 4;
      if (!suff_len)
         suff_len = *term2++;
      memcpy(&term2b[pref_len], term2, suff_len);
      
      int cmp = lmemcmp(term2b, pref_len + suff_len, term1, len1);
      if (cmp < 0)
         term2 += suff_len;
      else if (cmp == 0)
         return bkt * HV_BLOCKING_FACTOR + pos + 1;
      else
         break;
   }
   return 0;
}

size_t hv_extract(const struct halva *hv, uint32_t pos, void *buf)
{
   if (!pos || pos > hv->num_words) {
      *(uint8_t *)buf = '\0';
      return 0;
   }
   
   pos--;
   uint32_t bkt = pos / HV_BLOCKING_FACTOR;
   uint32_t rest = pos & (HV_BLOCKING_FACTOR - 1);
   
   const uint8_t *target = hv->body + hv->header[bkt];
   size_t pref_len = *target++;
   size_t suff_len = 0;
   
   memcpy(buf, target, pref_len);
   if (rest) {
      target += pref_len;
      do {
         pref_len = *target & HV_NIBBLE_SIZE;
         suff_len = *target++ >> 4;
         if (!suff_len)
            suff_len = *target++;
         memcpy((uint8_t *)buf + pref_len, target, suff_len);
         target += suff_len;
      } while (--rest);
   }
   
   ((uint8_t *)buf)[pref_len += suff_len] = '\0';
   return pref_len;
}

void hv_free(struct halva *hv)
{
   free(hv);
}


/*******************************************************************************
 * Iterator
 ******************************************************************************/

uint32_t hv_iter_init(struct halva_iter *it, const struct halva *hv)
{
   it->hv = hv;
   it->pos = 0;
   it->p = hv->body;
   return it->pos < hv->num_words ? 1 : 0;
}

uint32_t hv_iter_inits(struct halva_iter *it, const struct halva *hv,
                       const void *term, size_t len1)
{
   const uint8_t *term1 = term;
   
   uint32_t bkt = hv_find_bkt(hv, term1, len1);
   if (!bkt)
      return hv_iter_init(it, hv);
   
   it->hv = hv;
   
   const uint8_t *cur = hv->body + hv->header[--bkt];
   const uint8_t *term2 = cur;
   size_t pref_len, suff_len = *term2++;
   
   if (!lmemcmp(term1, len1, term2, suff_len)) {
      it->pos = bkt * HV_BLOCKING_FACTOR;
      it->p = cur;
      return it->pos + 1;
   }
   
   memcpy(it->word, term2, suff_len);
   term2 += suff_len;
   
   uint32_t high = hv_limit(hv, bkt);
   for (uint32_t pos = 1; pos < high; pos++) {
      cur = term2;
      pref_len = *term2 & HV_NIBBLE_SIZE;
      suff_len = *term2++ >> 4;
      if (!suff_len)
         suff_len = *term2++;
      memcpy(&it->word[pref_len], term2, suff_len);
      if (lmemcmp(term1, len1, it->word, pref_len + suff_len) > 0) {
         term2 += suff_len;
         continue;
      }
      it->pos = bkt * HV_BLOCKING_FACTOR + pos;
      it->p = cur;
      return it->pos + 1;
   }
   
   it->pos = (bkt + 1) * HV_BLOCKING_FACTOR;
   it->p = term2;
   if (it->pos > hv->num_words)
      return 0;
   return it->pos + 1;
}

uint32_t hv_iter_initn(struct halva_iter *it, const struct halva *hv,
                       uint32_t pos)
{
   it->hv = hv;
   
   if (pos == 0 || pos > hv->num_words) {
      it->pos = hv->num_words;
      it->p = NULL;
      return 0;
   }
   
   pos--;
   uint32_t bkt = pos / HV_BLOCKING_FACTOR;
   uint32_t rest = pos & (HV_BLOCKING_FACTOR - 1);
   
   if (!rest) {
      it->pos = pos;
      it->p = hv->body + hv->header[bkt];
   } else {
      const uint8_t *target = hv->body + hv->header[bkt];
      size_t pref_len = *target++;
      memcpy(it->word, target, pref_len);
      target += pref_len;
      while (--rest) {
         pref_len = *target & HV_NIBBLE_SIZE;
         size_t suff_len = *target++ >> 4;
         if (!suff_len)
            suff_len = *target++;
         memcpy(&it->word[pref_len], target, suff_len);
         target += suff_len;
      }
      it->pos = pos;
      it->p = target;
   }
   return it->pos + 1;
}

const char *hv_iter_next(struct halva_iter *it, size_t *len)
{
   if (it->pos >= it->hv->num_words) {
      if (len)
         *len = 0;
      return NULL;
   }
   
   if (!(it->pos & (HV_BLOCKING_FACTOR - 1))) {
      size_t pref_len = *it->p++;
      memcpy(it->word, it->p, pref_len);
      it->word[pref_len] = '\0';
      if (len)
         *len = pref_len;
      it->p += pref_len;
   } else {
      size_t pref_len = *it->p & HV_NIBBLE_SIZE;
      size_t suff_len = *it->p++ >> 4;
      if (!suff_len)
         suff_len = *it->p++;
      memcpy(&it->word[pref_len], it->p, suff_len);
      pref_len += suff_len;
      it->word[pref_len] = '\0';
      if (len)
         *len = pref_len;
      it->p += suff_len;
   }
   
   it->pos++;
   return it->word;
}
