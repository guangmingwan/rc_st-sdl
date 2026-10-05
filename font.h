#ifndef __FONT_H__
#define __FONT_H__

#include <SDL/SDL.h>

enum { FONT_EMBEDDED, FONT_CONSOLE };

void draw_char(SDL_Surface* surface, unsigned char symbol, int x, int y, unsigned short color);
void draw_string(SDL_Surface* surface, const char* text, int x, int y, unsigned short color);
void draw_char_ex(SDL_Surface* surface, unsigned char symbol, int x, int y, unsigned short color, int font);
void draw_string_ex(SDL_Surface* surface, const char* text, int x, int y, unsigned short color, int font);
int font_char_width(int font);
int font_line_height(int font);
void set_display_font(int font);
int get_display_font(void);
void toggle_display_font(void);

#endif
