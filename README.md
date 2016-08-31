# easy_profiler [![License](https://img.shields.io/badge/license-GPL3-blue.svg)](https://github.com/yse/easy_profiler/blob/develop/COPYING)[![Build Status](https://travis-ci.org/yse/easy_profiler.svg?branch=develop)](https://travis-ci.org/yse/easy_profiler)

1. [About](#about)
2. [Build](#build)
    - [Linux](#linux)
    - [Windows](#windows)
3. [Usage](#usage)


# About
Lightweight profiler library for c++ 

You can profile any function in you code. Furthermore this library provide profiling of any block of code.

# Build

## Prerequisites

For core:
* compiler with c++11 support
* cmake 3.0 or later

For GUI:
* Qt 5.3.0 or later

## linux

```bash
$ mkdir build
$ cd build
$ cmake ..
$ make
```

## windows

If you use qtcreator IDE you can just open `CMakeLists.txt` file in root directory.

If you use Visual Studio you can generate solution by cmake command. In this case you should specify path to cmake scripts in Qt5 dir (usually in lib/cmake subdir), for example:
```batch
$ mkdir build
$ cd build
$ cmake -DCMAKE_PREFIX_PATH="C:\\\\Qt\\\\5.3\\\\msvc2013_64\\\\lib\\\\cmake" ..
```

# Usage

First of all you can specify path to include directory which contains `include/profiler` directory. For linking with ease_profiler you can specify path to library.

Example of usage.

This code snippet will generate block with function name and grouped it in Magenta group:
```cpp
#include <profiler/profiler.h>

void frame(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Magenta);
	prepareRender();
	calculatePhysics();
}
```
To profile any block you may do this as following:
```cpp
#include <profiler/profiler.h>

void frame(){
	//some code
	PROFILER_BEGIN_BLOCK("Calculating summ");
	for(int i = 0; i < 10; i++){
		sum += i;
	}
	PROFILER_END_BLOCK;
}
```
[![Analytics](https://ga-beacon.appspot.com/UA-82899176-1/easy_profiler/readme)](https://github.com/yse/easy_profiler)
