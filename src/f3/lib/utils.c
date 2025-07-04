#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>

#include <f3/utils.h>
#include <f3/version.h>
#include <f3/platform/platform_compat.h>

void msleep(double wait_ms)
{
	msleep_compat(wait_ms);
}

int f3_fdatasync(int fd)
{
	return fdatasync_compat(fd);
}

int f3_posix_fadvise(int fd, off_t offset, off_t len, int advice)
{
	return posix_fadvise_compat(fd, offset, len, advice);
}

void adjust_dev_path(const char **dev_path)
{
	if (chdir(*dev_path)) {
		err(errno, "Can't change working directory to %s at %s()", *dev_path, __func__);
	}
	*dev_path = ".";

	if (!chroot(*dev_path)) {
		assert(!chdir("/"));
	} else if (errno != EPERM) {
		err(errno, "Can't change root directory to %s at %s()", *dev_path, __func__);
	}
}

const char *adjust_unit(double *ptr_bytes)
{
	const char *units[] = { "Byte", "KB", "MB", "GB", "TB", "PB", "EB" };
	int i = 0;
	double final = *ptr_bytes;

	while (i < 6 && final >= 1024) {
		final /= 1024;
		i++;
	}
	*ptr_bytes = final;
	return units[i];
}

int is_my_file(const char *filename)
{
	const char *p = filename;

	if (!p || !isdigit(*p))
		return 0;

	/* Skip digits. */
	do {
		p++;
	} while (isdigit(*p));

	return	(p[0] == '.') && (p[1] == 'h') && (p[2] == '2') &&
		(p[3] == 'w') && (p[4] == '\0');
}

char *full_fn_from_number(const char **filename, const char *path, long num)
{
	char *str;
	assert(asprintf(&str, "%s/%li.h2w", path, num + 1) > 0);
	*filename = str + strlen(path) + 1;
	return str;
}

static long number_from_filename(const char *filename)
{
	const char *p;
	long num;

	assert(is_my_file(filename));

	p = filename;
	num = 0;
	do {
		num = num * 10 + (*p - '0');
		p++;
	} while (isdigit(*p));

	return num - 1;
}

static inline bool include_this_file(const char *filename,
	long start_at, long end_at, long *number)
{
	if (!is_my_file(filename))
		return false;

	*number = number_from_filename(filename);

	return start_at <= *number && *number <= end_at;
}

static long count_files(const char *path, long start_at, long end_at)
{
	DIR *dir = opendir(path);
	struct dirent *entry;
	long dummy, total = 0;

	if (!dir)
		err(errno, "Can't open path %s at %s()", path, __func__);

	entry = readdir(dir);
	while (entry) {
		if (include_this_file(entry->d_name, start_at, end_at, &dummy))
			total++;
		entry = readdir(dir);
	}
	closedir(dir);

	return total;
}

/* Don't call this function directly, use ls_my_files() instead. */
static long *__ls_my_files(const char *path, long start_at, long end_at,
	long *pcount)
{
	long total_files = count_files(path, start_at, end_at);
	DIR *dir;
	struct dirent *entry;
	long *ret, index;

	assert(total_files >= 0);
	ret = malloc(sizeof(*ret) * (total_files + 1));
	assert(ret);

	dir = opendir(path);
	if (!dir)
		err(errno, "Can't open path %s at %s()", path, __func__);

	entry = readdir(dir);
	index = 0;
	while (entry) {
		long number;
		if (include_this_file(entry->d_name, start_at, end_at,
				&number)) {
			if (index >= total_files) {
				/* The folder @path received more files
				 * before we finished scanning it.
				 */
				closedir(dir);
				free(ret);
				return NULL;
			}
			ret[index++] = number;
		}

		entry = readdir(dir);
	}
	closedir(dir);

	ret[index] = -1;
	*pcount = index;
	return ret;
}

/* To be used with qsort(3). */
static int cmpintp(const void *p1, const void *p2)
{
	return *(const long *)p1 - *(const long *)p2;
}

const long *ls_my_files(const char *path, long start_at, long end_at)
{
	long *ret, my_count;

	do {
		ret = __ls_my_files(path, start_at, end_at, &my_count);
	} while (!ret);

	qsort(ret, my_count, sizeof(*ret), cmpintp);
	return ret;
}

long arg_to_long(const struct argp_state *state, const char *arg)
{
	char *end;
	long l;

	if (!arg)
		argp_error(state, "An integer must be provided");

	l = strtol(arg, &end, 0);
	if (!*arg || *end)
		argp_error(state, "`%s' is not an integer", arg);
	return l;
}

void print_header(FILE *f, const char *name)
{
	fprintf(f,
	"F3 %s " F3_STR_VERSION "\n"
	"Copyright (C) 2010 Digirati Internet LTDA.\n"
	"This is free software; see the source for copying conditions.\n"
	"\n", name);
}
