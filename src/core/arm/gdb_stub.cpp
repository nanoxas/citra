// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#ifdef _WIN32
#include <io.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iphlpapi.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "core/arm/gdb_stub.h"

#define GDB_BFR_MAX  10000
#define GDB_MAX_BP   10

#define GDB_STUB_START '$'
#define GDB_STUB_END   '#'
#define GDB_STUB_ACK   '+'
#define GDB_STUB_NAK   '-'

static int tmpsock = -1;
static int sock = -1;
static struct sockaddr_in saddr_server, saddr_client;

static u8 cmd_bfr[GDB_BFR_MAX];
static u32 cmd_len;

static u32 sig = 0;
static u32 send_signal = 0;
static u32 step_break = 0;


#ifdef _WIN32
WSADATA InitData;
#endif

// private helpers
static u8 hex2char(u8 hex) {
    if (hex >= '0' && hex <= '9')
        return hex - '0';
    else if (hex >= 'a' && hex <= 'f')
        return hex - 'a' + 0xa;
    else if (hex >= 'A' && hex <= 'F')
        return hex - 'A' + 0xa;

    LOG_ERROR(GDB, "Invalid nibble: %c (%02x)\n", hex, hex);
    return 0;
}

static u8 nibble2hex(u8 n) {
    n &= 0xf;
    if (n < 0xa)
        return '0' + n;
    else
        return 'A' + n - 0xa;
}

static void mem2hex(u8 *dst, u8 *src, u32 len) {
    u8 tmp;

    while (len-- > 0) {
        tmp = *src++;
        *dst++ = nibble2hex(tmp>>4);
        *dst++ = nibble2hex(tmp);
    }
}

static void hex2mem(u8 *dst, u8 *src, u32 len) {
    while (len-- > 0) {
        *dst++ = (hex2char(*src) << 4) | hex2char(*(src+1));
        src += 2;
    }
}

static u8 gdb_read_byte() {
    ssize_t res;
    char c = '+';

    res = recv(sock, &c, 1, MSG_WAITALL);
    if (res != 1) {
        LOG_ERROR(GDB, "recv failed : %ld", res);
        GDB::DeInit();
    }

    return c;
}

static u8 gdb_calc_chksum() {
    u32 len = cmd_len;
    u8 *ptr = cmd_bfr;
    u8 c = 0;

    while (len-- > 0)
        c += *ptr++;

    return c;
}

static void gdb_nak() {
    const char nak = GDB_STUB_NAK;
    ssize_t res;

    res = send(sock, &nak, 1, 0);
    if (res != 1)
        LOG_ERROR(GDB, "send failed");
}

static void gdb_ack() {
    const char ack = GDB_STUB_ACK;
    ssize_t res;

    res = send(sock, &ack, 1, 0);
    if (res != 1)
        LOG_ERROR(GDB, "send failed");
}

static void gdb_read_command() {
    u8 c;
    u8 chk_read, chk_calc;

    cmd_len = 0;
    memset(cmd_bfr, 0, sizeof cmd_bfr);

    c = gdb_read_byte();
    if (c == '+') {
        //ignore ack
        return;
    } else if (c == 0x03) {
        Core::Halt("gdb: CtrlC Signal sent");
        GDB::Signal(SIGTRAP);
        return;
    } else if (c != GDB_STUB_START) {
        LOG_DEBUG(GDB, "gdb: read invalid byte %02x\n", c);
        return;
    }

    while ((c = gdb_read_byte()) != GDB_STUB_END) {
        cmd_bfr[cmd_len++] = c;
        if (cmd_len == sizeof cmd_bfr) {
            LOG_ERROR(GDB, "gdb: cmd_bfr overflow\n");
            gdb_nak();
            return;
        }
    }

    chk_read = hex2char(gdb_read_byte()) << 4;
    chk_read |= hex2char(gdb_read_byte());

    chk_calc = gdb_calc_chksum();

    if (chk_calc != chk_read) {
        LOG_ERROR(GDB, "gdb: invalid checksum: calculated %02x and read %02x for $%s# (length: %d)\n", chk_calc, chk_read, cmd_bfr, cmd_len);
        cmd_len = 0;

        gdb_nak();
        return;
    }

    LOG_DEBUG(GDB, "gdb: read command %c with a length of %d: %s\n", cmd_bfr[0], cmd_len, cmd_bfr);
    gdb_ack();
}

