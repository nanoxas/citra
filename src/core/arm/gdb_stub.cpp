// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <map>
#ifdef _WIN32
#include <io.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iphlpapi.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "core/arm/gdb_stub.h"

// notice: the PacketSize needs to be set to the hex of this number
#define GDB_BFR_MAX  10000
#define GDB_STUB_START '$'
#define GDB_STUB_END   '#'
#define GDB_STUB_ACK   '+'
#define GDB_STUB_NAK   '-'

// TODO: consider adding in some of the fields in this RSP gdb stub impl
// http://www.embecosm.com/appnotes/ean4/embecosm-howto-rsp-server-ean4-issue-2.html#sec_rsp_data_structures
static struct gdb_state {
    int socket = -1;
    struct sockaddr_in saddr_server;
    struct sockaddr_in saddr_client;
    bool stepping = false;
    u16 check_read = 0;
    std::map<u32, GDB::BreakPoint> breakpoints;
} GDBState;

// for sample xmls see the gdb source /gdb/features
// gdb also wants the l character at the start
// this XML defines what the registers are for this specific ARM device
static const char* target_xml =
    "l<?xml version=\"1.0\"?>"
    "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">"
//    "<architechture>arm</architechture>" // gdb didn't like this tag and i dunno why
    "<target>"
        "<feature name=\"org.gnu.gdb.arm.core\">"
            "<reg name=\"r0\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r1\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r2\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r3\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r4\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r5\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r6\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r7\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r8\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r9\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r10\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r11\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"r12\" bitsize=\"32\" type=\"uint32\"/>"
            "<reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>"
            "<reg name=\"lr\" bitsize=\"32\"/>"
            "<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>"
            "<reg name=\"cpsr\" bitsize=\"32\" regnum=\"25\"/>"
        "</feature>"
        "<feature name=\"org.gnu.gdb.arm.vfp\">"
            "<reg name=\"d0\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d1\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d2\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d3\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d4\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d5\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d6\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d7\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d8\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d9\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d10\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d11\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d12\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d13\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d14\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"d15\" bitsize=\"64\" type=\"ieee_double\"/>"
            "<reg name=\"fpscr\" bitsize=\"32\" type=\"int\" group=\"float\"/>"
        "</feature>"
    "</target>"
;

#ifdef _WIN32
WSADATA InitData;
#define ssize_t int
#define SHUT_RDWR 2
#endif

// private helpers definitions at the bottom of the file
static u8 hex2char(u8 hex);
static u8 nibble2hex(u8 nibble);
static void mem2hex(u8 *dst, const u8 *src, u32 len);
static void hex2mem(u8 *dst, const u8 *src, u32 len);
static void wbe32hex(u8 *dst, u32 v);
static void wbe64hex(u8 *dst, u64 v);
static u32 re32hex(const u8 *p);
static u64 re64hex(const u8 *p);

// gdb internal functions
static u8 gdb_read_byte() {
    char c = '+';
    ssize_t res = recv(GDBState.socket, &c, 1, MSG_WAITALL);
    if (res != 1) {
        LOG_ERROR(GDB, "recv failed : %ld Closing connection.", res);
        GDB::DeInit();
    }
    return static_cast<u8>(c);
}

static u8 gdb_calc_chksum(std::string command) {
    u64 len = command.length();
    const u8 *ptr = reinterpret_cast<const u8 *>(command.c_str());
    u8 c = 0;
    while (len-- > 0) {
        c += *ptr++;
    }
    return c;
}

static void gdb_nak() {
    const char nak = GDB_STUB_NAK;
    ssize_t res = send(GDBState.socket, &nak, 1, 0);
    if (res != 1) {
        LOG_ERROR(GDB, "Sending Nack failed.");
    }
}

static void gdb_ack() {
    const char ack = GDB_STUB_ACK;
    ssize_t res = send(GDBState.socket, &ack, 1, 0);
    if (res != 1) {
        LOG_ERROR(GDB, "Sending Ack failed.");
    }
}

