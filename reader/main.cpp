#include "profiler/profiler.h"
#include <fstream>
#include <list>
#include <iostream>

template<class T>
struct tree
{
	typedef tree<T> tree_t;
	tree_t* parent;
};

int main()
{
	std::ifstream inFile("test.prof", std::fstream::binary);

	if (!inFile.is_open()){
		return -1;
	}

	std::list<profiler::SerilizedBlock> blocksList;
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
		blocksList.emplace_back(sz, data);
	}
	for (auto& i : blocksList){
		static auto thread_id = i.block()->thread_id;
		//if (i.block()->thread_id == thread_id)
			std::cout << i.block()->duration() << "\n";
		//std::cout << i.getBlockName() << ":" << (i.block()->end - i.block()->begin)/1000 << " usec." << std::endl;

	}

	return 0;
}