static int gdb_data_available() {
    struct timeval t;
    fd_set _fds, *fds = &_fds;

    FD_ZERO(fds);
    FD_SET(sock, fds);

    t.tv_sec = 0;
    t.tv_usec = 20;

    if (select(sock + 1, fds, nullptr, nullptr, &t) < 0) {
        LOG_ERROR(GDB, "select failed");
        return 0;
    }

    if (FD_ISSET(sock, fds))
        return 1;
    return 0;
}

static void gdb_reply(const char *reply) {
    u8 chk;
    u32 left;
    u8 *ptr;
    int n;

    if (!GDB::IsActive())
        return;

    memset(cmd_bfr, 0, sizeof cmd_bfr);

    cmd_len = strlen(reply);
    if (cmd_len + 4 > sizeof cmd_bfr)
        LOG_ERROR(GDB, "cmd_bfr overflow in gdb_reply");

    memcpy(cmd_bfr + 1, reply, cmd_len);

    cmd_len++;
    chk = gdb_calc_chksum();
    cmd_len--;
    cmd_bfr[0] = GDB_STUB_START;
    cmd_bfr[cmd_len + 1] = GDB_STUB_END;
    cmd_bfr[cmd_len + 2] = nibble2hex(chk >> 4);
    cmd_bfr[cmd_len + 3] = nibble2hex(chk);

    LOG_DEBUG(GDB, "gdb: reply (len: %d): %s\n", cmd_len, cmd_bfr);

    ptr = cmd_bfr;
    left = cmd_len + 4;
    while (left > 0) {
#ifdef _WIN32
        n = send(sock, (char *)ptr, left, 0);
#else
        n = send(sock, ptr, left, 0);
#endif

        if (n < 0) {
            LOG_ERROR(GDB, "gdb: send failed");
            return GDB::DeInit();
        }
        left -= n;
        ptr += n;
    }
}

static void gdb_handle_query() {
    LOG_DEBUG(GDB, "gdb: query '%s'\n", cmd_bfr+1);

    if (!strcmp((const char *)(cmd_bfr+1), "TStatus")) {
        return gdb_reply("T0");
    }

    gdb_reply("");
}

static void gdb_handle_set_thread() {
    if (memcmp(cmd_bfr, "Hg0", 3) == 0 ||
        memcmp(cmd_bfr, "Hc-1", 4) == 0 ||
        memcmp(cmd_bfr, "Hc0", 4) == 0 ||
        memcmp(cmd_bfr, "Hc1", 4) == 0)
        return gdb_reply("OK");
    gdb_reply("E01");
}

static void gdb_handle_signal() {
    char bfr[128];
    memset(bfr, 0, sizeof bfr);
    sprintf(bfr, "T%02x%02x:%08x;%02x:%08x;", sig, 15, Core::g_app_core->GetPC(), 1, Core::g_app_core->GetReg(1));
    gdb_reply(bfr);
}

static void wbe32hex(u8 *p, u32 v) {
    u32 i;
    for (i = 0; i < 8; i++)
        p[i] = nibble2hex(v >> (28 - 4*i));
}

static void wbe64hex(u8 *p, u64 v) {
    u32 i;
    for (i = 0; i < 16; i++)
        p[i] = nibble2hex(v >> (60 - 4*i));
}

