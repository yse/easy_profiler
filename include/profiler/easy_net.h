#ifndef EASY_NET_H
#define EASY_NET_H

#include <stdint.h>

namespace profiler {
namespace net {

const char* DAFAULT_ADDRESS = "tcp://127.0.0.1:28077";
const uint32_t EASY_MESSAGE_SIGN = 20160909;

#pragma pack(push,1)

enum MessageType : uint8_t
{
    MESSAGE_TYPE_ZERO,

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

struct DataMessage : public Message
{
    uint32_t size = 0;//bytes

    DataMessage(MessageType _t = MESSAGE_TYPE_REPLY_BLOCKS) :
        Message(_t)
    {}

    DataMessage(uint32_t _s, MessageType _t = MESSAGE_TYPE_REPLY_BLOCKS) :
        Message(_t)
      , size(_s)
    {}

    const char* data() const
    {
        return reinterpret_cast<const char*>(this) + sizeof(DataMessage);
    }
};

struct BlockStatusMessage : public Message
{
    uint32_t    id;
    uint8_t status;

    BlockStatusMessage(uint32_t _id, uint8_t _status)
        : Message(MESSAGE_TYPE_EDIT_BLOCK_STATUS)
        , id(_id)
        , status(_status)
    {
    }

private:

    BlockStatusMessage() = delete;
};

#pragma pack(pop)

}//net

}//profiler

#endif // EASY_NET_H
