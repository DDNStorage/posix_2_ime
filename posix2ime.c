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

#if defined _LIBC || defined _IO_MTSAFE_IO
#include <pthread.h>
#endif

/* IME environment variable to enable/disable min connections mode: a client
 * process connects to only one network interface per IME server node) */
#define MIN_CONNECTIONS_ENV "IM_CLIENT_MIN_CONNECTIONS"

/* OPTIONAL: Defines IME root path to the Backing File System on the compute nodes */
#define BFS_PATH_ENV        "IM_CLIENT_BFS_PATH"

/* OPTIONAL: Disables open O_CREAT conversion to mknod + native open */
#define NO_MKNOD_CREATE_ENV "IM_CLIENT_NO_MKNOD_CREATE"

/* OPTIONAL: Disables opendir redirection to the Backing File System */
#define NO_BFS_OPENDIR_ENV  "IM_CLIENT_NO_BFS_OPENDIR"

/* OPTIONAL: Disables large buffer for opendir */
#define NO_LARGE_DIR_ENV    "IM_CLIENT_NO_LARGE_DIR_BUFFER"

#define OPENDIR_BUFFER_SZ   1052672


typedef struct __dirstream
{
    int fd;                     /* File descriptor.  */
#if defined _LIBC || defined _IO_MTSAFE_IO
    pthread_mutex_t lock;
#else
    int lock;
#endif
    size_t allocation;          /* Space allocated for the block.  */
    size_t size;                /* Total valid data in the block.  */
    size_t offset;              /* Current offset into the block.  */
    off_t filepos;              /* Position of next entry to read.  */
    int errcode;                /* Delayed error code.  */
    /* Directory block.  We must make sure that this block starts
       at an address that is aligned adequately enough to store
       dirent entries.  Using the alignment of "void *" is not
       sufficient because dirents on 32-bit platforms can require
       64-bit alignment.  We use "long double" here to be consistent
       with what malloc uses.  */
    char data[0] __attribute__ ((aligned (__alignof__ (long double))));
} DIR;

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
static bool enable_mknod_create = false;
static bool enable_bfs_opendir = false;
static bool enable_large_dir_buffer = false;

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

int __attribute__((optimize("O0"))) unlink(const char *pathname)
{
    /* Needs optimize("O0") attribute otherwise this condition is dropped */
    if (pathname == NULL)
    {
        errno = ENOENT;
        return -1;
    }

    if (!is_init)
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
    if (!is_init)
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
            enable_mknod_create &&
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
            enable_mknod_create &&
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
    else if (enable_bfs_opendir &&
             ime_client_native2_is_fuse_path_and_convert(name, tmp))
    {
        char bfs_path[PATH_MAX];
        DIR *d;
        strcpy(bfs_path, client_bfs_path);
        strcat(bfs_path, tmp);
        d = real_opendir(bfs_path);

        /* Extend size of DIR buffer to improve readdir efficiency */
        if (d != NULL && enable_large_dir_buffer)
        {
            DIR *new_d = calloc(1, sizeof(DIR) + OPENDIR_BUFFER_SZ);
            if (new_d != NULL)
            {
                memcpy(new_d, d, sizeof(DIR));
                new_d->allocation = OPENDIR_BUFFER_SZ;
                free(d);
                d = new_d;
            }
        }

        return d;
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

static void ime_env_init(void)
{
    /* Disable IME min connections if no env. variable set */
    int ret = setenv(MIN_CONNECTIONS_ENV , "0", 0);
    if (ret != 0)
        fprintf(stderr, "Unable to disable IME min connections: %s\n",
                strerror(errno));
}

int __libc_start_main(int (*main) (int,char **,char **),
              int argc,char **ubp_av,
              void (*init) (void),
              void (*fini)(void),
              void (*rtld_fini)(void),
              void (*stack_end))
{
    /* Check if some operations should be redirected to the Backing File System */
    char *env_tmp = getenv(BFS_PATH_ENV);
    if (env_tmp != NULL)
    {
        strcpy(client_bfs_path, env_tmp);
        enable_client_bfs = true;
    }

    /* Check if opendir should be redirected to the BFS */
    env_tmp = getenv(NO_BFS_OPENDIR_ENV);
    if (enable_client_bfs && (env_tmp == NULL))
        enable_bfs_opendir = true;

    /* Check if opendir should allocate larger buffer */
    env_tmp = getenv(NO_LARGE_DIR_ENV);
    if (enable_bfs_opendir && (env_tmp == NULL))
        enable_large_dir_buffer = true;

    /* Check if open O_CREAT should be converted into mknod in the Backing File
     * System followed by an IME native open (without O_CREAT flag). */
    env_tmp = getenv(NO_MKNOD_CREATE_ENV);
    if (enable_client_bfs && (env_tmp == NULL))
        enable_mknod_create = true;

    printf("POSIX 2 IME Library Loaded (opendir to BFS: %s, "
           "large dir buffer: %s, mknod create: %s)\n",
           enable_bfs_opendir      ? "on" : "off",
           enable_large_dir_buffer ? "on" : "off",
           enable_mknod_create     ? "on" : "off");

    init_real();

    ime_env_init();
    ime_native_init();

    is_init = true;

    return real_libc_start_main(main, argc, ubp_av,
                                init, fini, rtld_fini, stack_end);
}