static u32 re32hex(u8 *p) {
    u32 i;
    u32 res = 0;

    for (i = 0; i < 8; i++)
        res = (res << 4) | hex2char(p[i]);

    return res;
}

static u64 re64hex(u8 *p) {
    u32 i;
    u64 res = 0;

    for (i = 0; i < 16; i++)
        res = (res << 4) | hex2char(p[i]);

    return res;
}

static void gdb_read_register() {
    static u8 reply[64];
    u32 id;

    memset(reply, 0, sizeof reply);
    id = hex2char(cmd_bfr[1]);
    if (cmd_bfr[2] != '\0')
    {
        id <<= 4;
        id |= hex2char(cmd_bfr[2]);
    }

    LOG_DEBUG(GDB, "Reading register %d contents: %d", id, Core::g_app_core->GetReg(id));
    if (0 <= id && id <= 12) {
        wbe32hex(reply, Core::g_app_core->GetReg(id));
    } else if (id == 15) {
        wbe32hex(reply, Core::g_app_core->GetPC());
    } else {
        return gdb_reply("E01");
    }


    //switch (id) {
    //    // TODO: Registers are only 0 ... 15 
    //    case 0 ... 31:
    //        // wbe32hex(reply, GPR(id));
    //        wbe32hex(reply, Core::g_app_core->GetReg(id));
    //        break;
    //    case 32 ... 63:
    //        // wbe64hex(reply, riPS0(id-32));
    //        break;
    //    case 64:
    //        wbe32hex(reply, Core::g_app_core->GetPC());
    //        break;
    //    case 65:
    //        // wbe32hex(reply, MSR);
    //        break;
    //    case 66:
    //        wbe32hex(reply, Core::g_app_core->GetCPSR());
    //        break;
    //    case 67:
    //        // wbe32hex(reply, LR);
    //        break;
    //    case 68:
    //        // wbe32hex(reply, CTR);
    //        break;
    //    case 69:
    //        // wbe32hex(reply, PowerPC::ppcState.spr[SPR_XER]);
    //        break;
    //    case 70:
    //        wbe32hex(reply, 0x0BADC0DE);
    //        break;
    //    case 71:
    //        // wbe32hex(reply, FPSCR.Hex);
    //        break;
    //    default:
    //        return gdb_reply("E01");
    //        break;
    //}

    gdb_reply((char *)reply);
}

static void gdb_read_registers() {
    static u8 bfr[GDB_BFR_MAX - 4];
    u8 * bufptr = bfr;
    u32 i;

    memset(bfr, 0, sizeof bfr);

    // TODO: Only 15 registers 
    for (i = 0; i < 16; i++)
    {
        wbe32hex(bufptr + i*8, Core::g_app_core->GetReg(i));
    }
    bufptr += 16 * 8;

    /*
    for (i = 0; i < 32; i++)
    {
        wbe32hex(bufptr + i*8, riPS0(i));
    }
    bufptr += 32 * 8;
    wbe32hex(bufptr, PC);      bufptr += 4;
    wbe32hex(bufptr, MSR);     bufptr += 4;
    wbe32hex(bufptr, GetCR()); bufptr += 4;
    wbe32hex(bufptr, LR);      bufptr += 4;


    wbe32hex(bufptr, CTR);     bufptr += 4;
    wbe32hex(bufptr, PowerPC::ppcState.spr[SPR_XER]); bufptr += 4;
    // MQ register not used.
    wbe32hex(bufptr, 0x0BADC0DE); bufptr += 4;
    */


    gdb_reply((char *)bfr);
}

static void gdb_write_registers() {
    u32 i;
    u8 * bufptr = cmd_bfr;
    // TODO Only 15 registers
    for (i = 0; i < 32; i++)
    {
        Core::g_app_core->SetReg(i, re32hex(bufptr + i*8));
    }
    bufptr += 32 * 8;

    gdb_reply("OK");
}

