#include "pinyin_engine.h"
#include "pinyinime.h"
#include <cstdio>
#include <cstring>

using namespace ime_pinyin;
static bool opened;

static size_t to_utf8(const char16 *text, size_t length, char *out, size_t cap)
{
	size_t used = 0;
	if (!cap) return 0;
	for (size_t i = 0; i < length && text[i]; i++) {
		unsigned long u = text[i];
		if (u >= 0xd800 && u <= 0xdbff && i + 1 < length &&
			text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff)
			u = 0x10000 + ((u - 0xd800) << 10) + text[++i] - 0xdc00;
		else if (u >= 0xd800 && u <= 0xdfff) u = 0xfffd;
		int bytes = u < 0x80 ? 1 : u < 0x800 ? 2 : u < 0x10000 ? 3 : 4;
		if (used + bytes >= cap) break;
		if (bytes == 1) out[used++] = u;
		else {
			out[used++] = (bytes == 2 ? 0xc0 : bytes == 3 ? 0xe0 : 0xf0) | (u >> (6 * (bytes - 1)));
			for (int b = bytes - 2; b >= 0; b--) out[used++] = 0x80 | ((u >> (6 * b)) & 0x3f);
		}
	}
	out[used] = 0;
	return used;
}

int pinyin_open(const char *dictionary, const char *user_dictionary)
{
	pinyin_close();
	if (!dictionary || !user_dictionary) return 0;
	/* Reject a dictionary for a different size_t ABI before upstream allocates. */
	FILE *fp = std::fopen(dictionary, "rb");
	if (!fp) return 0;
	size_t spelling_size = 0, spelling_count = 0;
	bool header_ok = std::fread(&spelling_size, sizeof(size_t), 1, fp) == 1 &&
		std::fread(&spelling_count, sizeof(size_t), 1, fp) == 1 &&
		spelling_size >= 2 && spelling_size <= 16 && spelling_count >= 300 && spelling_count <= 1024;
	std::fclose(fp);
	if (!header_ok) return 0;
	if (!im_open_decoder(dictionary, user_dictionary)) {
		im_close_decoder();
		return 0;
	}
	opened = true;
	im_set_max_lens(39, 16);
	im_enable_shm_as_szm(true);
	im_enable_ym_as_szm(true);
	return 1;
}

void pinyin_close(void)
{
	if (!opened) return;
	im_close_decoder();
	opened = false;
}

void pinyin_reset(void) { if (opened) im_reset_search(); }
size_t pinyin_search(const char *text, size_t len) { return opened ? im_search(text, len) : 0; }
size_t pinyin_choose(size_t index) { return opened ? im_choose(index) : 0; }
size_t pinyin_cancel_choice(void) { return opened ? im_cancel_last_choice() : 0; }
size_t pinyin_fixed(void) { return opened ? im_get_fixed_len() : 0; }
size_t pinyin_syllables(void)
{
	const uint16 *starts = NULL;
	return opened ? im_get_spl_start_pos(starts) : 0;
}
size_t pinyin_fixed_bytes(void)
{
	const uint16 *starts = NULL;
	size_t n = opened ? im_get_spl_start_pos(starts) : 0;
	size_t fixed = pinyin_fixed();
	return starts && fixed <= n ? starts[fixed] : 0;
}
size_t pinyin_decoded(void)
{
	size_t decoded = 0;
	if (opened) im_get_sps_str(&decoded);
	return decoded;
}
int pinyin_candidate(size_t index, char *utf8, size_t cap)
{
	char16 candidate[40] = {0};
	if (cap) utf8[0] = 0;
	if (!opened || !im_get_candidate(index, candidate, 40)) return 0;
	return (int)to_utf8(candidate, 40, utf8, cap);
}
void pinyin_preedit(const char *input, char *utf8, size_t cap)
{
	char16 candidate[40] = {0};
	size_t used = 0, skip = pinyin_fixed_bytes();
	if (!cap) return;
	utf8[0] = 0;
	if (pinyin_fixed() && im_get_candidate(0, candidate, 40))
		used = to_utf8(candidate, pinyin_fixed(), utf8, cap);
	if (skip > std::strlen(input)) skip = 0;
	std::snprintf(utf8 + used, cap - used, "%s", input + skip);
}
