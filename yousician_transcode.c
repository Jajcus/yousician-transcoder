#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <string.h>
#include <stdarg.h>

#define CACHE_DIR_PATH "unity3d/Yousician/Yousician/HTTPCache/"
#define CONFIG_DIR_PATH ".config/"

static char cache_dir[PATH_MAX];
static size_t cache_dir_l;

int (*orig_open)(const char * pathname, int flags, ...);

int open(const char * path, int flags, ...) {

	va_list ap;
	int mode;
	va_start(ap, flags);
	if (flags & O_CREAT)
		mode = va_arg(ap, int);
	else
		mode = 0;
	va_end(ap);

	int saved_errno = errno;
	if (strncmp(path, cache_dir, cache_dir_l) != 0) {
		return orig_open(path, flags, mode);
	}
	fprintf(stderr, "wrapping open(\"%s\", %i, %i)\n", path, flags, mode);
	errno = saved_errno;
	return orig_open(path, flags, mode);
}

void _init(void) {

	fprintf(stderr, "Loading Yousician Transcoder...\n");
	orig_open = dlsym(RTLD_NEXT, "open");
	if (!orig_open) {
		fprintf(stderr, "%s\n", dlerror());
		abort();
	}

	int n;
	const char * config_dir = getenv("XDG_CONFIG_HOME");
	if (config_dir) {
		n = snprintf(cache_dir, PATH_MAX, "%s/" CACHE_DIR_PATH, config_dir);
	}
	else {
		const char * home_dir = getenv("HOME");
		if (!home_dir) {
			fprintf(stderr, "Neither $XDG_CONFIG_HOME nor $HOME set\n");
			abort();
		}
		n = snprintf(cache_dir, PATH_MAX, "%s/" CONFIG_DIR_PATH CACHE_DIR_PATH, home_dir);
	}
	if (n >= PATH_MAX) {
		fprintf(stderr, "cache path too long\n");
		abort();
	}
	cache_dir_l = n;
	fprintf(stderr, "Yousician HTTP cache dir: %s\n", cache_dir);
}
