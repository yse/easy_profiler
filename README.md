# easy_profiler [![License](https://img.shields.io/badge/license-GPL3-blue.svg)](https://github.com/yse/easy_profiler/blob/develop/COPYING)
Lightweight profiler library for c++ 

You can profile any function in you code. Furthermore this library provide profiling of any block of code.

Example of usage.

This code snippet will generate block with function name and grouped it in Magenta group:
```cpp
void frame(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Magenta);
	prepareRender();
	calculatePhysics();
}
```
To profile any block you may do this as following:
```cpp
void frame(){
	//some code
	PROFILER_BEGIN_BLOCK("Calculating summ");
	for(int i = 0; i < 10; i++){
		sum += i;
	}
	PROFILER_END_BLOCK;
}
```
