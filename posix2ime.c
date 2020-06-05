/**
* Infinite Memory Engine - POSIX to IME native wrapper
*
* Copyright (c) 2020, DataDirect Networks.
******************************************************************************/

#define _GNU_SOURCE
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include "ime_native.h"

#define BFS_PATH_ENV "IM_CLIENT_BFS_PATH"

static ssize_t (*real_read)(int fd, void *buf, size_t count) = NULL;
static ssize_t (*real_write)(int fd, const void *buf, size_t count) = NULL;
static int     (*real_open)(const char *pathname, int flags, ...) = NULL;
static int     (*real___open_2)(const char *pathname, int flags) = NULL;
static int     (*real_close)(int fd) = NULL;
static int     (*real_access)(const char *pathname, int mode) = NULL;
static int     (*real_fsync)(int fd) = NULL;
static int     (*real_unlink)(const char *pathname) = NULL;
static int     (*real_stat)(const char *pathname, struct stat *statbuf) = NULL;
static int     (*real_lstat)(const char *pathname, struct stat *statbuf) = NULL;
static off64_t (*real_lseek)(int fd, off64_t offset, int whence) = NULL;
static int     (*real_statvfs)(const char *path, struct statvfs *buf) = NULL;
static DIR*    (*real_opendir)(const char *name) = NULL;
static int     (*real_mkdir)(const char *pathname, mode_t mode) = NULL;
static int     (*real_rmdir)(const char *pathname) = NULL;
static int     (*real_mknod)(const char *pathname, mode_t mode, dev_t dev) = NULL;
static int     (*real_execve)(const char *filename, char *const argv[],
                    char *const envp[]) = NULL;
static int     (*real_libc_start_main)(int (*main) (int,char **,char **),
                    int argc, char **ubp_av,
                    void (*init) (void),
                    void (*fini)(void),
                    void (*rtld_fini)(void),
                    void (*stack_end)) = NULL;

static bool is_init = false;
static char client_bfs_path[PATH_MAX] = {0};
static bool enable_client_bfs = false;

static void init_real(void)
{
    real_mknod           = dlsym(RTLD_NEXT, "mknod");
    real_opendir         = dlsym(RTLD_NEXT, "opendir");
    real_mkdir           = dlsym(RTLD_NEXT, "mkdir");
    real_rmdir           = dlsym(RTLD_NEXT, "rmdir");
    real_statvfs         = dlsym(RTLD_NEXT, "statvfs");
    real_unlink          = dlsym(RTLD_NEXT, "unlink");
    real_stat            = dlsym(RTLD_NEXT, "stat");
    real_lstat           = dlsym(RTLD_NEXT, "lstat");
    real_fsync           = dlsym(RTLD_NEXT, "fsync");
    real_access          = dlsym(RTLD_NEXT, "access");
    real_execve          = dlsym(RTLD_NEXT, "execve");
    real_lseek           = dlsym(RTLD_NEXT, "lseek");
    real_open            = dlsym(RTLD_NEXT, "open");
    real___open_2        = dlsym(RTLD_NEXT, "__open_2");
    real_close           = dlsym(RTLD_NEXT, "close");
    real_write           = dlsym(RTLD_NEXT, "write");
    real_read            = dlsym(RTLD_NEXT, "read");
    real_libc_start_main = dlsym(RTLD_NEXT, "__libc_start_main");
}

int unlink(const char *pathname)
{
    if (!is_init || pathname == NULL)
    {
        real_unlink = dlsym(RTLD_NEXT, "unlink");
        return real_unlink(pathname);
    }
    else
        return ime_native_unlink(pathname);
}

int mknod(const char *pathname, mode_t mode, dev_t dev)
{
    if (!is_init)
    {
        real_mknod = dlsym(RTLD_NEXT, "mknod");
        return real_mknod(pathname, mode, dev);
    }
    else
        return ime_native_mknod(pathname, mode, dev);
}

int rmdir(const char *pathname)
{
    if (!is_init || pathname == NULL)
    {
        real_rmdir = dlsym(RTLD_NEXT, "rmdir");
        return real_rmdir(pathname);
    }
    else
        return ime_native_rmdir(pathname);
}

int mkdir(const char *pathname, mode_t mode)
{
    if (!is_init)
    {
        real_mkdir = dlsym(RTLD_NEXT, "mkdir");
        return real_mkdir(pathname, mode);
    }
    else
        return ime_native_mkdir(pathname, mode);
}

int statvfs(const char *path, struct statvfs *buf)
{
    if (!is_init)
    {
        real_statvfs = dlsym(RTLD_NEXT, "statvfs");
        return real_statvfs(path, buf);
    }
    else
        return ime_native_statvfs(path, buf);
}

int stat(const char *pathname, struct stat *statbuf)
{
    if (!is_init)
    {
        real_stat = dlsym(RTLD_NEXT, "stat");
        return real_stat(pathname, statbuf);
    }
    else
        return ime_native_stat(pathname, statbuf);
}

