/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev

This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef ___PROFILER____MANAGER____H______
#define ___PROFILER____MANAGER____H______

#include "profiler/profiler.h"

class ProfileManager
{
	ProfileManager();
	ProfileManager(const ProfileManager& p) = delete;
	ProfileManager& operator=(const ProfileManager&) = delete;
    static ProfileManager m_profileManager;

	bool m_isEnabled = false;
public:
    static ProfileManager& instance();

	void registerMark(profiler::Mark* _mark);
	void beginBlock(profiler::Block* _block);
	void endBlock();
	void setEnabled(bool isEnable);
};

#endif
