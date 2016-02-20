#include "profile_manager.h"

#include <thread>

using namespace profiler;

extern "C"{

    void PROFILER_API endBlock()
    {
        ProfileManager::instance().endBlock();
    }

    void PROFILER_API setEnabled(bool isEnable)
    {
        ProfileManager::instance().setEnabled(isEnable);
    }
	void PROFILER_API beginBlock(Block* _block)
	{
		ProfileManager::instance().beginBlock(_block);
	}
}

SerilizedBlock::SerilizedBlock(Block* block):
		m_size(0),
		m_data(nullptr)
{
	m_size += sizeof(BaseBlockData);
	auto name_len = strlen(block->getName()) + 1;
	m_size += name_len;

	m_data = new char[m_size];
	memcpy(&m_data[0], block, sizeof(BaseBlockData));
	strncpy(&m_data[sizeof(BaseBlockData)], block->getName(), name_len);

}

SerilizedBlock::~SerilizedBlock()
{
	if (m_data){
		delete[] m_data;
		m_data = nullptr;
	}
}


ProfileManager::ProfileManager()
{

}

ProfileManager::~ProfileManager()
{
	for (auto* b : m_blocks){
		delete b;
	}
}

ProfileManager& ProfileManager::instance()
{
    ///C++11 makes possible to create Singleton without any warry about thread-safeness
    ///http://preshing.com/20130930/double-checked-locking-is-fixed-in-cpp11/
    static ProfileManager m_profileManager;
    return m_profileManager;
}

void ProfileManager::beginBlock(Block* _block)
{
	if (!m_isEnabled)
		return;
	if (_block->getType() != BLOCK_TYPE_MARK){
		guard_lock_t lock(m_spin);
		m_openedBracketsMap[_block->getThreadId()].push(_block);
	}
	else{
		_internalInsertBlock(_block);
	}
	
}

void ProfileManager::endBlock()
{
	if (!m_isEnabled)
		return;

	size_t threadId = std::hash<std::thread::id>()(std::this_thread::get_id());

	guard_lock_t lock(m_spin);
	auto& stackOfOpenedBlocks = m_openedBracketsMap[threadId];

	if (stackOfOpenedBlocks.empty())
		return;

	Block* lastBlock = stackOfOpenedBlocks.top();

	if (lastBlock && !lastBlock->isFinished()){
		lastBlock->finish();
	}
	_internalInsertBlock(lastBlock);
	stackOfOpenedBlocks.pop();
}

void ProfileManager::setEnabled(bool isEnable)
{
	m_isEnabled = isEnable;
}

void ProfileManager::_internalInsertBlock(profiler::Block* _block)
{
	guard_lock_t lock(m_storedSpin);
	m_blocks.emplace_back(new SerilizedBlock(_block));
}