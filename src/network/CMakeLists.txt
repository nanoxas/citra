add_library(network STATIC
    client_server_common.h
    network.cpp
    network.h
    packet.cpp
    packet.h
    room.cpp
    room.h
    room_member.cpp
    room_member.h
)

create_target_directory_groups(network)

target_link_libraries(network PRIVATE common yojimbo)
