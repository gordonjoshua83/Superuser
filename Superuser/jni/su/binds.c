/*
   Copyright 2016, Pierre-Hugues Husson <phh@phh.me>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <strings.h>
#include <stdint.h>
#include <pwd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <sys/types.h>
#include <selinux/selinux.h>
#include <arpa/inet.h>
#include "binds.h"

int bind_foreach(bind_cb cb, void* arg) {
    int res = 0;
    char *str = NULL;
	int fd = open("/data/su/binds", O_RDONLY);
    if(fd<0)
        return 1;

	off_t size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

    str = malloc(size);
    if(read(fd, str, size) != size)
        goto error;

    char *base = str;
    while(base < str+size) {
        char *parse_src, *parse_dst;
        int uid;

        char *ptr = memchr(base, 0, size-(base-str));
        if(ptr == NULL)
            goto error;
        sscanf(base, "%d", &uid);

        parse_src = strchr(base, ':');
        if(!parse_src)
            goto error;
        parse_src++;

        parse_dst = strchr(parse_src, ':');
        if(!parse_dst)
            goto error;
        *parse_dst = 0; // Split parse_src string
        parse_dst++;

		cb(arg, uid, parse_src, parse_dst);

        base = ptr+1;
    }

    res = 1;
error:
    if(str) free(str);
    close(fd);
    return res;
}

/*
 * The callbacks below used to be GCC nested functions, which clang (the only
 * NDK compiler) does not support. They now take their state through `arg`.
 */
struct bind_uniq_ctx {
    const char *dst;
    int res;
};

static void bind_uniq_dst_cb(void *arg, int uid, const char *src, const char *dst) {
    struct bind_uniq_ctx *ctx = arg;
    if(strcmp(dst, ctx->dst) == 0)
        ctx->res = 0;
}

int bind_uniq_dst(const char *dst) {
    struct bind_uniq_ctx ctx = { .dst = dst, .res = 1 };
    if(!bind_foreach(bind_uniq_dst_cb, &ctx))
        return 0;
    return ctx.res;
}

static void bind_ls_cb(void *arg, int uid, const char *src, const char *dst) {
    int caller_uid = *(int *)arg;
	if(caller_uid == 0 || caller_uid == 2000 || caller_uid == uid) {
		fprintf(stderr, "%d %s => %s\n", uid, src, dst);
	}
}

void bind_ls(int uid) {
    bind_foreach(bind_ls_cb, &uid);
}

struct bind_remove_ctx {
    const char *path;
    int uid;
    int fd;
    int found;
};

static void bind_remove_cb(void *arg, int uid, const char *src, const char *dst) {
    struct bind_remove_ctx *ctx = arg;
	//The one we want to drop
    if(strcmp(dst, ctx->path) == 0 &&
			(ctx->uid == 0 || ctx->uid == 2000 || ctx->uid == uid)) {
		ctx->found = 1;
		return;
	}
	char *str = NULL;
	int len = asprintf(&str, "%d:%s:%s", uid, src, dst);
	write(ctx->fd, str, len+1); //len doesn't include final \0 and we want to write it
	free(str);
}

int bind_remove(const char *path, int uid) {
    struct bind_remove_ctx ctx = { .path = path, .uid = uid, .fd = -1, .found = 0 };

	unlink("/data/su/binds.new");
	ctx.fd = open("/data/su/binds.new", O_WRONLY|O_CREAT, 0600);
	if(ctx.fd<0)
		return 0;

    bind_foreach(bind_remove_cb, &ctx);
	close(ctx.fd);
	unlink("/data/su/binds");
	rename("/data/su/binds.new", "/data/su/binds");
	return ctx.found;
}

int init_foreach(init_cb icb, void* arg) {
    int res = 0;
    char *str = NULL;
	int fd = open("/data/su/init", O_RDONLY);
    if(fd<0)
        return 1;

	off_t size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

    str = malloc(size);
    if(read(fd, str, size) != size)
        goto error;

    char *base = str;
    while(base < str+size) {
        char *parsed;
        int uid;

        char *ptr = memchr(base, 0, size-(base-str));
        if(ptr == NULL)
            goto error;
        sscanf(base, "%d", &uid);

        parsed = strchr(base, ':');
        if(!parsed)
            goto error;
        parsed++;


		icb(arg, uid, parsed);

        base = ptr+1;
    }

    res = 1;
error:
    if(str) free(str);
    close(fd);
    return res;
}

struct init_uniq_ctx {
    const char *path;
    int res;
};

static void init_uniq_cb(void *arg, int uid, const char *path) {
    struct init_uniq_ctx *ctx = arg;
    if(strcmp(path, ctx->path) == 0)
        ctx->res = 0;
}

int init_uniq(const char *path) {
    struct init_uniq_ctx ctx = { .path = path, .res = 1 };
    if(!init_foreach(init_uniq_cb, &ctx))
        return 0;
    return ctx.res;
}

struct init_remove_ctx {
    const char *path;
    int uid;
    int fd;
    int found;
};

static void init_remove_cb(void *arg, int uid, const char *path) {
    struct init_remove_ctx *ctx = arg;
	//The one we want to drop
    if(strcmp(path, ctx->path) == 0 &&
			(ctx->uid == 0 || ctx->uid == 2000 || uid == ctx->uid)) {
		ctx->found = 1;
		return;
	}
	char *str = NULL;
	int len = asprintf(&str, "%d:%s", uid, path);
	write(ctx->fd, str, len+1); //len doesn't include final \0 and we want to write it
	free(str);
}

int init_remove(const char *path, int uid) {
    struct init_remove_ctx ctx = { .path = path, .uid = uid, .fd = -1, .found = 0 };

	unlink("/data/su/init.new");
	ctx.fd = open("/data/su/init.new", O_WRONLY|O_CREAT, 0600);
	if(ctx.fd<0)
		return 0;

    init_foreach(init_remove_cb, &ctx);
	close(ctx.fd);
	unlink("/data/su/init");
	rename("/data/su/init.new", "/data/su/init");
	return ctx.found;
}

static void init_ls_cb(void *arg, int uid, const char *path) {
    int caller_uid = *(int *)arg;
	if(caller_uid == 2000 || caller_uid == 0 || caller_uid == uid)
		fprintf(stderr, "%d %s\n", uid, path);
}

void init_ls(int uid) {
    init_foreach(init_ls_cb, &uid);
}
