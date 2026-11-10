#ifndef __FONT_H__
#define __FONT_H__

#include <SDL/SDL.h>

enum { FONT_EMBEDDED, FONT_CONSOLE, FONT_UNICODE };

void draw_char(SDL_Surface* surface, unsigned char symbol, int x, int y, unsigned short color);
void draw_string(SDL_Surface* surface, const char* text, int x, int y, unsigned short color);
void draw_char_ex(SDL_Surface* surface, unsigned char symbol, int x, int y, unsigned short color, int font);
void draw_string_ex(SDL_Surface* surface, const char* text, int x, int y, unsigned short color, int font);
void draw_text_ex(SDL_Surface* surface, const char* text, size_t length, int x, int y, unsigned short color, int font);
int load_unicode_font(const char* path);
int unicode_width(unsigned long codepoint);
int font_text_width(const char* text, int font);
int font_char_width(int font);
int font_line_height(int font);
void set_display_font(int font);
int get_display_font(void);
void toggle_display_font(void);

#endif
