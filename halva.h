#ifndef HALVA_H
#define HALVA_H

#define HV_VERSION "0.2"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Maximum length of a word, in bytes, not including the terminating nul byte,
 * if any. Cannot be increased.
 */
#define HV_MAX_WORD_LEN 255

/* Size of a group of words in a lexicon. Must be a power of two. This can be
 * tweaked to get better compression (with a large blocking factor) or to
 * increase processing speed (with a smaller blocking factor).
 */
#define HV_BLOCKING_FACTOR 16

/* Error codes.
 * All functions below that return an int return one of these.
 */
enum {
   HV_OK,         /* No error. */
   HV_EWORD,      /* Attempt to add the empty string or a too long word. */
   HV_EORDER,     /* Word added out of order. */
   HV_EMAGIC,     /* Magic identifier mismatch. */
   HV_EVERSION,   /* Version mismatch. */
   HV_EFREEZED,   /* Attempt to add a word to a freezed lexicon. */
   HV_E2BIG,      /* Lexicon has grown too large. */
   HV_EIO,        /* IO error. */
   HV_ENOMEM,     /* Out of memory. */
};

/* Returns a string describing an error code. */
const char *hv_strerror(int err);


/*******************************************************************************
 * Encoder
 ******************************************************************************/

struct halva_enc {
   uint32_t num_words;                 /* Number of words added so far. */
   uint32_t *header;                   /* Bucket pointers. */
   size_t header_size;
   size_t header_alloc;
   uint8_t *body;                      /* Front-encoded terms. */
   size_t body_size;
   size_t body_alloc;
   uint8_t prev[HV_MAX_WORD_LEN + 1];  /* Previous word added. */
   size_t prev_len;
   int finished;                       /* Whether the encoder is freezed. */
};

/* Initializer. */
#define HV_ENC_INIT {.num_words = 0}

/* Destructor. */
void hv_enc_fini(struct halva_enc *);

/* Adds a new word.
 * Words must be added in lexicographical order (memcmp() order), must be
 * unique, and their length must be > 0 and <= HV_MAX_WORD_LEN.
 */
int hv_enc_add(struct halva_enc *, const void *word, size_t len);

/* Dumps a lexicon to a file.
 * The provided callback will be called several times for writing the lexicon
 * to some file or memory location. It must return zero on success, non-zero on
 * error. If it returns non-zero, this function will return HV_EIO.
 *
 * After this function is called, the encoder will be freezed. Then, no new
 * words should be added until hv_enc_clear() is called.
 */
int hv_enc_dump(struct halva_enc *,
                int (*write)(void *arg, const void *data, size_t size),
                void *arg);

/* Dumps an encoded lexicon to a file.
 * The provided file must be opened in binary mode, for writing. The underlying
 * file descriptor is not synced with the device, but the file handle is
 * flushed.
 */
int hv_enc_dump_file(struct halva_enc *, FILE *fp);

/* Clears an encoder.
 * After this is called, the encoder object can be used again to encode a new
 * set of words.
 */
void hv_enc_clear(struct halva_enc *);


/*******************************************************************************
 * Decoder
 ******************************************************************************/

struct halva;

/* Loads a lexicon.
 * The provided callback will be called several times for reading the lexicon.
 * It should return zero on success, non-zero on failure. A short read must be
 * considered as an error.
 * On success, makes the provided struct pointer point to the allocated lexicon.
 * On failure, makes it point to NULL.
 */
int hv_load(struct halva **, int (*read)(void *arg, void *buf, size_t size),
            void *arg);

/* Loads a lexicon from a file.
 * The provided file must be opened in binary mode, for reading.
 */
int hv_load_file(struct halva **, FILE *);

/* Destructor. */
void hv_free(struct halva *);

/* Returns the number of words in a lexicon. */
size_t hv_size(const struct halva *);

/* Returns the ordinal associated to a word.
 * If the word doesn't exist in the lexicon, the return value is 0, otherwise a
 * positive integer.
 */
uint32_t hv_locate(const struct halva *, const void *word, size_t len);

/* Retrieves a word given its corresponding ordinal.
 * If the provided position is valid, fills "buf" with the corresponding word,
 * and return its length. Otherwise, add a nul character at the beginning of
 * "buf", and return 0. "buf" must be at least as large as HV_MAX_WORD_LEN + 1.
 */
size_t hv_extract(const struct halva *, uint32_t pos, void *buf);


/*******************************************************************************
 * Iterator
 ******************************************************************************/

struct halva_iter {
   const struct halva *hv;          /* Associated lexicon. */
   uint32_t pos;                    /* Position of the current word. */
   const uint8_t *p;                /* Memory region being traversed. */
   char word[HV_MAX_WORD_LEN + 1];  /* Current word. */
};

/* Initializes an iterator for iterating over all words in a lexicon,
 * in ascending order.
 * Returns 1 if there is something to iterate on, 0 otherwise.
 */
uint32_t hv_iter_init(struct halva_iter *, const struct halva *);

/* Initializes an iterator for iterating over all words of a lexicon that are
 * >= some given word, in ascending order.
 * Returns the position of the word at which iteration will start, or 0 if there
 * is nothing to iterate on.
 */
uint32_t hv_iter_inits(struct halva_iter *, const struct halva *,
                       const void *word, size_t len);

/* Initializes an iterator for iterating over all words of a lexicon which
 * ordinal is >= some given position.
 * Returns the position of the word at which iteration will start, or 0 if there
 * is nothing to iterate on.
 */
uint32_t hv_iter_initn(struct halva_iter *, const struct halva *,
                       uint32_t pos);

/* Fetches the next word from an initialized iterator.
 * If "len" is not NULL, it will be assigned the length of the current word.
 * On end of iteration, NULL is returned, and "len", if not NULL, is set to 0.
 */
const char *hv_iter_next(struct halva_iter *, size_t *len);

#endif
