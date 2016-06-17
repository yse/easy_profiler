#include "profiler/profiler.h"
#include <fstream>
#include <list>
#include <iostream>
#include <map>
#include <stack>
#include <vector>

struct BlocksTree
{
	profiler::SerilizedBlock* node;
	std::vector<BlocksTree> children;
	BlocksTree* parent;
	BlocksTree(){
		node = nullptr;
		parent = nullptr;
	}
	BlocksTree(BlocksTree&& that){

		node = that.node;
		parent = that.parent;

		children = std::move(that.children);

		that.node = nullptr;
		that.parent = nullptr;
	}
	~BlocksTree(){
		if (node){
			delete node;
		}
		node = nullptr;
	}
};

int main()
{
	std::ifstream inFile("test.prof", std::fstream::binary);

	if (!inFile.is_open()){
		return -1;
	}

	//std::list<profiler::SerilizedBlock> blocksList;
	typedef std::map<profiler::timestamp_t, profiler::SerilizedBlock> blocks_map_t;
	typedef std::map<size_t, blocks_map_t> thread_map_t;
	typedef std::map<size_t, BlocksTree> thread_blocks_tree_t;
	thread_map_t blocksList;


	BlocksTree root;
	thread_blocks_tree_t threaded_trees;
	while (!inFile.eof()){
		uint16_t sz = 0;
		inFile.read((char*)&sz, sizeof(sz));
		if (sz == 0)
		{
			inFile.read((char*)&sz, sizeof(sz));
			continue;
		}
		char* data = new char[sz];
		inFile.read((char*)&data[0], sz);
		profiler::BaseBlockData* baseData = (profiler::BaseBlockData*)data;

		blocksList[baseData->getThreadId()].emplace(
			baseData->getBegin(),
			profiler::SerilizedBlock(sz, data)); 
			
		
		BlocksTree tree;
		tree.node = new profiler::SerilizedBlock(sz, data);

		root.children.push_back(std::move(tree));

		BlocksTree currentRoot = threaded_trees[baseData->getThreadId()];

		if (currentRoot.node == nullptr){
			threaded_trees[baseData->getThreadId()] = tree;
		}

		delete[] data;

		//blocksList.emplace_back(sz, data);
	}
	
	return 0;
}
