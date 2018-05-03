// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <vector>
#include <yojimbo.h>
#include "common/common_types.h"
#include "network/packet.h"
#include "network/room.h"

// Common configuration that should be shared between room and member
// As this includes yojimbo, it should not be directly pulled into another header
namespace Network {

// The different types of messages that can be sent. The first byte of each packet defines the type
enum RoomMessageTypes : u8 {
    IdJoinRequest = 1, ///< Client sends to Room to attempt to join
    IdJoinSuccess,     ///< Response from Room if join is successful
    IdRoomInformation, ///< Broadcasted by the Room
    IdSetGameInfo,     ///< Client sends to Room to update currently playing game
    IdWifiPacket,      ///< Game data to be sent to the appropriate clients
    IdJoinFailure,     ///< Response from the room if join is not successful
    IdCloseRoom,       ///< Room is closing (all clients will be disconnected)
    COUNT,
};

template <>
Packet& Packet::operator<<(const GameInfo& g) {
    *this << g.id;
    *this << g.name;
    return *this;
}

template <>
Packet& Packet::operator<<(const Room::Member& m) {
    *this << m.client_id;
    *this << m.console_id_hash;
    *this << m.mac_address;
    *this << m.nickname;
    *this << m.game_info.id;
    *this << m.game_info.name;
    return *this;
}

template <>
Packet& Packet::operator>>(GameInfo& g) {
    *this >> g.id;
    *this >> g.name;
    return *this;
}

template <>
Packet& Packet::operator>>(Room::Member& m) {
    *this >> m.client_id;
    *this >> m.console_id_hash;
    *this >> m.mac_address;
    *this >> m.nickname;
    *this >> m.game_info.id;
    *this >> m.game_info.name;
    return *this;
}

// Using the yojimbo macros will auto generate all of the different possible serialization types
// (read, write, and measure). In order to do that, we need to write the templated Serialize method
// in such a way that its valid for all of them.

struct JoinRequestMessage : public yojimbo::Message {
    u64 game_id;
    MacAddress preferred_mac{0};
    SHA1Hash console_id_hash{0};
    std::string nickname{""};
    std::string password{""};

    template <typename Stream>
    bool Serialize(Stream& stream) {
        Packet p;
        if (stream.IsWriting()) {
            p << game_id;
            p << preferred_mac;
            p << console_id_hash;
            p << nickname;
            p << password;
        }
        serialize_object(stream, p);
        if (stream.IsReading()) {
            p >> game_id;
            p >> preferred_mac;
            p >> console_id_hash;
            p >> nickname;
            p >> password;
        }

        return true;
    }
    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct JoinSuccessMessage : public yojimbo::Message {
    MacAddress assigned_mac{0};

    template <typename Stream>
    bool Serialize(Stream& stream) {
        Packet p;
        if (stream.IsWriting()) {
            p << assigned_mac;
        }
        serialize_object(stream, p);
        if (stream.IsReading()) {
            p >> assigned_mac;
        }
        return true;
    }
    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct RoomInformationMessage : public yojimbo::Message {
    u16 port;
    u8 member_slots;
    std::string name{""};
    std::string uid{""};
    std::vector<GameInfo> preferred_games{};
    std::vector<Room::Member> members{};

    template <typename Stream>
    bool Serialize(Stream& stream) {
        Packet p;
        if (stream.IsWriting()) {
            p << port;
            p << member_slots;
            p << name;
            p << uid;
            p << preferred_games;
            p << members;
        }
        serialize_object(stream, p);
        if (stream.IsReading()) {
            p >> port;
            p >> member_slots;
            p >> name;
            p >> uid;
            p >> preferred_games;
            p >> members;
        }
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct SetGameInfoMessage : public yojimbo::Message {
    GameInfo game_info;

    template <typename Stream>
    bool Serialize(Stream& stream) {
        Packet p;
        if (stream.IsWriting()) {
            p << game_info;
        }
        serialize_object(stream, p);
        if (stream.IsReading()) {
            p >> game_info;
        }
        return true;
    }
    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct WifiPacketMessage : public yojimbo::Message {
    MacAddress assigned_mac{0};

    template <typename Stream>
    bool Serialize(Stream& stream) {
        Packet p;
        if (stream.IsWriting()) {
            p << assigned_mac;
        }
        serialize_object(stream, p);
        if (stream.IsReading()) {
            p >> assigned_mac;
        }
        return true;
    }
    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct JoinFailureMessage : public yojimbo::Message {
    MacAddress assigned_mac{0};

    template <typename Stream>
    bool Serialize(Stream& stream) {
        Packet p;
        if (stream.IsWriting()) {
            p << assigned_mac;
        }
        serialize_object(stream, p);
        if (stream.IsReading()) {
            p >> assigned_mac;
        }
        return true;
    }
    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct CloseRoomMessage : public yojimbo::Message {
    MacAddress assigned_mac{0};

    template <typename Stream>
    bool Serialize(Stream& stream) {
        Packet p;
        if (stream.IsWriting()) {
            p << assigned_mac;
        }
        serialize_object(stream, p);
        if (stream.IsReading()) {
            p >> assigned_mac;
        }
        return true;
    }
    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
};

YOJIMBO_MESSAGE_FACTORY_START(CitraMessageFactory, COUNT);
YOJIMBO_DECLARE_MESSAGE_TYPE(IdJoinRequest, JoinRequestMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE(IdJoinSuccess, JoinSuccessMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE(IdRoomInformation, RoomInformationMessage);
YOJIMBO_MESSAGE_FACTORY_FINISH();

} // namespace Network
