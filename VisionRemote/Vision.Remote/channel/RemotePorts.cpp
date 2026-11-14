/**
 * @file RemotePorts.cpp
 * @brief 本进程端口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemotePorts.h"
#include "Messages.h"

namespace Vision {

int RemotePorts::s_listen = RemoteConst::kPort;
int RemotePorts::s_discover = RemoteConst::kDiscoverPort;
int RemotePorts::s_direct = 19102;
int RemotePorts::s_preview = 19180;
int RemotePorts::s_relay = 19200;
int RemotePorts::s_relayWs = 19280;
const char* RemotePorts::s_access = "remote.subapi.fun";
bool RemotePorts::s_accessOn = true;

int RemotePorts::listen()
{
    return s_listen;
}

int RemotePorts::discover()
{
    return s_discover;
}

int RemotePorts::direct()
{
    return s_direct;
}

int RemotePorts::preview()
{
    return s_preview;
}

int RemotePorts::relay()
{
    return s_relay;
}

int RemotePorts::relayWs()
{
    return s_relayWs;
}

const char* RemotePorts::accessHost()
{
    return s_access ? s_access : "";
}

bool RemotePorts::accessOnByDefault()
{
    return s_accessOn;
}

void RemotePorts::useDesk()
{
    s_listen = 29300;
    s_discover = 29301;
    s_direct = 29302;
    s_preview = 29380;
    s_relay = 29400;
    s_relayWs = 29480;
    s_access = "";
    s_accessOn = false;
}

}
