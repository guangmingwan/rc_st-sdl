#ifndef ST_IME_H
#define ST_IME_H

#include <SDL/SDL.h>
#include <stddef.h>

typedef void (*ImeCommit)(const char *, size_t);
int ime_init(const char *dictionary, const char *user_dictionary);
void ime_shutdown(void);
void ime_toggle(void);
void ime_disable(void);
int ime_enabled(void);
int ime_available(void);
int ime_composing(void);
const char *ime_preedit(void);
const char *ime_candidate(int slot);
int ime_candidate_count(void);
int ime_page(void);
int ime_handle_key(const SDL_KeyboardEvent *event, ImeCommit commit);
void ime_draw(SDL_Surface *surface, int cursor_x, int cursor_y, int cell_height);

#endif
