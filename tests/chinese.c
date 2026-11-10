/* Exercise terminal column semantics and the actual SDL bitmap renderer. */
#define main terminal_main
#include "../st.c"
#undef main
#include <assert.h>

static void feed(const char *text)
{
	while (*text) {
		char c[UTF_SIZ] = {0};
		int len = utf8size((char *)text);
		memcpy(c, text, len);
		tputc(c, len);
		text += len;
	}
}

static void check_pairs(void)
{
	int x, y;
	for (y = 0; y < term.row; y++)
		for (x = 0; x < term.col; x++) {
			Glyph *g = &term.line[y][x];
			if (!(g->state & GLYPH_SET)) continue;
			if (g->mode & ATTR_WIDE) {
				assert(x + 1 < term.col);
				assert(term.line[y][x + 1].state & GLYPH_SET);
				assert(term.line[y][x + 1].mode & ATTR_WDUMMY);
			}
			if (g->mode & ATTR_WDUMMY) {
				assert(x > 0);
				assert(term.line[y][x - 1].state & GLYPH_SET);
				assert(term.line[y][x - 1].mode & ATTR_WIDE);
			}
		}
}

static void test_columns(void)
{
	tnew(8, 4);
	sel.bx = -1;
	feed("A中文B");
	assert(term.c.x == 6);
	assert(term.line[0][1].mode & ATTR_WIDE);
	assert(term.line[0][2].mode & ATTR_WDUMMY);
	assert(term.line[0][3].mode & ATTR_WIDE);
	assert(term.line[0][5].c[0] == 'B');
	feed("\033[1;3HX"); /* overwrite the trailing half */
	assert(!(term.line[0][1].state & GLYPH_SET));
	assert(term.line[0][2].c[0] == 'X');
	tclearregion(4, 0, 4, 0); /* erase half of 文 */
	assert(!(term.line[0][3].state & GLYPH_SET));
	assert(!(term.line[0][4].state & GLYPH_SET));

	treset();
	feed("1234567中"); /* last column cannot contain half a glyph */
	assert(term.c.y == 1 && term.c.x == 2);
	assert(term.line[1][0].mode & ATTR_WIDE);
	treset();
	feed("123456中"); /* exact fit leaves delayed wrap on last column */
	assert(term.c.x == 7 && (term.c.state & CURSOR_WRAPNEXT));
	feed("B");
	assert(term.c.y == 1 && term.c.x == 1);
	treset();
	feed("\033[?7l1234567中"); /* no-wrap mode */
	assert(term.c.y == 0);
	check_pairs();

	treset();
	feed("中AB");
	tmoveto(1, 0);
	tdeletechar(1);
	check_pairs();
	assert(term.line[0][1].c[0] == 'A');
	treset();
	feed("中AB");
	tmoveto(1, 0);
	tinsertblank(1);
	check_pairs();
	treset();
	feed("123456中");
	tresize(7, 4);
	check_pairs();
	assert(!(term.line[0][6].state & GLYPH_SET));
	tresize(8, 4);

	/* Mixed edits must never leave an orphan half behind. */
	srand(1);
	for (int i = 0; i < 5000; i++) {
		switch (rand() % 7) {
		case 0: feed("中"); break;
		case 1: feed("A"); break;
		case 2: tmoveto(rand() % term.col, rand() % term.row); break;
		case 3: tinsertblank(rand() % 4 + 1); break;
		case 4: tdeletechar(rand() % 4 + 1); break;
		case 5: tclearregion(rand() % term.col, term.c.y, rand() % term.col, term.c.y); break;
		case 6: feed("\n"); break;
		}
		check_pairs();
	}
	treset();
	tresize(1, 4);
	feed("中");
	assert(!(term.line[0][0].mode & ATTR_WIDE));
	tresize(8, 4);
}

