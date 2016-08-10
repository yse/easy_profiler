//#define FULL_DISABLE_PROFILER
#include "profiler/profiler.h"
#include <chrono>
#include <thread>
#include <vector>
#include <iostream>
#include <condition_variable>
#include "profiler/reader.h"
#include <cstdlib>
#include <math.h>

std::condition_variable cv;
std::mutex cv_m;
int g_i = 0;

int OBJECTS = 9000;
int RENDER_SPEPS = 1600;
int MODELLING_STEPS = 1000;
int RESOURCE_LOADING_COUNT = 50;

void localSleep(int magic=200000)
{
    //PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
    volatile int i = 0;
    for (; i < magic; ++i);
}

void loadingResources(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Darkcyan);
    localSleep();
//	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void prepareMath(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
    int* intarray = new int[OBJECTS];
    for (int i = 0; i < OBJECTS; ++i)
        intarray[i] = i * i;
    delete[] intarray;
	//std::this_thread::sleep_for(std::chrono::milliseconds(3));
}

void calcIntersect(){
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
    //int* intarray = new int[OBJECTS * OBJECTS];
    int* intarray = new int[OBJECTS];
    for (int i = 0; i < OBJECTS; ++i)
    {
        for (int j = i; j < OBJECTS; ++j)
            //intarray[i * OBJECTS + j] = i * j - i / 2 + (OBJECTS - j) * 5;
            intarray[j] = i * j - i / 2 + (OBJECTS - j) * 5;
    }
    delete[] intarray;
	//std::this_thread::sleep_for(std::chrono::milliseconds(4));
}

double multModel(double i)
{
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
    return i * sin(i) * cos(i);
}

void calcPhys(){
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
    double* intarray = new double[OBJECTS];
    for (int i = 0; i < OBJECTS; ++i)
        intarray[i] = multModel(double(i)) + double(i / 3) - double((OBJECTS - i) / 2);
    calcIntersect();
    delete[] intarray;
}

double calcSubbrain(int i)
{
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
    return i * i * i - i / 10 + (OBJECTS - i) * 7 ;
}

void calcBrain(){
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Blue);
    double* intarray = new double[OBJECTS];
    for (int i = 0; i < OBJECTS; ++i)
        intarray[i] = calcSubbrain(double(i)) + double(i * 180 / 3);
    delete[] intarray;
	//std::this_thread::sleep_for(std::chrono::milliseconds(3));
}

void calculateBehavior(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Darkblue);
    calcPhys();
    calcBrain();
}

void modellingStep(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Navy);
	prepareMath();
	calculateBehavior();
}

void prepareRender(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Darkred);
    localSleep();
	//std::this_thread::sleep_for(std::chrono::milliseconds(8));

}

int multPhys(int i)
{
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
    return i * i * i * i / 100;
}

int calcPhysicForObject(int i)
{
    PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
    return  multPhys(i) + i / 3 - (OBJECTS - i) * 15;
}

void calculatePhysics(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
    unsigned int* intarray = new unsigned int[OBJECTS];
    for (int i = 0; i < OBJECTS; ++i)
        intarray[i] = calcPhysicForObject(i);
    delete[] intarray;
	//std::this_thread::sleep_for(std::chrono::milliseconds(8));
}

void frame(){
	PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Magenta);
	prepareRender();
	calculatePhysics();
}

void loadingResourcesThread(){
	//std::unique_lock<std::mutex> lk(cv_m);
	//cv.wait(lk, []{return g_i == 1; });
	PROFILER_SET_THREAD_NAME("Resource loading")
	for(int i = 0; i < RESOURCE_LOADING_COUNT; i++){
		loadingResources();
		PROFILER_ADD_EVENT_GROUPED("Resources Loading!",profiler::colors::Cyan);
        //std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

void modellingThread(){
	//std::unique_lock<std::mutex> lk(cv_m);
	//cv.wait(lk, []{return g_i == 1; });
	PROFILER_SET_THREAD_NAME("Modelling")
		for (int i = 0; i < RENDER_SPEPS; i++){
		modellingStep();
        localSleep(300000);
        //std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

void renderThread(){
	//std::unique_lock<std::mutex> lk(cv_m);
	//cv.wait(lk, []{return g_i == 1; });
	PROFILER_SET_THREAD_NAME("Render")
	for (int i = 0; i < MODELLING_STEPS; i++){
		frame();
        localSleep(300000);
        //std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

int main(int argc, char* argv[])
{
	if (argc > 1 && argv[1]){
		OBJECTS = std::atoi(argv[1]);
	}
	if (argc > 2 && argv[2]){
		RENDER_SPEPS = std::atoi(argv[2]);
	}
	if (argc > 3 && argv[3]){
		MODELLING_STEPS = std::atoi(argv[3]);
	}
	if (argc > 4 && argv[4]){
		RESOURCE_LOADING_COUNT = std::atoi(argv[4]);
	}

	std::cout << "Objects count: " << OBJECTS << std::endl;
	std::cout << "Render steps: " << RENDER_SPEPS << std::endl;
	std::cout << "Modelling steps: " << MODELLING_STEPS << std::endl;
	std::cout << "Resource loading count: " << RESOURCE_LOADING_COUNT << std::endl;

	auto start = std::chrono::system_clock::now();
	PROFILER_ENABLE;
	PROFILER_SET_MAIN_THREAD;
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

	std::cout << "Elapsed time: " << elapsed.count() << " usec" << std::endl;

	auto blocks_count = profiler::dumpBlocksToFile("test.prof");

	std::cout << "Blocks count: " << blocks_count << std::endl;

	return 0;
}
