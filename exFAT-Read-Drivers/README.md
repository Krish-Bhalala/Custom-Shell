---
title: COMP 3430 Operating Systems
subtitle: "Assignment 1: implementing exFAT"
date: Winter 2025
---

Overview
========

This directory contains the following:

* This `README.md` file (you're reading it!).
* A `Makefile` that can build some sample code.
* A generic, POSIX-like interface for opening and reading files in a file
  system (`exfat_io.h`).
* An example of code that might use this interface (`cat.c`, `ls.c`, and
  `paste.c`).
* An exFAT-specific set of types and values (`exfat_types.h`).

Building and running
====================

There are three sample programs in this repository:

* `cat.c`
* `ls.c`
* `paste.c`

All three of these programs can be compiled on the command line:

```bash
make
```

`cat.c`
-------

The program `cat.c` expects two or more arguments on the command-line

1. The filename of the volume it should read from (when using libc instead this
   argument is ignored).
2. The files that it should print to standard output. The paths to the files
   should be absolute paths within the volume.

For example, to print the `README.md` in `one-file.img` from the [assignment 1
volumes repository], you would run something like:

```bash
./cat one-file.img /README.md
```

This should print out the contents of `README.md` to standard output.

If you want to "print" a non-text file (like a picture in `fragmented.img`v),
you will probably want to redirect standard output to a file, something like:

```bash
./cat fragmented.img /picture.jpg > picture.jpg
```

This will make a new file in the current folder called `picture.jpg` containing
whatever was written to standard output by `cat`.

[assignment 1 volumes repository]: https://code.cs.umanitoba.ca/comp3430-winter2025/assignment1-volumes

### Special options for compiling `cat.c`

While the code for `cat.c` depends on your implementation of `exfat_io.h`, it's
been written in a way that we can replace calls to `exfat_*` with their libc
equivalent (i.e., instead of calling `exfat_read`, we can call `read`).

You can compile and run *just* `cat.c` without having implemented any `exfat_*`
functions using a special flag to `make`:

```bash
make clean # in case you already built it, then
make USE_LIBC_INSTEAD=1
```

If you have compiled `cat` this way, you will need to pass it relative paths
instead of absolute, and the files/folders will have to exist alongside the
program, so for example:

```bash
echo 'hello' > hello.txt
./cat img hello.txt
```

In this case, `cat` will ignore the second argument passed to it and print the
content of `hello.txt` to standard output. 

This mode is only useful to get a sense of what `cat` might produce as output,
but it does not interact with any exFAT code at all.

`ls.c`
------

`ls.c` takes two arguments on the command-line:

1. The filename of the volume it should read from.
2. The directory for which it should print the contents to standard output.

For example, to list the contents of the root directory from the file
`one-file.img`, you would run the following command:

```bash
./ls one-file.img /
```

To list the contents of the `assignment1-template` directory in `normal.img`,
you would run:

```bash
./ls normal.img /assignment1-template # OR
./ls normal.img /assignment1-template/
```

`paste.c`
---------

`paste.c` takes three arguments on the command-line:

1. The filename of the volume it should read from.
2. The two files that it should paste together.

For example, to paste `README.md` beside itself from `one-file.img`, you would
run the following command:

```bash
./paste one-file.img /README.md /README.md
```

To paste together the two files in `two-text-root.img`, you would run the
following command:

```bash
./paste two-file-root.img /iliad.txt /odyssey.txt
```
