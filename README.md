---
title: COMP 3430 Operating Systems
subtitle: "Assignment 2: shell!"
date: Winter 2025
---

Overview
========

This directory contains the following:

* This `README.md` file (you're reading it!).
* A `Makefile` that can build the sample code.
* A generic, POSIX-like interface for opening and reading files in a file system
  (`nqp_io.h`).
* A pre-compiled implementation of this interface (`nqp_exfat.o`).
* A sample exFAT-formatted volume containing some files and programs
  (`root.img`).
* An initial template implementation of a shell that works with the provided
  volume.

Building and running
====================

The only runnable program in this directory is `nqp_shell.c`.

You can compile this program on the command line:

```bash
make
```

You can run this program on the command line by passing the volume that you
would like to have the shell use as a root directory:

```bash
./nqp_shell root.img
```

`nqp_exfat.o`
-------------

This is a pre-compiled implementation of the interface defined in `nqp_io.h`.
The file was compiled on Aviary. This *may* work on other x86_64 Linux
installations (e.g., Windows Subsystem for Linux) but will not work on macOS
(neither Intel nor Apple Silicon).

### macOS and Lima

This *may* work in something like Lima, assuming that you are [running an x86_64
version of Linux within Lima].

This repository also now contains an arm-compiled version of the repository.

To build with the arm version, you'll have to specify a variable when you run
`make`:

```bash
make NQP_EXFAT=nqp_exfat_arm.o
```

[running an x86_64 version of Linux within Lima]: https://github.com/lima-vm/lima/discussions/1797

Using your own implementation
-----------------------------

If you would like to use your own implementation of exFAT for `nqp_io.h`, you
will need to implement one additional function. You are welcome to implement
the function on your own (it's called `nqp_vol_label`); this function should
read the volume label directory entry from the root directory, pass that to
`unicode2ascii`, and then return the string returned by `unicode2ascii`.
