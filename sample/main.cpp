#include "profiler/profiler.h"
#include <chrono>
#include <thread>
#include <vector>
#include <iostream>

void loadingResources(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Lightcyan);
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
}



void prepareMath(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

void calcIntersect(){
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
    std::this_thread::sleep_for(std::chrono::microseconds(700));
}

void calcPhys(){
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
    calcIntersect();
}

void calcBrain(){
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
    std::this_thread::sleep_for(std::chrono::microseconds(300));
}

void calculateBehavior(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Lightblue);
	std::this_thread::sleep_for(std::chrono::milliseconds(3));
    calcPhys();
    calcBrain();
}

void modellingStep(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Navy);
	prepareMath();
	calculateBehavior();
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

void prepareRender(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Lightred);
	std::this_thread::sleep_for(std::chrono::milliseconds(5));

}

void calculatePhysics(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
	std::this_thread::sleep_for(std::chrono::milliseconds(7));
}

void frame(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Magenta);
	prepareRender();
	calculatePhysics();
	std::this_thread::sleep_for(std::chrono::milliseconds(4));
}

void loadingResourcesThread(){
	for(int i = 0; i < 10; i++){
		loadingResources();
		PROFILER_ADD_EVENT_GROUPED("Resources Loading!",profiler::colors::Cyan);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

void modellingThread(){
	for(int i = 0; i < 160; i++){
		modellingStep();
	}
}

void renderThread(){
	for(int i = 0; i < 100; i++){
		frame();
	}
}
int main()
{
	auto start = std::chrono::system_clock::now();
	PROFILER_ENABLE;

	std::thread render = std::thread(renderThread);
	std::thread modelling = std::thread(modellingThread);

	std::vector<std::thread> threads;
	for(int i=0; i < 3; i++){
		threads.emplace_back(std::thread(loadingResourcesThread));
	}

	render.join();
	modelling.join();
	for(auto& t : threads){
		t.join();
	}
	auto end = std::chrono::system_clock::now();
	auto elapsed =
			std::chrono::duration_cast<std::chrono::microseconds>(end - start);

	std::cout << elapsed.count() << std::endl;
	//block count ~810
	return 0;
}