// Take the first character and determine the action to take
static bool _handle_packet_header() {
    u8 c = gdb_read_byte();
    if (c == GDB_STUB_ACK) {
        //ignore ack
        return false;
    } else if (c == SIGINT) {
        Core::Halt("gdb: CtrlC Signal sent");
        GDB::Signal(SIGINT);
        return false;
    } else if (c != GDB_STUB_START) {
        LOG_ERROR(GDB, "Read invalid byte '%02x' Expected: \'%c\'", c, GDB_STUB_START);
        return false;
    }
    return true;
}

static std::string gdb_read_command() {
    std::string command;
    u8 c;
    while ((c = gdb_read_byte()) != GDB_STUB_END) {
        command += c;
    }
    // checking checksum (the two bytes after GDB_STUB_END)
    u8 chk_read = hex2char(gdb_read_byte()) << 4;
    chk_read |= hex2char(gdb_read_byte());

    u8 chk_calc = gdb_calc_chksum(command);

    if (chk_calc != chk_read) {
        LOG_ERROR(GDB, "gdb: invalid checksum: calculated %02x and read %02x for $%s# (length: %lu)\n", chk_calc, chk_read, command.c_str(), command.length());
        gdb_nak();
        return "";
    }

    gdb_ack();
    return command;
}

static bool gdb_data_available() {
    struct timeval t;
    fd_set _fds, *fds = &_fds;

    FD_ZERO(fds);
    FD_SET(GDBState.socket, fds);

    t.tv_sec = 0;
    t.tv_usec = 20;

    if (select(GDBState.socket + 1, fds, nullptr, nullptr, &t) < 0) {
        LOG_ERROR(GDB, "select failed");
        return 0;
    }

    if (FD_ISSET(GDBState.socket, fds)) {
        return 1;
    }
    return 0;
}

static void gdb_reply(std::string reply) {
    if (!GDB::IsActive())
        return;

    u8 chk = gdb_calc_chksum(reply);
    std::string command = GDB_STUB_START + reply + GDB_STUB_END;
    command += nibble2hex(chk >> 4);
    command += nibble2hex(chk);

    const char *ptr = command.c_str();
    u64 left = command.length();
    while (left > 0) {
#ifdef _WIN32
        ssize_t n = send(GDBState.socket, const_cast<char*>(ptr), left, 0);
#else
        ssize_t n = send(GDBState.socket, ptr, left, 0);
#endif
        if (n < 0) {
            LOG_ERROR(GDB, "gdb: send failed");
            return GDB::DeInit();
        }
        left -= n;
        ptr += n;
    }
}

static std::string _get_status() {
    // BEST PRACTICES:
    // Since register values can be supplied with this packet, it is often useful
    // to return the PC, SP, FP, LR (if any), and FLAGS regsiters so that separate
    // packets don't need to be sent to read each of these registers from each
    // thread.
    char buf[128];
    memset(buf, 0, 128);
    sprintf(buf, "%02x:%08x;%02x:%08x;%02x:%08x;%02x:%08x;",
            13, Common::swap32(Core::g_app_core->GetReg(13)),
            14, Common::swap32(Core::g_app_core->GetReg(14)),
            15, Common::swap32(Core::g_app_core->GetPC()),
            25, Common::swap32(Core::g_app_core->GetCPSR())
    );
    return std::string(buf);
}

static void gdb_handle_signal(int sig=SIGTRAP) {
    char buf[128];
    sprintf(buf, "T%02x%s", sig, _get_status().c_str());
    gdb_reply(buf);
}

static void gdb_handle_query(std::string command) {
    if (command == "TStatus") {
        return gdb_reply("");
    } else if (command.substr(0, 9) == "Supported") {
        // we want to send the target info so we tell gdb to ask about it
        return gdb_reply("PacketSize=2710;qXfer:features:read+");
    } else if (command.substr(0, 29) == "Xfer:features:read:target.xml") {
        // now gdb is asking about it so tell it we are arm
        return gdb_reply(target_xml);
    } else {
        gdb_reply("");
    }
}

