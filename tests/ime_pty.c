/* Drive the application's key handlers into real shell and Vim PTYs. */
#define main terminal_main
#include "../st.c"
#undef main
#include <assert.h>

static pid_t test_child = -1;

static void expired(int unused)
{
	(void)unused;
	if (test_child > 0) kill(test_child, SIGKILL);
	_exit(2);
}

static void event(int type, SDLKey key, unsigned int u, SDLMod mod)
{
	SDL_Event e = {0};
	e.type = e.key.type = type;
	e.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
	e.key.keysym.sym = key;
	e.key.keysym.unicode = u;
	e.key.keysym.mod = mod;
	if (!handle_keyboard_event(&e) && type == SDL_KEYDOWN) kpress(&e);
}

static void press(SDLKey key, unsigned int u)
{
	event(SDL_KEYDOWN, key, u, KMOD_NONE);
	event(SDL_KEYUP, key, 0, KMOD_NONE);
}

static void text(const char *s)
{
	while (*s) {
		unsigned char c = *s++;
		press(c == '\r' ? SDLK_RETURN : (SDLKey)c, c);
	}
}

static void shift(void)
{
	event(SDL_KEYDOWN, SDLK_LSHIFT, 0, KMOD_LSHIFT);
	event(SDL_KEYUP, SDLK_LSHIFT, 0, KMOD_NONE);
}

static void chinese_sentence(void)
{
	shift(); assert(ime_enabled());
	text("nihao");
	assert(!strcmp(ime_candidate(0), "你好"));
	press(SDLK_SPACE, ' ');
	press(SDLK_COMMA, ',');
	text("shijie");
	assert(!strcmp(ime_candidate(0), "世界"));
	press(SDLK_SPACE, ' ');
	event(SDL_KEYDOWN, SDLK_LSHIFT, 0, KMOD_LSHIFT);
	event(SDL_KEYDOWN, SDLK_1, '!', KMOD_LSHIFT);
	event(SDL_KEYUP, SDLK_1, 0, KMOD_LSHIFT);
	event(SDL_KEYUP, SDLK_LSHIFT, 0, KMOD_NONE);
	assert(ime_enabled());
}

static void spawn(char *const argv[])
{
	int master, slave;
	struct winsize w = {16, 59, 0, 0};
	assert(open_pty_pair(&master, &slave, &w) == 0);
	test_child = fork();
	assert(test_child >= 0);
	if (!test_child) {
		setsid();
		assert(ioctl(slave, TIOCSCTTY, NULL) == 0);
		dup2(slave, 0); dup2(slave, 1); dup2(slave, 2);
		close(master); close(slave);
		setenv("TERM", "xterm", 1);
		execv(argv[0], argv);
		_exit(127);
	}
	close(slave);
	cmdfd = master;
	alarm(25);
}

static int read_output(char *buffer, size_t cap)
{
	fd_set set;
	struct timeval timeout = {0, 50000};
	FD_ZERO(&set); FD_SET(cmdfd, &set);
	if (select(cmdfd + 1, &set, NULL, NULL, &timeout) <= 0) return 0;
	ssize_t n = read(cmdfd, buffer, cap - 1);
	if (n <= 0) return 0;
	buffer[n] = 0;
	return n;
}

static void wait_text(const char *token)
{
	char collected[16384] = {0}, chunk[2048];
	size_t used = 0;
	while (!strstr(collected, token)) {
		int n = read_output(chunk, sizeof(chunk));
		if (!n) continue;
		if (used + n >= sizeof(collected)) {
			memmove(collected, collected + used / 2, used - used / 2);
			used -= used / 2;
		}
		memcpy(collected + used, chunk, n + 1); used += n;
	}
}

static void finish(void)
{
	int status;
	char chunk[2048];
	while (waitpid(test_child, &status, WNOHANG) == 0) read_output(chunk, sizeof(chunk));
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	test_child = -1;
	alarm(0);
	close(cmdfd);
}

static void check_file(const char *path, const char *expected)
{
	char data[256] = {0};
	FILE *fp = fopen(path, "rb");
	assert(fp);
	size_t n = fread(data, 1, sizeof(data) - 1, fp);
	fclose(fp);
	assert(n == strlen(expected) && !memcmp(data, expected, n));
	unlink(path);
}

int main(int argc, char **argv)
{
	assert(argc >= 2);
	signal(SIGALRM, expired);
	assert(SDL_Init(0) == 0);
	char prefix[] = "/tmp/st-ime-pty-XXXXXX";
	int fd = mkstemp(prefix);
	assert(fd >= 0); close(fd); unlink(prefix);
	char user[128], shell_file[128], vim_file[128], ready[128], command[1024];
	snprintf(user, sizeof(user), "%s.user", prefix);
	snprintf(shell_file, sizeof(shell_file), "%s.sh.txt", prefix);
	snprintf(vim_file, sizeof(vim_file), "%s.vim.txt", prefix);
	snprintf(ready, sizeof(ready), "%s.ready", prefix);
	assert(ime_init(argv[1], user));
	init_keyboard();
	press(SDLK_F1, 0);
	tnew(59, 16);

	snprintf(command, sizeof(command), "printf 'IME-SHELL-READY\\n'; IFS= read -r line; printf '%%s' \"$line\" > '%s'; printf 'IME-SHELL-DONE\\n'", shell_file);
	char *shell[] = {"/bin/sh", "-c", command, NULL};
	spawn(shell);
	wait_text("IME-SHELL-READY");
	chinese_sentence();
	shift(); assert(!ime_enabled());
	press(SDLK_RETURN, '\r');
	wait_text("IME-SHELL-DONE");
	finish();
	check_file(shell_file, "你好，世界！");
	puts("Real /bin/sh received the UTF-8 sentence through Shift -> Pinyin -> terminal key handler -> PTY.");

	const char *vim = argc > 2 ? argv[2] : "/usr/bin/vim";
	snprintf(command, sizeof(command), "set encoding=utf-8 fileencoding=utf-8 noswapfile ttimeoutlen=20 timeoutlen=50 | call writefile(['ready'], '%s')", ready);
	char *vim_args[] = {(char *)vim, "-N", "-u", "NONE", "-n", "-i", "NONE", "-T", "xterm", "-c", command, vim_file, NULL};
	spawn(vim_args);
	char chunk[2048];
	while (access(ready, R_OK) != 0) read_output(chunk, sizeof(chunk));
	unlink(ready);
	press(SDLK_i, 'i');
	chinese_sentence();
	press(SDLK_ESCAPE, 27);
	assert(!ime_enabled());
	text(":wq\r");
	finish();
	check_file(vim_file, "你好，世界！\n");
	puts("Real Vim inserted Chinese, accepted Esc into Normal mode, and saved matching UTF-8 bytes with :wq.");
	ime_shutdown();
	unlink(user);
	SDL_Quit();
	return 0;
}
