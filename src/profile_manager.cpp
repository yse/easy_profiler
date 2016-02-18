#include "profile_manager.h"

#include <thread>

using namespace profiler;

extern "C"{

    void PROFILER_API registerMark(Mark* _mark)
    {
        ProfileManager::instance().registerMark(_mark);
    }

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


ProfileManager::ProfileManager()
{

}

ProfileManager& ProfileManager::instance()
{
    ///C++11 makes possible to create Singleton without any warry about thread-safeness
    ///http://preshing.com/20130930/double-checked-locking-is-fixed-in-cpp11/
    static ProfileManager m_profileManager;
    return m_profileManager;
}

void ProfileManager::registerMark(Mark* _mark)
{
	if (!m_isEnabled)
		return;
}

void ProfileManager::beginBlock(Block* _block)
{
	if (!m_isEnabled)
		return;

	guard_lock_t lock(m_spin);
	m_openedBracketsMap[_block->getThreadId()].push(_block);
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
	stackOfOpenedBlocks.pop();
}

void ProfileManager::setEnabled(bool isEnable)
{
	m_isEnabled = isEnable;
}