static void gdb_handle_set_thread(std::string command) {
    if ((command.substr(0, 2) == "g0") ||
        (command.substr(0, 3) == "c-1") ||
        (command.substr(0, 2) == "c0") ||
        (command.substr(0, 2) == "c1")) {
        return gdb_reply("OK");
    }
    gdb_reply("E01");
}

static std::string _read_register(u32 id) {
    u8 reply[9] = {0};
    if (0 <= id && id < 0x10) {
        wbe32hex(reply, Core::g_app_core->GetReg(id));
    } else if (id == 0x19) {
        wbe32hex(reply, Core::g_app_core->GetCPSR());
    } else if (0x1a <= id && id < 0x2a) {
        wbe64hex(reply, Core::g_app_core->GetVFP(id-0x1a));
    } else if (id == 0x2a) {
        wbe32hex(reply, Core::g_app_core->GetFPSCR());
    } else {
        return "E00";
    }
    return std::string(reinterpret_cast<char*>(reply));
}

static void gdb_read_register(std::string command) {
    u32 id = hex2char(static_cast<u8>(command[0]));
    if (command.length() > 1) {
        id <<= 4;
        id |= hex2char(static_cast<u8>(command[1]));
    }

    gdb_reply(_read_register(id));
}

static void gdb_read_registers() {
    std::string buffer;
    // size of all registers we return
    buffer.reserve((32 * 16) + 32 + (64 * 16) + 32);
    for (u8 i = 0; i < 16; i++) {
        buffer += _read_register(i);
    }
    buffer += _read_register(25);
    for (u8 i = 26; i < 26+16; i++) {
        buffer += _read_register(i);
    }
    buffer += _read_register(42);

    gdb_reply(buffer);
}

static std::string _write_register(u32 id, const u8* buf) {
    if (0 <= id && id < 0x10) {
        Core::g_app_core->SetReg(id, re32hex(buf));
    } else if (id == 0x19) {
        Core::g_app_core->SetCPSR(re32hex(buf));
    } else if (0x1a <= id && id < 0x2a) {
        Core::g_app_core->SetVFP(id-0x1a, re64hex(buf));
    } else if (id == 0x2a) {
        Core::g_app_core->SetFPSCR(re32hex(buf));
    } else {
        return "E00";
    }
    return "OK";
}

static void gdb_write_register(std::string command) {
    u32 id = hex2char(static_cast<u8>(command[0]));
    u64 offset = command.find_first_of('=') + 1;
    if (command.length() > 1) {
        id <<= 4;
        id |= hex2char(static_cast<u8>(command[1]));
    }

    gdb_reply(_write_register(id, reinterpret_cast<const u8*>(command.c_str() + offset)));
}

static void gdb_write_registers(std::string command) {
    // TODO: What format does this enter as?
    LOG_WARNING(GDB, "Write registers not implemented");
    gdb_reply("E00");
}

static void gdb_read_mem(std::string command) {
    static u8 reply[GDB_BFR_MAX - 4];
    u32 addr = 0;
    u32 i = 0;
    while (command[i] != ',') {
        addr = (addr << 4) | hex2char(static_cast<u8>(command[i++]));
    }
    i++;

    u32 len = 0;
    while (i < command.length()) {
        len = (len << 4) | hex2char(static_cast<u8>(command[i++]));
    }
    u8 * data = Memory::GetPointer(addr);
    if (!data) {
        return gdb_reply("E01");
    }

    mem2hex(reply, data, len);
    reply[len*2] = '\0';
    gdb_reply((char *)reply);
}

static void gdb_write_mem(std::string command) {
    u32 i = 0;
    u32 addr = 0;
    while (command[i] != ',') {
        addr = (addr << 4) | hex2char(static_cast<u8>(command[i++]));
    }
    i++;

    u32 len = 0;
    while (command[i] != ':') {
        len = (len << 4) | hex2char(static_cast<u8>(command[i++]));
    }
    u8 * dst = Memory::GetPointer(addr);
    if (!dst) {
        return gdb_reply("E00");
    }
    hex2mem(dst, reinterpret_cast<const u8*>(command.c_str() + i + 1), len);
    gdb_reply("OK");
}

