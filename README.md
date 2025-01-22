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
  system (`nqp_io.h`).
* An example of code that might use this interface (`cat.c`).
* An exFAT-specific set of types and values (`nqp_exfat_types.h`).

Building and running
====================

Currently the only runnable program in this directory is `cat.c`. While the code
depends on your implementation of `nqp_io.h`, it's been written in a way that we
can replace calls to `nqp_*` with their libc equivalent (i.e., instead of
calling `nqp_read`, we can call `read`).

You can compile and run the code in its current form with `make`:

```bash
make USE_LIBC_INSTEAD=1
```

You can then run the `cat` program on the command line. It takes two or more
arguments:

1. The filename of the volume it should read from (when using libc instead this
   argument is ignored).
2. The files that it should print to standard output.

```bash
./cat img README.md
```

This should print out the contents of this file (`README.md`) to standard
output.

When you have implemented your code for exFAT, you should build this code with
your own implementations instead, using `make`:

```bash
make
```