static void gdb_write_register() {
    u32 id;

    u8 * bufptr = cmd_bfr + 3;

    id = hex2char(cmd_bfr[1]);
    if (cmd_bfr[2] != '=') {
        ++bufptr;
        id <<= 4;
        id |= hex2char(cmd_bfr[2]);
    }

    if (0 <= id && id <= 12) {
        Core::g_app_core->SetReg(id, re32hex(bufptr));
    } else if (id == 15) {
        Core::g_app_core->SetPC(re32hex(bufptr));
    } else {
        return gdb_reply("E01");
    }


    //switch (id) {
    //    case 0 ... 31:
    //        Core::g_app_core->SetReg(id, re32hex(bufptr));
    //        break;
    //    case 32 ... 63:
    //        // riPS0(id-32) = re64hex(bufptr);
    //        break;
    //    case 64:
    //        Core::g_app_core->SetPC(re32hex(bufptr));
    //        break;
    //    case 65:
    //        // MSR = re32hex(bufptr);
    //        break;
    //    case 66:
    //        // SetCR(re32hex(bufptr));
    //        Core::g_app_core->SetCPSR(re32hex(bufptr));
    //        break;
    //    case 67:
    //        // LR = re32hex(bufptr);
    //        break;
    //    case 68:
    //        // CTR = re32hex(bufptr);
    //        break;
    //    case 69:
    //        // PowerPC::ppcState.spr[SPR_XER] = re32hex(bufptr);
    //        break;
    //    case 70:
    //        // do nothing, we dont have MQ
    //        break;
    //    case 71:
    //        // FPSCR.Hex = re32hex(bufptr);
    //        break;
    //    default:
    //        return gdb_reply("E01");
    //        break;
    //}

    gdb_reply("OK");
}

static void gdb_read_mem() {
    static u8 reply[GDB_BFR_MAX - 4];
    u32 addr, len;
    u32 i;

    i = 1;
    addr = 0;
    while (cmd_bfr[i] != ',')
        addr = (addr << 4) | hex2char(cmd_bfr[i++]);
    i++;

    len = 0;
    while (i < cmd_len)
        len = (len << 4) | hex2char(cmd_bfr[i++]);
    LOG_DEBUG(GDB, "gdb: read memory: %08x bytes from %08x\n", len, addr);

    if (len*2 > sizeof reply)
        gdb_reply("E01");
    u8 * data = Memory::GetPointer(addr);
    if (!data)
        return gdb_reply("E0");
    mem2hex(reply, data, len);
    reply[len*2] = '\0';
    gdb_reply((char *)reply);
}

static void gdb_write_mem() {
    u32 addr, len;
    u32 i;

    i = 1;
    addr = 0;
    while (cmd_bfr[i] != ',')
        addr = (addr << 4) | hex2char(cmd_bfr[i++]);
    i++;

    len = 0;
    while (cmd_bfr[i] != ':')
        len = (len << 4) | hex2char(cmd_bfr[i++]);
    LOG_DEBUG(GDB, "gdb: write memory: %08x bytes to %08x\n", len, addr);

    u8 * dst = Memory::GetPointer(addr);
    if (!dst)
        return gdb_reply("E00");
    hex2mem(dst, cmd_bfr + i + 1, len);
    gdb_reply("OK");
}

static void gdb_step() {
    GDB::Break();
    Core::RunLoop(1);
}

static void gdb_continue() {
    send_signal = 1;
    Core::RunLoop();
}

