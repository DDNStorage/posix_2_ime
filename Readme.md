# POSIX to IME  library

This project transparently translates POSIX calls into IME native
calls.

## Preparation

The program interfaces directly with IME libraries.
To generate the wrapper, run

    $ make

## Usage

To use the wrapper you need to set the LD_PRELOAD environnement variable.
To set it globally:

    $ export LD_PRELOAD=<absolute_path_to_libposix2ime.so>

One time usage with ior:

    $ LD_PRELOAD=<absolute_path_to_libposix2ime.so> ior

In addition, the IM_CLIENT_BFS_PATH environnement variable can be set to
contain the Backing File System path used by IME (if mounted on
the compute nodes). opeddir calls will then use tat path to speed up calls
such as readdir.
