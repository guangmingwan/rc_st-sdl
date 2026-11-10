#ifndef ST_PINYIN_ENGINE_H
#define ST_PINYIN_ENGINE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
int pinyin_open(const char *dictionary, const char *user_dictionary);
void pinyin_close(void);
void pinyin_reset(void);
size_t pinyin_search(const char *text, size_t length);
size_t pinyin_choose(size_t index);
size_t pinyin_cancel_choice(void);
size_t pinyin_fixed(void);
size_t pinyin_fixed_bytes(void);
size_t pinyin_syllables(void);
size_t pinyin_decoded(void);
int pinyin_candidate(size_t index, char *utf8, size_t capacity);
void pinyin_preedit(const char *input, char *utf8, size_t capacity);
#ifdef __cplusplus
}
#endif

#endif
