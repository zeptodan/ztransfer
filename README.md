# ztransfer

`ztransfer` is a terminal-based LAN file transfer tool written in C.
It provides a text user interface using **ncurses (Linux)** and **PDCurses (Windows)** and allows machines on the same network to discover each other and transfer files over TCP.

## Features

* LAN peer discovery using UDP broadcast
* TCP file transfer
* Terminal UI with progress bars
* Works on **Linux and Windows**
* Cross-platform build system using **CMake**

---

# Requirements

## Linux

* gcc or clang
* cmake
* ncurses development package


## Windows

The project uses **PDCurses compiled with MinGW**.

Because the included library was compiled with **MinGW**, the project must also be compiled using a **MinGW toolchain**.

Recommended environment:

* **MSYS2**
* MinGW-w64 gcc

Install MSYS2 from:
https://www.msys2.org/

Open the **UCRT64 shell** before building.

---

# Building

Clone the repository:

```
git clone https://github.com/yourusername/ztransfer.git
cd ztransfer
```

Create a build directory:

```
mkdir build
cd build
```

Generate build files:

```
cmake ..
```

Compile:

```
cmake --build .
```

The executable will be generated inside the `build` directory.

---

# Running

Run the program from the build directory:

Linux

```
./ztransfer
```

Windows

```
ztransfer.exe
```

---

# Notes

* Windows builds require **MinGW** because the included **PDCurses library was compiled with MinGW**.
* If using another compiler (such as MSVC), PDCurses must be rebuilt using that compiler.
* The program is intended for **local network file transfers**.

---

# License

MIT License
