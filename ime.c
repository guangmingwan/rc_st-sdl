#include "ime.h"
#include "font.h"
#include "pinyin_engine.h"
#include <stdio.h>
#include <string.h>

#define INPUT_MAX 39
#define PAGE_SIZE 5
#define TEXT_MAX 160

static struct {
	int available, enabled, punctuation, quote, single_quote;
	char input[INPUT_MAX + 1], preedit[TEXT_MAX], candidates[PAGE_SIZE][TEXT_MAX];
	size_t length, caret, total;
	int page, selected, visible, error;
} im;

static void refresh(void)
{
	size_t first = im.page * PAGE_SIZE;
	if (first >= im.total) im.page = 0;
	first = im.page * PAGE_SIZE;
	im.visible = 0;
	for (int i = 0; i < PAGE_SIZE; i++) {
		im.candidates[i][0] = 0;
		if (first + i < im.total && pinyin_candidate(first + i, im.candidates[i], TEXT_MAX))
			im.visible++;
	}
	if (im.selected >= im.visible) im.selected = 0;
	pinyin_preedit(im.input, im.preedit, sizeof(im.preedit));
}

static void clear_input(void)
{
	im.input[0] = im.preedit[0] = 0;
	im.length = im.caret = im.total = 0;
	im.page = im.selected = im.visible = 0;
	pinyin_reset();
}

int ime_init(const char *dictionary, const char *user_dictionary)
{
	ime_shutdown();
	memset(&im, 0, sizeof(im));
	im.punctuation = 1;
	im.available = pinyin_open(dictionary, user_dictionary);
	return im.available;
}

void ime_shutdown(void)
{
	pinyin_close();
	memset(&im, 0, sizeof(im));
}

void ime_toggle(void)
{
	if (!im.available) { im.error = 1; return; }
	/* Switching to English cancels preedit, avoiding unintended shell commands. */
	clear_input();
	im.enabled = !im.enabled;
	im.error = 0;
}

void ime_disable(void) { clear_input(); im.enabled = 0; im.error = 0; }
int ime_enabled(void) { return im.enabled; }
int ime_available(void) { return im.available; }
int ime_composing(void) { return im.length != 0; }
const char *ime_preedit(void) { return im.preedit; }
const char *ime_candidate(int slot) { return slot >= 0 && slot < im.visible ? im.candidates[slot] : ""; }
int ime_candidate_count(void) { return im.visible; }
int ime_page(void) { return im.page; }

static void search(void)
{
	im.total = pinyin_search(im.input, im.length);
	im.page = im.selected = 0;
	refresh();
}

static int choose(int slot, ImeCommit commit)
{
	if (slot < 0 || slot >= im.visible || pinyin_decoded() != im.length) return 0;
	im.total = pinyin_choose(im.page * PAGE_SIZE + slot);
	im.page = im.selected = 0;
	if (pinyin_fixed() == pinyin_syllables() && im.total) {
		char output[TEXT_MAX];
		if (pinyin_candidate(0, output, sizeof(output))) {
			commit(output, strlen(output));
			clear_input();
			return 1;
		}
	}
	refresh();
	return 0;
}

static void page(int direction)
{
	if (direction > 0 && (size_t)(im.page + 1) * PAGE_SIZE < im.total) im.page++;
	if (direction < 0 && im.page > 0) im.page--;
	im.selected = 0;
	refresh();
}

static const char *punctuation(unsigned int u)
{
	if (!im.punctuation) return NULL;
	switch (u) {
	case ',': return "，";
	case '.': return "。";
	case '?': return "？";
	case '!': return "！";
	case ':': return "：";
	case ';': return "；";
	case '(': return "（";
	case ')': return "）";
	case '"': im.quote = !im.quote; return im.quote ? "“" : "”";
	case '\'': im.single_quote = !im.single_quote; return im.single_quote ? "‘" : "’";
	default: return NULL;
	}
}

