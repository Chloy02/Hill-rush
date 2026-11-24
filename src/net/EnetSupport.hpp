#pragma once

// Private to src/net: ENet's headers pull in <winsock2.h>/<windows.h> on
// Windows, whose names clash with raylib's, so they never appear in a public
// header.

#include <enet/enet.h>

#include <string>

namespace hr::net {

// Initialises ENet once per process (on first use). False if sockets are unavailable.
bool initEnet();

// "192.168.1.20" for an address (host in network byte order).
std::string ipString(const ENetAddress& address);

} // namespace hr::net