static void gdb_step() {
    GDB::Break();
    Core::RunLoop(1);
}

static void gdb_continue() {
    GDBState.stepping = false;
}

static void gdb_add_bp(std::string command) {
    u32 i, addr = 0, len = 0;
    i = 2;
    while (command[i] != ',') {
        addr = addr << 4 | hex2char(static_cast<u8>(command[i++]));
    }
    i++;
    while (i < command.length()) {
        len = len << 4 | hex2char(static_cast<u8>(command[i++]));
    }
    GDB::BreakPoint b = {len};
    GDBState.breakpoints[addr] = b;
    gdb_reply("OK");
}

static void gdb_remove_bp(std::string command) {
    u32 addr = 0;
    u32 i = 2;
    while (command[i] != ',') {
        addr = (addr << 4) | hex2char(static_cast<u8>(command[i++]));
    }
    GDBState.breakpoints.erase(addr);
    gdb_reply("OK");
}

// Public Functionality
namespace GDB {

void Init(u16 port) {
#ifdef _WIN32
    WSAStartup(MAKEWORD(2, 2), &InitData);
#endif

    int tmpsock = socket(AF_INET, SOCK_STREAM, 0);
    if (tmpsock == -1) {
        LOG_ERROR(GDB, "Failed to create gdb socket");
    }

    int on = 1;
#ifdef _WIN32
    // Windows wants the sockOpt as a char*
    if (setsockopt(tmpsock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&on), sizeof on) < 0) {
        LOG_ERROR(GDB, "Failed to setsockopt");
    }
#else
    if (setsockopt(tmpsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) < 0) {
        LOG_ERROR(GDB, "Failed to setsockopt");
    }
#endif

    memset(&GDBState.saddr_server, 0, sizeof GDBState.saddr_server);
    GDBState.saddr_server.sin_family = AF_INET;
    GDBState.saddr_server.sin_port = htons(port);
    GDBState.saddr_server.sin_addr.s_addr = INADDR_ANY;

    if (bind(tmpsock, (struct sockaddr *)&GDBState.saddr_server, sizeof GDBState.saddr_server) < 0)
        LOG_ERROR(GDB, "Failed to bind gdb socket");

    if (listen(tmpsock, 1) < 0)
        LOG_ERROR(GDB, "Failed to listen to gdb socket");

    LOG_INFO(GDB, "Waiting for gdb to connect...\n");
    
    int len = sizeof(GDBState.saddr_client);
    GDBState.socket = accept(tmpsock, (struct sockaddr *)&GDBState.saddr_client, &len);
    if (GDBState.socket < 0)
        LOG_ERROR(GDB, "Failed to accept gdb client");

    LOG_INFO(GDB, "Client connected.\n");
    GDBState.saddr_client.sin_addr.s_addr = ntohl(GDBState.saddr_client.sin_addr.s_addr);
//    if (((saddr_client.sin_addr.s_addr >> 24) & 0xff) != 127 ||
//         ((saddr_client.sin_addr.s_addr >> 16) & 0xff) !=   0 ||
//         ((saddr_client.sin_addr.s_addr >>  8) & 0xff) !=   0 ||
//         ((saddr_client.sin_addr.s_addr >>  0) & 0xff) !=   1)
//         LOG_ERROR(GDB, "gdb: incoming connection not from localhost");
#ifdef _WIN32
    closesocket(tmpsock);
#else
    close(tmpsock);
#endif
}

