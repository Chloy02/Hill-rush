#include "net/EnetSupport.hpp"

#include "core/Log.hpp"

#include <cstdlib>

namespace hr::net {

bool initEnet()
{
    static bool ready = false;
    if (ready) return true;
    if (enet_initialize() != 0) {
        LOG_ERROR("Could not initialise networking");
        return false;
    }
    std::atexit(enet_deinitialize);
    ready = true;
    return true;
}

std::string ipString(const ENetAddress& address)
{
    char text[64] = {};
    if (enet_address_get_host_ip(&address, text, sizeof(text)) != 0) return {};
    return text;
}

} // namespace hr::net
