#include "profile_manager.h"
#include "profiler/profiler.h"

using namespace profiler;

ProfileManager*  ProfileManager::m_profileManager = nullptr;

extern "C"{

	void PROFILER_API registerMark(Mark* _mark)
	{
		ProfileManager::instance()->registerMark(_mark);
	}

	void PROFILER_API endBlock()
	{
		ProfileManager::instance()->endBlock();
	}

	void PROFILER_API setEnabled(bool isEnable)
	{
		ProfileManager::instance()->setEnabled(isEnable);
	}
}


ProfileManager::ProfileManager()
{

}

ProfileManager* ProfileManager::instance()
{
	if (!m_profileManager)
	{
		//TODO: thread safety for profiler::instance
		//if(!m_profiler)//see paper by Scott Mayers and Alecsandrescu: http://www.aristeia.com/Papers/DDJ_Jul_Aug_2004_revised.pdf
		m_profileManager = new ProfileManager();
	}
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