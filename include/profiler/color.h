#ifndef ____PROFILER_____COLOR_____H_____
#define ____PROFILER_____COLOR_____H_____

#include <stdint.h>

namespace profiler{
	typedef uint16_t color_t; //16-bit RGB format (5-6-5)
	namespace colors{

		const color_t Red = 0xF800;
		const color_t Green = 0x07E0;
		const color_t Blue = 0x001F;

		const color_t White = 0xFFFF;
		const color_t Black = 0x0000;

	}
}

#endif