static void test_font(const char *actual_font)
{
	char path[] = "/tmp/st-font-test-XXXXXX";
	int fd = mkstemp(path);
	FILE *fp = fdopen(fd, "w");
	assert(fp);
	fputs("0041:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\n", fp);
	fputs("4E2D:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\n", fp);
	fclose(fp);
	assert(load_unicode_font(path));
	assert(get_display_font() == FONT_UNICODE);
	assert(font_char_width(FONT_UNICODE) == 8 && font_line_height(FONT_UNICODE) == 16);
	assert(unicode_width(0x4e2d) == 2 && unicode_width('A') == 1);
	assert(unicode_width(0xff0c) == 2 && unicode_width(0x20000) == 2);
	SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE, 480, 272, 16, 0xf800, 0x7e0, 0x1f, 0);
	assert(s);
	unsigned short ink = SDL_MapRGB(s->format, 255, 255, 255);
	SDL_FillRect(s, NULL, 0);
	draw_string_ex(s, "中A", 0, 0, ink, FONT_UNICODE);
	unsigned short *pixels = s->pixels;
	assert(pixels[15] == ink && pixels[16] == ink && pixels[23] == ink && pixels[24] == 0);
	/* Bounded four-byte glyphs, malformed input and negative clipping. */
	const char four[4] = {'\xf0', '\xa0', '\x80', '\x80'};
	draw_text_ex(s, four, sizeof(four), 470, 260, ink, FONT_UNICODE);
	draw_text_ex(s, "\xe4", 1, -2, -2, ink, FONT_UNICODE);
	draw_string_ex(s, "中A", -15, -15, ink, FONT_UNICODE);
	draw_string_ex(s, "中文", -5, -5, ink, FONT_CONSOLE);
	draw_string_ex(s, "中文", -5, -5, ink, FONT_EMBEDDED);

	fp = fopen(path, "w");
	assert(fp);
	fputs("4E2D:XYZ\n", fp);
	fclose(fp);
	assert(!load_unicode_font(path));
	assert(!load_unicode_font("/nonexistent/st-test.hex"));
	unlink(path);
	toggle_display_font(); assert(get_display_font() == FONT_CONSOLE);
	toggle_display_font(); assert(get_display_font() == FONT_EMBEDDED);
	toggle_display_font(); assert(get_display_font() == FONT_UNICODE);

	if (actual_font) {
		assert(load_unicode_font(actual_font));
		SDL_FillRect(s, NULL, SDL_MapRGB(s->format, 16, 20, 24));
		draw_string_ex(s, "D88 RetroFW / GMenuNX\n中文显示测试：你好，世界！\n简体中文 / 繁體中文\n文件：游戏 存档 设置 终端\nASCII + 中文 + 1234567890\n┌──────────────┐\n│ 中文双列对齐 │\n└──────────────┘", 8, 8, ink, FONT_UNICODE);
		assert(SDL_SaveBMP(s, ".build/chinese-preview.bmp") == 0);
	}
	SDL_FreeSurface(s);
}

int main(int argc, char **argv)
{
	assert(SDL_Init(0) == 0);
	test_columns();
	test_font(argc > 1 ? argv[1] : NULL);
	/* Draw terminal buffers and cursor on both halves of a Chinese glyph. */
	xw.win = SDL_CreateRGBSurface(SDL_SWSURFACE, 80, 80, 16, 0xf800, 0x7e0, 0x1f, 0);
	assert(xw.win);
	xw.cw = 8; xw.ch = 16; xw.w = 80; xw.h = 80;
	xw.state = WIN_VISIBLE | WIN_FOCUSED;
	initcolormap();
	treset();
	feed("中A");
	tmoveto(0, 0);
	drawregion(0, 0, term.col, term.row);
	tmoveto(1, 0);
	xdrawcursor();
	assert(term.line[0][0].c[0] == (char)0xe4 && term.line[0][0].c[3] == 0);
	SDL_FreeSurface(xw.win);
	SDL_Quit();
	puts("Chinese column, edit, wrap, resize, cursor and bitmap rendering tests passed.");
	return 0;
}
