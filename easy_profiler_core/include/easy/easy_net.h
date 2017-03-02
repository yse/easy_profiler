/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


GNU General Public License Usage
Alternatively, this file may be used under the terms of the GNU
General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef EASY_NET_H
#define EASY_NET_H

#include <stdint.h>

namespace profiler {
namespace net {

#define DAFAULT_ADDRESS "tcp://127.0.0.1:28077"
const uint32_t EASY_MESSAGE_SIGN = 20160909;

#pragma pack(push,1)

enum MessageType : uint8_t
{
    MESSAGE_TYPE_ZERO = 0,

    MESSAGE_TYPE_REQUEST_START_CAPTURE,
    MESSAGE_TYPE_REPLY_START_CAPTURING,
    MESSAGE_TYPE_REQUEST_STOP_CAPTURE,

    MESSAGE_TYPE_REPLY_BLOCKS,
    MESSAGE_TYPE_REPLY_BLOCKS_END,

    MESSAGE_TYPE_ACCEPTED_CONNECTION,

    MESSAGE_TYPE_REQUEST_BLOCKS_DESCRIPTION,
    MESSAGE_TYPE_REPLY_BLOCKS_DESCRIPTION,
    MESSAGE_TYPE_REPLY_BLOCKS_DESCRIPTION_END,

    MESSAGE_TYPE_EDIT_BLOCK_STATUS,

    MESSAGE_TYPE_EVENT_TRACING_STATUS,
    MESSAGE_TYPE_EVENT_TRACING_PRIORITY,
    MESSAGE_TYPE_CHECK_CONNECTION
};

struct Message
{
    uint32_t  magic_number = EASY_MESSAGE_SIGN;
    MessageType type = MESSAGE_TYPE_ZERO;

    bool isEasyNetMessage() const
    {
        return EASY_MESSAGE_SIGN == magic_number;
    }

    Message() = default;
    Message(MessageType _t):type(_t){}
};

struct DataMessage : public Message {
    uint32_t size = 0; // bytes
    DataMessage(MessageType _t = MESSAGE_TYPE_REPLY_BLOCKS) : Message(_t) {}
    DataMessage(uint32_t _s, MessageType _t = MESSAGE_TYPE_REPLY_BLOCKS) : Message(_t), size(_s) {}
    const char* data() const { return reinterpret_cast<const char*>(this) + sizeof(DataMessage); }
};

struct BlockStatusMessage : public Message {
    uint32_t    id;
    uint8_t status;
    BlockStatusMessage(uint32_t _id, uint8_t _status) : Message(MESSAGE_TYPE_EDIT_BLOCK_STATUS), id(_id), status(_status) { }
private:
    BlockStatusMessage() = delete;
};

struct EasyProfilerStatus : public Message
{
    bool         isProfilerEnabled;
    bool     isEventTracingEnabled;
    bool isLowPriorityEventTracing;

    EasyProfilerStatus(bool _enabled, bool _ETenabled, bool _ETlowp)
        : Message(MESSAGE_TYPE_ACCEPTED_CONNECTION)
        , isProfilerEnabled(_enabled)
        , isEventTracingEnabled(_ETenabled)
        , isLowPriorityEventTracing(_ETlowp)
    {
    }

private:

    EasyProfilerStatus() = delete;
};

struct BoolMessage : public Message {
    bool flag = false;
    BoolMessage(MessageType _t, bool _flag = false) : Message(_t), flag(_flag) { }
    BoolMessage() = default;
};

#pragma pack(pop)

}//net

}//profiler

#endif // EASY_NET_H
