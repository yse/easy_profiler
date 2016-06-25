#include "profiler/reader.h"
#include <fstream>
#include <iterator>
#include <algorithm> 

extern "C"{
	int fillTreesFromFile(const char* filename, thread_blocks_tree_t& threaded_trees)
	{
		std::ifstream inFile(filename, std::fstream::binary);

		if (!inFile.is_open()){
			return -1;
		}

		int blocks_counter = 0;

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

			BlocksTree& root = threaded_trees[baseData->getThreadId()];

			BlocksTree tree;
			tree.node = new profiler::SerilizedBlock(sz, data);
			blocks_counter++;

			if (root.children.empty()){
				root.children.push_back(std::move(tree));
			}
			else{
				BlocksTree& back = root.children.back();
				auto t1 = back.node->block()->getEnd();
				auto mt0 = tree.node->block()->getBegin();
				if (mt0 < t1)//parent - starts earlier than last ends
				{
					auto lower = std::lower_bound(root.children.begin(), root.children.end(), tree);

					std::move(lower, root.children.end(), std::back_inserter(tree.children));

					root.children.erase(lower, root.children.end());

				}

				root.children.push_back(std::move(tree));
			}


			//delete[] data;

		}
		return blocks_counter;
	}
}