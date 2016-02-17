#include "profile_manager.h"
#include "profiler/profiler.h"

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

void ProfileManager::registerMark(profiler::Mark* _mark)
{

}

void ProfileManager::endBlock()
{

}

void ProfileManager::setEnabled(bool isEnable)
{
    m_isEnabled = isEnable;
}
