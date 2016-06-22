//#define FULL_DISABLE_PROFILER
#include "profiler/profiler.h"
#include <chrono>
#include <thread>
#include <vector>
#include <iostream>
#include <condition_variable>
#include "profiler/reader.h"

std::condition_variable cv;
std::mutex cv_m;
int g_i = 0;

void loadingResources(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Lightcyan);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}



void prepareMath(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
	//std::this_thread::sleep_for(std::chrono::milliseconds(3));
}

void calcIntersect(){
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
	//std::this_thread::sleep_for(std::chrono::milliseconds(4));
}

void calcPhys(){
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
    calcIntersect();
}

void calcBrain(){
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
	//std::this_thread::sleep_for(std::chrono::milliseconds(3));
}

void calculateBehavior(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Lightblue);
    calcPhys();
    calcBrain();
}

void modellingStep(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Navy);
	prepareMath();
	calculateBehavior();
}

void prepareRender(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Lightred);
	//std::this_thread::sleep_for(std::chrono::milliseconds(8));

}

void calculatePhysics(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
	//std::this_thread::sleep_for(std::chrono::milliseconds(8));
}

void frame(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Magenta);
	prepareRender();
	calculatePhysics();
}

void loadingResourcesThread(){
	std::unique_lock<std::mutex> lk(cv_m);
	cv.wait(lk, []{return g_i == 1; });
	for(int i = 0; i < 10; i++){
		loadingResources();
		PROFILER_ADD_EVENT_GROUPED("Resources Loading!",profiler::colors::Cyan);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

void modellingThread(){
	std::unique_lock<std::mutex> lk(cv_m);
	cv.wait(lk, []{return g_i == 1; });
	for (int i = 0; i < 1600; i++){
		modellingStep();
	}
}

void renderThread(){
	std::unique_lock<std::mutex> lk(cv_m);
	cv.wait(lk, []{return g_i == 1; });
	for (int i = 0; i < 1000; i++){
		frame();
	}
}

void four()
{
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
	std::this_thread::sleep_for(std::chrono::milliseconds(37));
}

void five()
{
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}
void six()
{
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
	std::this_thread::sleep_for(std::chrono::milliseconds(42));
}

void three()
{
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
	four();
	five();
	six();
}

void seven()
{
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
	std::this_thread::sleep_for(std::chrono::milliseconds(147));
}

void two()
{
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
	std::this_thread::sleep_for(std::chrono::milliseconds(26));
}

void one()
{
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
	two();
	three();
	seven();
}

/*
one
	two
	three
		four
		five
		six
	seven

*/
int main()
{
	auto start = std::chrono::system_clock::now();
	PROFILER_ENABLE;

	//one();
	//one();
	/**/
	std::vector<std::thread> threads;

	std::thread render = std::thread(renderThread);
	std::thread modelling = std::thread(modellingThread);

	
	for(int i=0; i < 3; i++){
		threads.emplace_back(std::thread(loadingResourcesThread));
		threads.emplace_back(std::thread(renderThread));
		threads.emplace_back(std::thread(modellingThread));
	}
	{
		std::lock_guard<std::mutex> lk(cv_m);
		g_i = 1;
	}
	cv.notify_all();

	render.join();
	modelling.join();
	for(auto& t : threads){
		t.join();
	}
	/**/

	auto end = std::chrono::system_clock::now();
	auto elapsed =
			std::chrono::duration_cast<std::chrono::microseconds>(end - start);

	std::cout << elapsed.count() << " usec" << std::endl;

	thread_blocks_tree_t threaded_trees;
	int blocks_counter = fillTreesFromFile("test.prof", threaded_trees);
	std::cout << "Blocks count: " << blocks_counter << std::endl;

	return 0;
}
