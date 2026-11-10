#include "ime.h"
#include "font.h"
#include "keyboard.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char output[65536];
static size_t output_len;

static void commit(const char *text, size_t length)
{
	assert(output_len + length < sizeof(output));
	memcpy(output + output_len, text, length);
	output_len += length;
	output[output_len] = 0;
}

static int event(int type, SDLKey key, unsigned int unicode, SDLMod mod)
{
	SDL_Event e = {0};
	e.type = e.key.type = type;
	e.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
	e.key.keysym.sym = key;
	e.key.keysym.unicode = unicode;
	e.key.keysym.mod = mod;
	if (handle_keyboard_event(&e)) return 1;
	return ime_handle_key(&e.key, commit);
}

static int press(SDLKey key, unsigned int u, SDLMod mod)
{
	int consumed = event(SDL_KEYDOWN, key, u, mod);
	event(SDL_KEYUP, key, 0, mod);
	return consumed;
}

static void type(const char *text)
{
	while (*text) {
		unsigned char c = *text++;
		assert(press((SDLKey)c, c, KMOD_NONE));
	}
}

static void tap_shift(void)
{
	event(SDL_KEYDOWN, SDLK_LSHIFT, 0, KMOD_LSHIFT);
	event(SDL_KEYUP, SDLK_LSHIFT, 0, KMOD_NONE);
}

static void reset(void)
{
	ime_disable();
	tap_shift();
	assert(ime_enabled());
	output_len = 0;
	output[0] = 0;
}