int ime_handle_key(const SDL_KeyboardEvent *event, ImeCommit commit)
{
	SDLKey key = event->keysym.sym;
	SDLMod mod = event->keysym.mod;
	unsigned int u = event->keysym.unicode;
	if (event->type != SDL_KEYDOWN) return 0;
	im.error = 0;
	if (key == SDLK_SPACE && (mod & KMOD_CTRL) && !(mod & (KMOD_ALT | KMOD_META))) {
		ime_toggle(); return 1;
	}
	if (!im.enabled) return 0;
	if (key == SDLK_PERIOD && (mod & KMOD_CTRL)) {
		im.punctuation = !im.punctuation; return 1;
	}
	if (mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) {
		clear_input();
		if ((mod & KMOD_CTRL) && (key == SDLK_LEFTBRACKET || key == SDLK_c)) im.enabled = 0;
		return 0;
	}
	if (key == SDLK_ESCAPE) {
		if (im.length) { clear_input(); return 1; }
		ime_disable();
		return 0; /* Vim receives Esc and returns to Normal mode. */
	}
	if (!u && key >= SDLK_a && key <= SDLK_z)
		u = key - SDLK_a + ((mod & (KMOD_SHIFT | KMOD_CAPS)) ? 'A' : 'a');
	if ((u >= 'a' && u <= 'z') || (u == '\'' && im.length)) {
		if (im.length == INPUT_MAX) return 1;
		if (im.caret < pinyin_fixed_bytes()) pinyin_reset();
		memmove(im.input + im.caret + 1, im.input + im.caret, im.length - im.caret + 1);
		im.input[im.caret++] = u;
		im.length++;
		search();
		return 1;
	}
	if (im.length) {
		switch (key) {
		case SDLK_BACKSPACE:
			if (!im.caret) return 1;
			if (im.caret <= pinyin_fixed_bytes()) {
				im.total = pinyin_cancel_choice();
				refresh();
				return 1;
			}
			memmove(im.input + im.caret - 1, im.input + im.caret, im.length - im.caret + 1);
			im.caret--; im.length--;
			search(); return 1;
		case SDLK_DELETE:
			if (im.caret < im.length) {
				if (im.caret < pinyin_fixed_bytes()) pinyin_reset();
				memmove(im.input + im.caret, im.input + im.caret + 1, im.length - im.caret);
				im.length--; search();
			}
			return 1;
		case SDLK_LEFT:
			if (im.caret) im.caret--;
			if (im.caret < pinyin_fixed_bytes()) { pinyin_reset(); search(); }
			return 1;
		case SDLK_RIGHT: if (im.caret < im.length) im.caret++; return 1;
		case SDLK_HOME: im.caret = 0; pinyin_reset(); search(); return 1;
		case SDLK_END: im.caret = im.length; return 1;
		case SDLK_UP:
			if (im.selected > 0) im.selected--; else page(-1);
			return 1;
		case SDLK_DOWN:
			if (im.selected + 1 < im.visible) im.selected++; else page(1);
			return 1;
		case SDLK_TAB:
			if (im.visible) im.selected = (im.selected + ((mod & KMOD_SHIFT) ? im.visible - 1 : 1)) % im.visible;
			return 1;
		case SDLK_PAGEUP: case SDLK_MINUS: page(-1); return 1;
		case SDLK_PAGEDOWN: case SDLK_EQUALS: page(1); return 1;
		case SDLK_SPACE: choose(im.selected, commit); return 1;
		case SDLK_RETURN: case SDLK_KP_ENTER:
			commit(im.input, im.length); clear_input(); return 1;
		default: break;
		}
		if (u >= '1' && u <= '5') { choose(u - '1', commit); return 1; }
		if (u >= '0' && u <= '9') return 1;
		if (u >= 0x20 && u < 0x7f) {
			/* Shifted English/punctuation first accepts the sentence candidate. */
			if (!choose(0, commit)) return 1;
		} else {
			clear_input();
			return 0;
		}
	}
	const char *full = punctuation(u);
	if (full) { commit(full, strlen(full)); return 1; }
	return 0;
}

static void box(SDL_Surface *s, int x, int y, int w, int h, unsigned short color)
{
	SDL_Rect r = {x, y, w, h};
	SDL_FillRect(s, &r, color);
}

static void fit(const char *text, char *out, size_t cap, int width)
{
	size_t length = strlen(text);
	if (length >= cap) length = cap - 1;
	memcpy(out, text, length); out[length] = 0;
	if (font_text_width(out, FONT_UNICODE) <= width) return;
	while (length) {
		do { length--; } while (length && (out[length] & 0xc0) == 0x80);
		out[length] = 0;
		if (font_text_width(out, FONT_UNICODE) + 8 <= width) break;
	}
	if (length + 4 <= cap) memcpy(out + length, "…", 4);
}