int lstat(const char *pathname, struct stat *statbuf)
{
    if (!is_init)
    {
        real_lstat = dlsym(RTLD_NEXT, "lstat");
        return real_lstat(pathname, statbuf);
    }
    else
        return ime_native_stat(pathname, statbuf);
}

int fsync(int fd)
{
    if (!is_init)
    {
        real_fsync = dlsym(RTLD_NEXT, "fsync");
        return real_fsync(fd);
    }
    else
        return ime_client_native2_fsync(fd);
}

int access(const char *pathname, int mode)
{
    if (!is_init)
    {
        real_access = dlsym(RTLD_NEXT, "access");
        return real_access(pathname, mode);
    }
    else
        return ime_native_access(pathname, mode);
}

off64_t lseek(int fd, off64_t offset, int whence)
{
    if (!is_init)
    {
        real_lseek = dlsym(RTLD_NEXT, "lseek");
        return real_lseek(fd, offset, whence);
    }
    else
        return ime_native_lseek(fd, offset, whence);
}

int open(const char *pathname, int flags, ...)
{
    char tmp[PATH_MAX];
    int mode = 0;
    va_list l;

    if (flags & O_CREAT) {
        va_start(l, flags);
        mode = va_arg(l, int);
        va_end(l);
    }

    if (!is_init)
    {
        real_open = dlsym(RTLD_NEXT, "open");
        return real_open(pathname, flags, mode);
    }
    else
    {

        if ((flags & O_CREAT) &&
            enable_client_bfs &&
            !(flags & O_DIRECTORY) &&
            ime_client_native2_is_fuse_path_and_convert(pathname, tmp))
        {
            char bfs_path[PATH_MAX];
            strcpy(bfs_path, client_bfs_path);
            strcat(bfs_path, tmp);

            int ret = syscall(SYS_mknod, bfs_path, S_IFREG | mode, 0);
            if (ret < 0 && errno != EEXIST)
                return ret;
            else
                return ime_native_open(pathname, flags & ~O_CREAT, 0);
        }
        else
            return ime_native_open(pathname, flags, mode);
    }
}

int __open_2(const char *pathname, int flags)
{
    char tmp[PATH_MAX];

    if (!is_init)
    {
        real___open_2 = dlsym(RTLD_NEXT, "__open_2");
        return real___open_2(pathname, flags);
    }
    else
    {

        if (flags & O_CREAT &&
            enable_client_bfs &&
            !(flags & O_DIRECTORY) &&
            ime_client_native2_is_fuse_path_and_convert(pathname, tmp))
        {
            char bfs_path[PATH_MAX];
            strcpy(bfs_path, client_bfs_path);
            strcat(bfs_path, tmp);

            int ret = syscall(SYS_mknod, bfs_path, S_IFREG, 0);
            if (ret < 0 && errno != EEXIST)
                return ret;
            else
                return ime_native_open(pathname, flags & ~O_CREAT, 0);
        }
        else
            return ime_native_open(pathname, flags, 0);
    }
}

int close(int fd)
{
    if (!is_init)
    {
        real_close = dlsym(RTLD_NEXT, "close");
        return real_close(fd);
    }
    else
        return ime_native_close(fd);
}

ssize_t read(int fd, void *buf, size_t count)
{
    if (!is_init)
    {
        real_read = dlsym(RTLD_NEXT, "read");
        return real_read(fd, buf, count);
    }
    else
        return ime_native_read(fd, buf, count);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    if (!is_init)
    {
        real_write = dlsym(RTLD_NEXT, "write");
        return real_write(fd, buf, count);
    }
    else
        return ime_native_write(fd, buf, count);
}

DIR *opendir(const char *name)
{
    char tmp[PATH_MAX];

    if (!is_init)
    {
        real_opendir = dlsym(RTLD_NEXT, "opendir");
        return real_opendir(name);
    }
    else if (enable_client_bfs &&
             ime_client_native2_is_fuse_path_and_convert(name, tmp))
    {
        char bfs_path[PATH_MAX];
        strcpy(bfs_path, client_bfs_path);
        strcat(bfs_path, tmp);
        return real_opendir(bfs_path);
    }
    else
        return real_opendir(name);
}

int execve(const char *filename, char *const argv[], char *const envp[])
{
    if (!is_init)
        real_execve = dlsym(RTLD_NEXT, "execve");

    /* Avoid propagating LD_PRELOAD env variable */
    return real_execve(filename, argv, NULL);
}

int __libc_start_main(int (*main) (int,char **,char **),
              int argc,char **ubp_av,
              void (*init) (void),
              void (*fini)(void),
              void (*rtld_fini)(void),
              void (*stack_end))
{
    printf("POSIX 2 IME Library Loaded\n");

    char *env_tmp = getenv(BFS_PATH_ENV);
    if (env_tmp != NULL)
    {
        strcpy(client_bfs_path, env_tmp);
        enable_client_bfs = true;
    }

    init_real();

    ime_native_init();

    is_init = true;

    return real_libc_start_main(main, argc, ubp_av,
                                init, fini, rtld_fini, stack_end);
}