int main(int argc, char **argv)
{
	assert(argc >= 2);
	setvbuf(stdout, NULL, _IONBF, 0);
	assert(SDL_Init(0) == 0);
	char user[] = "/tmp/st-ime-user-XXXXXX";
	int fd = mkstemp(user);
	assert(fd >= 0); close(fd); unlink(user);
	assert(ime_init(argv[1], user));
	init_keyboard();
	assert(press(SDLK_F1, 0, KMOD_NONE));
	assert(!ime_enabled());
	puts("IME: Shift controls and decoder initialization");

	/* Physical Shift chords remain modifiers, including repeat and dual Shift. */
	tap_shift(); assert(ime_enabled());
	event(SDL_KEYDOWN, SDLK_LSHIFT, 0, KMOD_LSHIFT);
	event(SDL_KEYDOWN, SDLK_LSHIFT, 0, KMOD_LSHIFT);
	assert(!press(SDLK_a, 'A', KMOD_LSHIFT));
	event(SDL_KEYUP, SDLK_LSHIFT, 0, KMOD_NONE);
	assert(ime_enabled());
	event(SDL_KEYDOWN, SDLK_LSHIFT, 0, KMOD_LSHIFT | KMOD_LCTRL);
	event(SDL_KEYUP, SDLK_LSHIFT, 0, KMOD_LCTRL);
	assert(ime_enabled());
	event(SDL_KEYDOWN, SDLK_LSHIFT, 0, KMOD_LSHIFT);
	event(SDL_KEYDOWN, SDLK_RSHIFT, 0, KMOD_SHIFT);
	event(SDL_KEYUP, SDLK_LSHIFT, 0, KMOD_RSHIFT);
	assert(ime_enabled());
	event(SDL_KEYUP, SDLK_RSHIFT, 0, KMOD_NONE);
	assert(!ime_enabled());

	reset();
	type("nihao");
	puts("IME: phrase, punctuation and prefix selection");
	assert(output_len == 0 && !strcmp(ime_preedit(), "nihao"));
	assert(!strcmp(ime_candidate(0), "你好"));
	assert(press(SDLK_SPACE, ' ', KMOD_NONE));
	assert(!strcmp(output, "你好") && !ime_composing());
	assert(press(SDLK_PERIOD, '.', KMOD_NONE));
	assert(!strcmp(output, "你好。"));

	reset(); type("zhongwenshurufa");
	assert(!strcmp(ime_candidate(0), "中文输入法"));
	press(SDLK_SPACE, ' ', KMOD_NONE);
	assert(!strcmp(output, "中文输入法"));
	reset(); type("xi'an");
	assert(!strcmp(ime_candidate(0), "西安"));
	press(SDLK_1, '1', KMOD_NONE);
	assert(!strcmp(output, "西安"));

	/* Choosing a shorter word fixes its prefix and keeps the remaining syllable. */
	reset(); type("nihao");
	int shorter = -1;
	for (int i = 1; i < ime_candidate_count(); i++)
		if (!strcmp(ime_candidate(i), "你")) shorter = i;
	assert(shorter >= 0);
	press((SDLKey)(SDLK_1 + shorter), '1' + shorter, KMOD_NONE);
	assert(ime_composing() && output_len == 0);
	assert(!strncmp(ime_preedit(), "你", strlen("你")));
	press(SDLK_SPACE, ' ', KMOD_NONE);
	assert(!strcmp(output, "你好"));

	reset(); type("shi");
	char first[160], selected[160];
	snprintf(first, sizeof(first), "%s", ime_candidate(0));
	press(SDLK_PAGEDOWN, 0, KMOD_NONE);
	assert(ime_page() == 1 && strcmp(first, ime_candidate(0)));
	snprintf(selected, sizeof(selected), "%s", ime_candidate(1));
	press(SDLK_2, '2', KMOD_NONE);
	assert(!strcmp(output, selected));

	reset(); type("nihaoo"); press(SDLK_BACKSPACE, 8, KMOD_NONE);
	assert(!strcmp(ime_preedit(), "nihao"));
	press(SDLK_SPACE, ' ', KMOD_NONE); assert(!strcmp(output, "你好"));
	reset(); type("niho"); press(SDLK_LEFT, 0, KMOD_NONE); type("a");
	assert(!strcmp(ime_preedit(), "nihao"));
	press(SDLK_END, 0, KMOD_NONE); press(SDLK_SPACE, ' ', KMOD_NONE);
	assert(!strcmp(output, "你好"));

	reset(); type("nihao"); press(SDLK_RETURN, 13, KMOD_NONE);
	assert(!strcmp(output, "nihao") && !ime_composing());
	assert(!press(SDLK_RETURN, 13, KMOD_NONE));
	reset(); type("nihao");
	assert(press(SDLK_ESCAPE, 27, KMOD_NONE));
	assert(output_len == 0 && !ime_composing() && ime_enabled());
	assert(!press(SDLK_ESCAPE, 27, KMOD_NONE) && !ime_enabled());
	reset(); type("nihao"); tap_shift();
	assert(!ime_enabled() && !ime_composing() && output_len == 0);
	assert(press(SDLK_SPACE, 0, KMOD_LCTRL) && ime_enabled());
	type("nihao");
	assert(!press(SDLK_c, 3, KMOD_LCTRL) && !ime_enabled() && output_len == 0);

	reset(); type("lv"); assert(ime_candidate_count() > 0);
	press(SDLK_SPACE, ' ', KMOD_NONE); assert(output_len > 0);
	reset(); type("nihao");
	assert(!press(SDLK_a, 'A', KMOD_LSHIFT));
	assert(!strcmp(output, "你好"));
	reset(); press(SDLK_PERIOD, 0, KMOD_LCTRL);
	assert(!press(SDLK_PERIOD, '.', KMOD_NONE));
	reset();
	for (int i = 0; i < 100; i++) press(SDLK_n, 'n', KMOD_NONE);
	puts("IME: bounded repeated initials");
	assert(strlen(ime_preedit()) <= 39 && output_len == 0);
	for (int i = 0; i < 45; i++) press(SDLK_BACKSPACE, 8, KMOD_NONE);
	assert(!ime_composing());
	reset(); type("ni''hao");
	press(SDLK_ESCAPE, 27, KMOD_NONE);
	assert(!ime_composing() && !output_len);

	SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE, 480, 272, 16, 0xf800, 0x7e0, 0x1f, 0);
	puts("IME: candidate window rendering");
	assert(s);
	if (argc > 2 && access(argv[2], R_OK) == 0) assert(load_unicode_font(argv[2]));
	reset(); type("nihao");
	SDL_FillRect(s, NULL, SDL_MapRGB(s->format, 24, 28, 34));
	draw_string_ex(s, "D88 terminal / Vim\n\nShift 切换拼音输入\nnihao -> 你好\nSpace 选词  1-5 数字选词", 8, 8,
		SDL_MapRGB(s->format, 225, 230, 235), FONT_UNICODE);
	ime_draw(s, 8, 130, 16);
	assert(SDL_SaveBMP(s, ".build/pinyin-preview.bmp") == 0);
	for (int x = 0; x <= 480; x += 120)
		for (int y = 0; y <= 272; y += 68) ime_draw(s, x, y, 16);
	/* F1 toggles help; dismissing it keeps the virtual keyboard hidden. */
	ime_disable();
	SDL_FillRect(s, NULL, 0);
	press(SDLK_F1, 0, KMOD_NONE);
	draw_keyboard(s);
	assert(((unsigned short *)s->pixels)[0] != 0);
	assert(((unsigned short *)s->pixels)[80 * (s->pitch >> 1) + 100] != 0);
	press(SDLK_F1, 0, KMOD_NONE);
	SDL_FillRect(s, NULL, 0);
	draw_keyboard(s);
	assert(((unsigned short *)s->pixels)[0] == 0);
	assert(((unsigned short *)s->pixels)[80 * (s->pitch >> 1) + 100] == 0);
	SDL_FreeSurface(s);
	ime_shutdown();
	puts("IME: saved user dictionary; reopening decoder");
	assert(access(user, R_OK) == 0); /* User dictionary is persisted. */
	assert(ime_init(argv[1], user));
	reset(); type("nihao"); press(SDLK_SPACE, ' ', KMOD_NONE);
	assert(!strcmp(output, "你好"));
	ime_shutdown();
	unlink(user);
	assert(!ime_init("/nonexistent/st-ime-dictionary", user));
	ime_toggle(); assert(!ime_enabled());
	ime_shutdown();
	SDL_Quit();
	puts("Shift, full Pinyin, phrases, prefix choice, paging, editing, Vim Esc, persistence and candidate UI tests passed.");
	return 0;
}
