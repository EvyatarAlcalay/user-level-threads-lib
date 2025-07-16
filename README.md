# 🧵 User-Level Threads Library

A lightweight user-level threading library implemented in C++, featuring preemptive round-robin scheduling using POSIX signals and virtual timers.

## 📌 Features

- Preemptive scheduling using `SIGVTALRM` and `setitimer`
- Round-Robin thread management
- Full user-level thread lifecycle: `spawn`, `terminate`, `block`, `resume`
- Thread context switching using `sigsetjmp` / `siglongjmp`
- Written in clean and modular C++ code

## 🛠️ Build Instructions

```bash
make
```

This will generate a static library called `libuthreads.a`.

## 📄 Files

- `uthreads.cpp` – Implementation of the thread library
- `uthreads.h` – Header file for public API (not included here, provided by course)
- `libraries.h` – Utility includes and helpers
- `Makefile` – Build configuration

## 🧪 Usage

To use the library, include `uthreads.h` in your C++ program and link against the compiled `libuthreads.a`. The API allows creation, blocking, resuming, and termination of user-level threads with precise scheduling behavior.

## 🧠 Learning Goals

- Understand low-level context switching and scheduling mechanisms
- Gain hands-on experience with UNIX signal handling and virtual timers
- Develop robust system-level code in C++
- Deepen knowledge of how user-level threads differ from kernel threads

## 📚 Background

Developed as part of an Operating Systems course at the Hebrew University of Jerusalem.

