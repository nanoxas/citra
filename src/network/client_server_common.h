// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "network/room.h"
#include "yojimbo/yojimbo.h"

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
        serialize_uint64(stream, game_id);
        serialize_bytes(stream, preferred_mac.data(), preferred_mac.size());
        serialize_bytes(stream, console_id_hash.data(), console_id_hash.size());

        int len = nickname.size();
        char* str = nickname.c_str();
        serialize_string(stream, str, len);
        if (stream.IsReading()) {
            nickname.assign(str, len);
        }

        len = password.size();
        str = password.c_str();
        serialize_string(stream, str, len);
        if (stream.IsReading()) {
            password.assign(str, len);
        }
        return true;
    }
    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct JoinSuccessMessage : public yojimbo::Message {
    MacAddress assigned_mac{0};

    template <typename Stream>
    bool Serialize(Stream& stream) {
        serialize_bytes(stream, assigned_mac.data(), 6);
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
        serialize_bits(stream, port, 16);
        serialize_bits(stream, member_slots, 8);

        int len = name.size();
        char* str = name.c_str();
        serialize_string(stream, str, len);
        if (stream.IsReading()) {
            name.assign(str, len);
        }

        len = uid.size();
        str = uid.c_str();
        serialize_string(stream, str, len);
        if (stream.IsReading()) {
            uid.assign(str, len);
        }

        u8 vector_size = preferred_games.size();
        serialize_bits(stream, vector_size, 8);
        for (int i = 0; i < vector_size; ++i) {
            GameInfo game_info;
            if (stream.IsWriting()) {
                game_info = preferred_games[i];
            }
            serialize_uint64(stream, game_info.id);

            len = game_info.name.size();
            str = game_info.name.c_str();
            serialize_string(stream, str, len);
            if (stream.IsReading()) {
                game_info.name.assign(str, len);
                preferred_games.push_back(game_info);
            }
        }

        vector_size = members.size();
        serialize_byte(stream, vector_size, 8);
        for (int i = 0; i < vector_size; ++i) {
            Room::Member member{};
            if (stream.IsWriting()) {
                member = members[i];
            }
            serialize_uint8(stream, member.client_id);

            len = member.nickname.size();
            str = member.nickname.c_str();
            serialize_string(stream, str, len);
            if (stream.IsReading()) {
                member.nickname.assign(str, len);
            }

            GameInfo game_info;
            if (stream.IsWriting()) {
                game_info = preferred_games[i];
            }
            serialize_uint64(stream, game_info.id);

            len = game_info.name.size();
            str = game_info.name.c_str();
            serialize_string(stream, str, len);
            if (stream.IsReading()) {
                game_info.name.assign(str, len);
                member.game_info = game_info;
            }

            serialize_bytes(stream, member.preferred_mac.data(), member.preferred_mac.size());
            serialize_bytes(stream, member.console_id_hash.data(), member.console_id_hash.size());
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
