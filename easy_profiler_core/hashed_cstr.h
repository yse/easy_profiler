/************************************************************************
* file name         : hashed_str.h
* ----------------- :
* creation time     : 2016/09/11
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains definition of C-strings with calculated hash-code.
*                   : These strings may be used as optimized keys for std::unordered_map.
* ----------------- :
* change log        : * 2016/09/11 Victor Zarubkin: Initial commit. Moved sources from reader.cpp
*                   : 
*                   : *
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : This program is free software : you can redistribute it and / or modify
*                   : it under the terms of the GNU General Public License as published by
*                   : the Free Software Foundation, either version 3 of the License, or
*                   : (at your option) any later version.
*                   :
*                   : This program is distributed in the hope that it will be useful,
*                   : but WITHOUT ANY WARRANTY; without even the implied warranty of
*                   : MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
*                   : GNU General Public License for more details.
*                   :
*                   : You should have received a copy of the GNU General Public License
*                   : along with this program.If not, see <http://www.gnu.org/licenses/>.
************************************************************************/

#ifndef EASY_PROFILER__HASHED_CSTR__H_
#define EASY_PROFILER__HASHED_CSTR__H_

#include <functional>
#include <string.h>
#include <string>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)// && _MSC_VER >= 1800
# define EASY_PROFILER_HASHED_CSTR_DEFINED

namespace profiler {

    /** \brief Simple C-string pointer with length.

    It is used as base class for a key in std::unordered_map.
    It is used to get better performance than std::string.
    It simply stores a pointer and a length, there is no
    any memory allocation and copy.

    \warning Make sure you know what you are doing. You have to be sure that
    pointed C-string will exist until you finish using this cstring.

    \ingroup profiler
    */
    class cstring
    {
    protected:

        const char* m_str;
        size_t      m_len;

    public:

        cstring(const char* _str) : m_str(_str), m_len(strlen(_str))
        {
        }

        cstring(const char* _str, size_t _len) : m_str(_str), m_len(_len)
        {
        }

        cstring(const cstring&) = default;
        cstring& operator = (const cstring&) = default;

        inline bool operator == (const cstring& _other) const
        {
            return m_len == _other.m_len && !strncmp(m_str, _other.m_str, m_len);
        }

        inline bool operator != (const cstring& _other) const
        {
            return !operator == (_other);
        }

        inline bool operator < (const cstring& _other) const
        {
            if (m_len == _other.m_len)
            {
                return strncmp(m_str, _other.m_str, m_len) < 0;
            }

            return m_len < _other.m_len;
        }

        inline const char* c_str() const
        {
            return m_str;
        }

        inline size_t size() const
        {
            return m_len;
        }

    }; // END of class cstring.

    /** \brief cstring with precalculated hash.

    This is used to calculate hash for C-string and to cache it
    to be used in the future without recurring hash calculatoin.

    \note This class is used as a key in std::unordered_map.

    \ingroup profiler
    */
    class hashed_cstr : public cstring
    {
        typedef cstring Parent;

        size_t m_hash;

    public:

        hashed_cstr(const char* _str) : Parent(_str), m_hash(0)
        {
            m_hash = ::std::_Hash_seq((const unsigned char *)m_str, m_len);
        }

        hashed_cstr(const char* _str, size_t _hash_code) : Parent(_str), m_hash(_hash_code)
        {
        }

        hashed_cstr(const char* _str, size_t _len, size_t _hash_code) : Parent(_str, _len), m_hash(_hash_code)
        {
        }

        hashed_cstr(const hashed_cstr&) = default;
        hashed_cstr& operator = (const hashed_cstr&) = default;

        inline bool operator == (const hashed_cstr& _other) const
        {
            return m_hash == _other.m_hash && Parent::operator == (_other);
        }

        inline bool operator != (const hashed_cstr& _other) const
        {
            return !operator == (_other);
        }

        inline size_t hcode() const
        {
            return m_hash;
        }

    }; // END of class hashed_cstr.

} // END of namespace profiler.

namespace std {

    /** \brief Simply returns precalculated hash of a C-string. */
    template <> struct hash<::profiler::hashed_cstr> {
        typedef ::profiler::hashed_cstr argument_type;
        typedef size_t                    result_type;
        inline size_t operator () (const ::profiler::hashed_cstr& _str) const {
            return _str.hcode();
        }
    };

} // END of namespace std.

#else ////////////////////////////////////////////////////////////////////

// TODO: Create hashed_cstr for Linux (need to use Linux version of std::_Hash_seq)

#endif

namespace profiler {

    class hashed_stdstring
    {
        ::std::string m_str;
        size_t       m_hash;

    public:

        hashed_stdstring(const char* _str) : m_str(_str), m_hash(::std::hash<::std::string>()(m_str))
        {
        }

        hashed_stdstring(const ::std::string& _str) : m_str(_str), m_hash(::std::hash<::std::string>()(m_str))
        {
        }

        hashed_stdstring(::std::string&& _str) : m_str(::std::forward<::std::string&&>(_str)), m_hash(::std::hash<::std::string>()(m_str))
        {
        }

        hashed_stdstring(hashed_stdstring&& _other) : m_str(::std::move(_other.m_str)), m_hash(_other.m_hash)
        {
        }

        hashed_stdstring(const char* _str, size_t _hash_code) : m_str(_str), m_hash(_hash_code)
        {
        }

        hashed_stdstring(const ::std::string& _str, size_t _hash_code) : m_str(_str), m_hash(_hash_code)
        {
        }

        hashed_stdstring(::std::string&& _str, size_t _hash_code) : m_str(::std::forward<::std::string&&>(_str)), m_hash(_hash_code)
        {
        }

        hashed_stdstring(const hashed_stdstring&) = default;
        hashed_stdstring& operator = (const hashed_stdstring&) = default;

        hashed_stdstring& operator = (hashed_stdstring&& _other)
        {
            m_str = ::std::move(_other.m_str);
            m_hash = _other.m_hash;
            return *this;
        }

        inline bool operator == (const hashed_stdstring& _other) const
        {
            return m_hash == _other.m_hash && m_str == _other.m_str;
        }

        inline bool operator != (const hashed_stdstring& _other) const
        {
            return !operator == (_other);
        }

        inline size_t hcode() const
        {
            return m_hash;
        }

        inline const char* c_str() const
        {
            return m_str.c_str();
        }

        inline size_t size() const
        {
            return m_str.size();
        }

    }; // END of class hashed_stdstring.

} // END of namespace profiler.

namespace std {

    /** \brief Simply returns precalculated hash of a std::string. */
    template <> struct hash<::profiler::hashed_stdstring> {
        typedef ::profiler::hashed_stdstring argument_type;
        typedef size_t                         result_type;
        inline size_t operator () (const ::profiler::hashed_stdstring& _str) const {
            return _str.hcode();
        }
    };

} // END of namespace std.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__HASHED_CSTR__H_