void DeInit() {
    if (GDBState.socket != -1) {
        shutdown(GDBState.socket, SHUT_RDWR);
        GDBState.socket = -1;
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

bool IsActive() {
    return GDBState.socket != -1;
}

//static u64 counter = 0;
bool IsStepping() {
    if (GDBState.stepping) {
        GDBState.stepping = false;
        return true;
    }
    // check to see if we got a anything from gdb
    // check_read should limit the number of times we query the socket for a small speedup
    if (GDBState.check_read++ == 0  && gdb_data_available()) {
        GDBState.stepping = true;
    }

    // get the CPSR and check to see if we are in Thumb mode
    // and then subtract either 4 or 8 to check address
    u32 pc = Core::g_app_core->GetPC();
    bool is_thumb_mode = (Core::g_app_core->GetCPSR() & (1 << 23)) != 0;
    return (is_thumb_mode) ? GDBState.breakpoints.count(pc-4) == 1 : GDBState.breakpoints.count(pc-8) == 1;
}

int Signal(u32 sig) {
    if (GDBState.socket == -1) {
        return 1;
    }
    gdb_handle_signal(sig);
    return 0;
}

void Break() {
    GDBState.stepping = true;
}

void HandleException() {
    while (GDB::IsActive()) {
        if (!gdb_data_available())
            continue;
        if (!_handle_packet_header())
            continue;
        std::string command = gdb_read_command();
        if (command.length() == 0)
            continue;
        const char action = command[0];
        command = command.substr(1);
        switch (action) {
        case 'q':
            gdb_handle_query(command);
            break;
        case 'H':
            gdb_handle_set_thread(command);
            break;
        case '?':
            gdb_handle_signal();
            break;
        case 'k':
            GDB::DeInit();
            LOG_INFO(GDB, "killed by gdb");
            return;
        case 'g':
            gdb_read_registers();
            break;
        case 'G':
            gdb_write_registers(command);
            break;
        case 'p':
            gdb_read_register(command);
            break;
        case 'P':
            gdb_write_register(command);
            break;
        case 'm':
            gdb_read_mem(command);
            break;
        case 'M':
            gdb_write_mem(command);
            break;
        case 's':
            gdb_step();
            return;
        case 'C':
        case 'c':
            gdb_continue();
            return;
        case 'z':
            gdb_remove_bp(command);
            break;
        case 'Z':
            gdb_add_bp(command);
            break;
        default:
            gdb_reply("");
            break;
        }
    }
}

}

// private helper functions
static u8 hex2char(u8 hex) {
    if (hex >= '0' && hex <= '9')
        return hex - '0';
    else if (hex >= 'a' && hex <= 'f')
        return static_cast<u8>(hex - 'a' + 0xa);
    else if (hex >= 'A' && hex <= 'F')
        return static_cast<u8>(hex - 'A' + 0xa);

    LOG_ERROR(GDB, "Invalid nibble: %c (%02x)\n", hex, hex);
    return 0;
}

static u8 nibble2hex(u8 n) {
    n &= 0xf;
    if (n < 0xa)
        return '0' + n;
    else
        return static_cast<u8>('A' + n - 0xa);
}

static void mem2hex(u8 *dst, const u8 *src, u32 len) {
    u8 tmp;
    while (len-- > 0) {
        tmp = *src++;
        *dst++ = nibble2hex(tmp>>4);
        *dst++ = nibble2hex(tmp);
    }
}

static void hex2mem(u8 *dst, const u8 *src, u32 len) {
    while (len-- > 0) {
        *dst++ = (hex2char(*src) << 4) | hex2char(*(src+1));
        src += 2;
    }
}

// write big endian 32 bit as hex into dst
static void wbe32hex(u8 *dst, u32 v) {
    u8 src[4];
    ((u32*)src)[0] = (u32_be) v;
    mem2hex(dst, src, 4);
}

static void wbe64hex(u8 *dst, u64 v) {
    u8 src[8];
    ((u64*)src)[0] = (u64_be) v;
    mem2hex(dst, src, 4);
}

static u32 re32hex(const u8 *p) {
    u32 res = 0;
    for (u8 i = 0; i < 8; i++) {
        res = (res << 4) | hex2char(p[i]);
    }
    return res;
}

static u64 re64hex(const u8 *p) {
    u64 res = 0;
    for (u8 i = 0; i < 16; i++) {
        res = (res << 4) | hex2char(p[i]);
    }
    return res;
}
