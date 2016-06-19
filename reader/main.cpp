#include "profiler/profiler.h"
#include "profiler/reader.h"
#include <fstream>
#include <list>
#include <iostream>
#include <map>
#include <stack>
#include <vector>
#include <iterator>
#include <algorithm> 
#include <ctime>
#include <chrono>
#include <iostream>
#include <string>

void printTree(const BlocksTree& tree, int level = 0)
{
	if (tree.node){
		std::cout << std::string(level, '\t') << " "<< tree.node->getBlockName() << std::endl;
	} 

	for (const auto& i : tree.children){
		printTree(i,level+1);
	}
}

int main()
{
	std::ifstream inFile("test.prof", std::fstream::binary);

	if (!inFile.is_open()){
		return -1;
	}

	thread_blocks_tree_t threaded_trees;

	auto start = std::chrono::system_clock::now();

	int blocks_counter = fillTreesFromFile("test.prof", threaded_trees);

	auto end = std::chrono::system_clock::now();

	std::cout << "Blocks count: " << blocks_counter << std::endl;
	std::cout << "dT =  " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " usec" << std::endl;
	for (const auto & i : threaded_trees){
	//	printTree(i.second,-1);
	}
	return 0;
}