namespace GDB {
std::vector<GDB::BreakPoint> breakpoints = {};

void Init(u32 port) {
    socklen_t len;
    int on;
#ifdef _WIN32
    WSAStartup(MAKEWORD(2, 2), &InitData);
#endif

    tmpsock = socket(AF_INET, SOCK_STREAM, 0);
    if (tmpsock == -1) {
        LOG_ERROR(GDB, "Failed to create gdb socket");
    }

    on = 1;
#ifdef _WIN32
    // Windows wants the sockOpt as a char*
    if (setsockopt(tmpsock, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof on) < 0) {
        LOG_ERROR(GDB, "Failed to setsockopt");
    }
#else
    if (setsockopt(tmpsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) < 0) {
        LOG_ERROR(GDB, "Failed to setsockopt");
    }
#endif

    memset(&saddr_server, 0, sizeof saddr_server);
    saddr_server.sin_family = AF_INET;
    saddr_server.sin_port = htons(port);
    saddr_server.sin_addr.s_addr = INADDR_ANY;

    if (bind(tmpsock, (struct sockaddr *)&saddr_server, sizeof saddr_server) < 0)
        LOG_ERROR(GDB, "Failed to bind gdb socket");

    if (listen(tmpsock, 1) < 0)
        LOG_ERROR(GDB, "Failed to listen to gdb socket");

    LOG_INFO(GDB, "Waiting for gdb to connect...\n");

    sock = accept(tmpsock, (struct sockaddr *)&saddr_client, &len);
    if (sock < 0)
        LOG_ERROR(GDB, "Failed to accept gdb client");
    LOG_INFO(GDB, "Client connected.\n");

    saddr_client.sin_addr.s_addr = ntohl(saddr_client.sin_addr.s_addr);
    /*if (((saddr_client.sin_addr.s_addr >> 24) & 0xff) != 127 ||
    *      ((saddr_client.sin_addr.s_addr >> 16) & 0xff) !=   0 ||
    *      ((saddr_client.sin_addr.s_addr >>  8) & 0xff) !=   0 ||
    *      ((saddr_client.sin_addr.s_addr >>  0) & 0xff) !=   1)
    *      LOG_ERROR(GDB, "gdb: incoming connection not from localhost");
    */
#ifdef _WIN32
    closesocket(tmpsock);
#else
    close(tmpsock);
#endif
    tmpsock = -1;
}

void DeInit() {
    if (tmpsock != -1) {
        shutdown(tmpsock, SHUT_RDWR);
        tmpsock = -1;
    }
    if (sock != -1) {
        shutdown(sock, SHUT_RDWR);
        sock = -1;
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

bool IsActive() {
    return tmpsock != -1 || sock != -1;
}

bool IsStepping() {
    if (step_break) {
        step_break = 0;

        LOG_DEBUG(GDB, "Step was successful.");
        return 1;
    }

    u32 pc = Core::g_app_core->GetPC();
    return std::count_if(GDB::breakpoints.begin(), GDB::breakpoints.end(), 
                    [pc](GDB::BreakPoint const& b){
                        return b.address == pc;
                    }) > 0;
}

int Signal(u32 s) {
    if (sock == -1)
        return 1;

    sig = s;

    if (send_signal) {
        gdb_handle_signal();
        send_signal = 0;
    }

    return 0;
}

void Break() {
    step_break = 1;
    send_signal = 1;
}

void HandleException() {
    while (GDB::IsActive()) {
        if (!gdb_data_available())
            continue;
        gdb_read_command();
        if (cmd_len == 0)
            continue;

        switch (cmd_bfr[0]) {
        case 'q':
            gdb_handle_query();
            break;
        case 'H':
            gdb_handle_set_thread();
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
            gdb_write_registers();
            break;
        case 'p':
            gdb_read_register();
            break;
        case 'P':
            gdb_write_register();
            break;
        case 'm':
            gdb_read_mem();
            break;
        case 'M':
            gdb_write_mem();
            // PowerPC::ppcState.iCache.Reset();
            // Host_UpdateDisasmDialog();
            break;
        case 's':
            gdb_step();
            return;
        case 'C':
        case 'c':
            gdb_continue();
            return;
        case 'z':
            //gdb_remove_bp();
            break;
        case 'Z':
            //_gdb_add_bp();
            break;
        default:
            gdb_reply("");
            break;
        }
    }
}

}