void ime_draw(SDL_Surface *s, int cursor_x, int cursor_y, int cell_height)
{
	if (!im.enabled && !im.error) return;
	if (s->w < 64 || s->h < 32) return;
	unsigned short white = SDL_MapRGB(s->format, 255, 255, 255);
	unsigned short black = SDL_MapRGB(s->format, 36, 40, 46);
	unsigned short blue = SDL_MapRGB(s->format, 53, 115, 209);
	unsigned short gray = SDL_MapRGB(s->format, 205, 210, 218);
	unsigned short muted = SDL_MapRGB(s->format, 105, 113, 125);
	char entries[PAGE_SIZE][TEXT_MAX], shown[TEXT_MAX], footer[100];
	int widths[PAGE_SIZE], row[PAGE_SIZE], col[PAGE_SIZE];
	int panel_w, panel_h, x, y, max_w = s->w - 8;
	if (!im.length || im.error) {
		const char *label = im.error ? "Pinyin dictionary unavailable" : "拼音   Shift 中/英";
		panel_w = 40 + font_text_width(label, FONT_UNICODE);
		if (panel_w > max_w) panel_w = max_w;
		panel_h = 28;
		x = cursor_x; if (x + panel_w > s->w - 4) x = s->w - panel_w - 4; if (x < 4) x = 4;
		y = cursor_y + cell_height + 2; if (y + panel_h > s->h - 4) y = cursor_y - panel_h - 2; if (y < 4) y = 4;
		box(s, x, y, panel_w, panel_h, gray);
		box(s, x + 1, y + 1, panel_w - 2, panel_h - 2, white);
		box(s, x + 4, y + 4, 24, 20, blue);
		draw_string_ex(s, "中", x + 8, y + 6, white, FONT_UNICODE);
		fit(label, shown, sizeof(shown), panel_w - 38);
		draw_string_ex(s, shown, x + 34, y + 6, black, FONT_UNICODE);
		return;
	}
	int sum = 0;
	for (int i = 0; i < im.visible; i++) {
		char candidate[TEXT_MAX];
		fit(im.candidates[i], candidate, sizeof(candidate), max_w / 2 - 40);
		snprintf(entries[i], sizeof(entries[i]), "%c %.150s", '1' + i, candidate);
		widths[i] = font_text_width(entries[i], FONT_UNICODE) + 16;
		sum += widths[i];
	}
	panel_w = sum + 12;
	int preedit_w = 44 + font_text_width(im.preedit, FONT_UNICODE);
	if (panel_w < preedit_w) panel_w = preedit_w;
	if (panel_w < 288) panel_w = 288;
	if (panel_w > max_w) panel_w = max_w;
	int cx = 6, cy = 30;
	for (int i = 0; i < im.visible; i++) {
		if (cx + widths[i] > panel_w - 6 && cx > 6) { cx = 6; cy += 24; }
		col[i] = cx; row[i] = cy;
		cx += widths[i];
	}
	panel_h = cy + 48;
	x = cursor_x; if (x + panel_w > s->w - 4) x = s->w - panel_w - 4; if (x < 4) x = 4;
	y = cursor_y + cell_height + 2; if (y + panel_h > s->h - 4) y = cursor_y - panel_h - 2; if (y < 4) y = 4;
	box(s, x + 2, y + 2, panel_w, panel_h, gray);
	box(s, x, y, panel_w, panel_h, gray);
	box(s, x + 1, y + 1, panel_w - 2, panel_h - 2, white);
	box(s, x + 4, y + 4, 24, 20, blue);
	draw_string_ex(s, "中", x + 8, y + 6, white, FONT_UNICODE);
	fit(im.preedit, shown, sizeof(shown), panel_w - 44);
	draw_string_ex(s, shown, x + 34, y + 6, black, FONT_UNICODE);
	int caret = (int)pinyin_fixed() * 16 + ((int)im.caret - (int)pinyin_fixed_bytes()) * 8;
	if (caret >= 0 && caret <= panel_w - 44) box(s, x + 34 + caret, y + 5, 1, 17, blue);
	box(s, x + 6, y + 27, panel_w - 12, 1, gray);
	if (!im.visible) draw_string_ex(s, "请检查拼音；Enter 输入原文", x + 12, y + 34, muted, FONT_UNICODE);
	for (int i = 0; i < im.visible; i++) {
		if (i == im.selected) box(s, x + col[i], y + row[i], widths[i] - 3, 22, blue);
		draw_string_ex(s, entries[i], x + col[i] + 6, y + row[i] + 3,
			i == im.selected ? white : black, FONT_UNICODE);
	}
	box(s, x + 6, y + panel_h - 23, panel_w - 12, 1, gray);
	snprintf(footer, sizeof(footer), "%d/%lu  空格选词  -/=翻页  %s", im.page + 1,
		(unsigned long)(im.total ? (im.total + PAGE_SIZE - 1) / PAGE_SIZE : 1), im.punctuation ? "。，" : ".,");
	fit(footer, shown, sizeof(shown), panel_w - 16);
	draw_string_ex(s, shown, x + 8, y + panel_h - 19, muted, FONT_UNICODE);
}
