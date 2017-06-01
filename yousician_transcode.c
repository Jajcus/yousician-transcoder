
/*
Copyright (c) 2017 Jacek Konieczny

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#define CACHE_DIR_PATH "unity3d/Yousician/Yousician/HTTPCache/"
#define CONFIG_DIR_PATH ".config/"

#define DEFAULT_TRANSCODER "sox -t mp3 - -t ogg -"

#define IO_BUF_SIZE 4096

static char cache_dir[PATH_MAX];
static size_t cache_dir_l;

/*
 * Open 'input' HTTP cache file, read HTTP headers, check if the file is MPEG
 * audio. If so, transcode the contents to OGG Vorbis and write to output.
 *
 * Return 1 when file was transcoded successfully
 *
 * not re-entrant!
 */
static int transcode(const char * input, const char * output) {
FILE * in_f = NULL;
FILE * out_f = NULL;
char buf[IO_BUF_SIZE];
char * p;
int transcoded = 0;
int is_mp3 = 0;

	in_f = fopen(input, "rb");
	if (!in_f) {
		perror(input);
		goto cleanup;
	}
	out_f = fopen(output, "wb");
	if (!out_f) {
		perror(output);
		goto cleanup;
	}

	// read and copy the headers, abort early if Content-Type is not audio/mpeg
	while(1) {
		p = fgets(buf, IO_BUF_SIZE, in_f);
		if (!p) {
			perror("fgets");
			goto cleanup;
		}
		if (buf[0] == '\r' && buf[1] == '\n' && buf[2] == '\000') break;
		if (strncmp(buf, "content-type: ", 14) == 0) {
			p = buf + 14;
			if (strncmp(p, "audio/mpeg", 10) != 0) {
				fprintf(stderr, "not transcoding %s, not MP3\n", input);
				goto cleanup;
			}
			is_mp3 = 1;
			continue;
		}
		if (fputs(buf, out_f) == 0) {
			perror("fputs");
			goto cleanup;
		}
	}
	if (!is_mp3) {
		fprintf(stderr, "not transcoding %s, not MP3\n", input);
		goto cleanup;
	}
	if (fputs("content-type: application/ogg\r\n\r\n", out_f) == 0) {
		perror("fputs");
		goto cleanup;
	}
	fflush(out_f);

	fprintf(stderr, "Transcoding %s to %s\n", input, output);

	// actual position on the file descriptor may be somewhere after
	// the current FILE * pointer, due to libc buffering
	long input_pos = ftell(in_f);
	int in_fd = fileno(in_f);
	int out_fd = fileno(out_f);

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		goto cleanup;
	}
	if (pid == 0) {
		/* child process */
		if (dup2(in_fd, STDIN_FILENO) == -1) {
			perror("dup2(stdin)");
			_exit(1);
		}
		close(in_fd);
		if (lseek(STDIN_FILENO, input_pos, SEEK_SET) == -1) {
			perror("lseek(stdin)");
			_exit(1);
		}
		if (dup2(out_fd, STDOUT_FILENO) == -1) {
			perror("dup2(stdout)");
			_exit(1);
		}
		unsetenv("LD_PRELOAD");
		const char * transcoder = getenv("YOUSICIAN_TRANSCODER");
		if (!transcoder) transcoder = DEFAULT_TRANSCODER;
		execl("/bin/sh", "sh", "-c", transcoder, (char *) 0);
		perror("execl");
		_exit(1);
	}
	else {
		int wstatus;
		pid_t rpid = waitpid(pid, &wstatus, 0);
		if (rpid == -1) {
			perror("waitpid");
			goto cleanup;
		}
		if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
			fprintf(stderr, "Transcoder failed!\n");
			goto cleanup;
		}
	}

	fprintf(stderr, "Transcoded!\n");
	transcoded = 1;

cleanup:
	if (in_f) fclose(in_f);
	if (out_f) fclose(out_f);
	if (!transcoded) unlink(output);

	return transcoded;
}

int (*orig_open)(const char * pathname, int flags, ...);
/*
 * open() syscall wrapper
 *
 * For open() calls for files inside Yousician HTTPCache directory:
 * - on open for read:
 *   - open transcoded files instead of the original one, if transcoded file exists
 *   - if there is no transcoded file - find out if transcoding is possible
 *     - if it is, transcode the file (from MP3 to OGG) and open the transcoded file
 *     - otherwise, or when transcoding fails, open the original file
 *
 * For other paths: just call the original open()
 */
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

	char transcoded_path[PATH_MAX];
	int n;

	n = snprintf(transcoded_path, PATH_MAX, "%s.transcoded", path);
	if (n >= PATH_MAX) {
		fprintf(stderr, "path buffer overflow\n");
		errno = EOVERFLOW;
		return -1;
	}

	struct stat st;
	int exists = (stat(transcoded_path, &st) == 0);

	if ((flags & O_ACCMODE) == O_WRONLY || (flags & O_ACCMODE) == O_RDWR) {
		if (exists) {
			fprintf(stderr, "removing stale transcoded file\n");
			unlink(path);
		}
	}
	else {
		if (exists) {
			fprintf(stderr, "Opening existing %s\n", transcoded_path);
			path = transcoded_path;
		}
		else if (transcode(path, transcoded_path)) {
			fprintf(stderr, "Opening transcoded %s\n", transcoded_path);
			path = transcoded_path;
		}
	}

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
	fprintf(stderr, "Yousician HTTP cache dir: %s (%i bytes)\n", cache_dir, (int)cache_dir_l);
}
