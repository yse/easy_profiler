# easy_profiler [![License](https://img.shields.io/badge/license-GPL3-blue.svg)](https://github.com/yse/easy_profiler/blob/develop/COPYING)[![Build Status](https://travis-ci.org/yse/easy_profiler.svg?branch=develop)](https://travis-ci.org/yse/easy_profiler)

1. [About](#about)
2. [Usage](#usage)
3. [Build](#build)
    - [Linux](#linux)
    - [Windows](#windows)

# About
Lightweight cross-platform profiler library for c++

You can profile any function in you code. Furthermore this library provide measuring time of any block of code.
For example, information for 12 millions of blocks is using less than 300Mb of memory.
Working profiler slows your application execution for only 1-2%.

Disabled profiler will not affect your application execution in any way. You can leave it in your Release build
and enable it in run-time at any moment during application launch to see what is happening at the moment.

Also the library can capture system's context switch events between threads. Context switch information includes
duration, target thread id, thread owner process id, thread owner process name.

You can see the results of measuring in simple GUI application which provides full statistics and renders beautiful time-line.

![GUI screenshot](https://lh6.googleusercontent.com/QREJu1XjJVoyGQ3oYjvrwwfEzcHUQkSbcx9vQvo_Zzmpoya5sfIbCroU0dOUl3QXQhcdl98Y=w1521-h697)

# Usage

First of all you can specify path to include directory which contains `include/profiler` directory and define macro `BUILD_WITH_EASY_PROFILER`.
For linking with easy_profiler you can specify path to library.

Example of usage.

This code snippet will generate block with function name and Magenta color:
```cpp
#include <easy/profiler.h>

void frame() {
    EASY_FUNCTION(profiler::colors::Magenta); // Magenta block with name "frame"
    prepareRender();
    calculatePhysics();
}
```

To profile any block you may do this as following.
You can specify these blocks also with Google material design colors or just set name of the block
(in this case it will have default color which is `Amber100`):
```cpp
#include <easy/profiler.h>

void foo() {
    // some code
    EASY_BLOCK("Calculating sum"); // Block with default color
    int sum = 0;
    for (int i = 0; i < 10; ++i) {
        EASY_BLOCK("Addition", profiler::colors::Red); // Scoped red block (no EASY_END_BLOCK needed)
        sum += i;
    }
    EASY_END_BLOCK; // This ends "Calculating sum" block

    EASY_BLOCK("Calculating multiplication", profiler::colors::Blue500); // Blue block
    int mul = 1;
    for (int i = 1; i < 11; ++i)
        mul *= i;
    //EASY_END_BLOCK; // This is not needed because all blocks are ended on destructor when closing braces met
}
```

You can also use your own colors. easy_profiler is using standard 32-bit ARGB color format.
Example:
```cpp
#include <easy/profiler.h>

void bar() {
    EASY_FUNCTION(0xfff080aa); // Function block with custom color
    // some code
}
```

# Build

## Prerequisites

For core:
* compiler with c++11 support
* cmake 3.0 or later

For GUI:
* Qt 5.3.0 or later

## Linux

```bash
$ mkdir build
$ cd build
$ cmake ..
$ make
```

## Windows

If you are using QtCreator IDE you can just open `CMakeLists.txt` file in root directory.
If you are using Visual Studio you can generate solution by cmake generator command.

### Way 1
Specify path to cmake scripts in Qt5 dir (usually in lib/cmake subdir) and execute cmake generator command,
for example:
```batch
$ mkdir build
$ cd build
$ cmake -DCMAKE_PREFIX_PATH="C:\Qt\5.3\msvc2013_64\lib\cmake" .. -G "Visual Studio 12 2013 Win64"
```

### Way 2
Create system variable "Qt5Widgets_DIR" and set it's value to "[path-to-Qt5-binaries]\lib\cmake\Qt5Widgets".
For example, "C:\Qt\5.3\msvc2013_64\lib\cmake\Qt5Widgets".
And then run cmake generator as follows:
```batch
$ mkdir build
$ cd build
$ cmake .. -G "Visual Studio 12 2013 Win64"
```



[![Analytics](https://ga-beacon.appspot.com/UA-82899176-1/easy_profiler/readme)](https://github.com/yse/easy_profiler)
