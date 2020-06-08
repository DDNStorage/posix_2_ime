# POSIX to IME  library

This project transparently translates POSIX calls into native
calls for DDN's _Infinite Memory Engine_ (IME) .


## Preparation

The program interfaces directly with IME libraries.
To generate the wrapper, run

    $ make


## Usage

To use the wrapper you need to set the `LD_PRELOAD` environment variable.

To set it globally:

    $ export LD_PRELOAD=<absolute_path_to_libposix2ime.so>

One time usage with ior:

    $ LD_PRELOAD=<absolute_path_to_libposix2ime.so> ior

With Mvapich or Intel MPI:

    $ mpirun -genv LD_PRELOAD <absolute_path_to_libposix2ime.so>

With OpenMPI:

    $ mpirun -x LD_PRELOAD=<absolute_path_to_libposix2ime.so>


## Advanded Features

In addition, the `IM_CLIENT_BFS_PATH` environment variable can be set to
contain the _Backing File System_ (BFS) path used by IME (if mounted on
the compute nodes). By default, if this environment variable is set:

* `opendir` calls are redirected to the BFS mount point on the compute node (accelerates subsequent calls
such as `readdir`). Export the `IM_CLIENT_NO_BFS_OPENDIR=1` environment variable to disable the feature.
* `open` with `O_CREAT` flag calls are split into a mknod call in the BFS mount point on the compute node followed by an open (without the `O_CREAT` flag) with the IME native interface. This feature improves create IOPs with Lustre. Indeed `mknod` does not allocate an OST object with Lustre. This is not needed as data is stored in IME. Export the `IM_CLIENT_NO_MKNOD_CREATE=1` environment variable to disable the feature.

