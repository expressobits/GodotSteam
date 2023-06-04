#include "steam_connection.h"

void SteamConnection::_bind_methods() {
}

SteamConnection::SteamConnection(CSteamID steamId) {
    this->peer_id = -1;
    this->steam_id = steamId;
    this->last_msg_timestamp = 0;
    networkIdentity = SteamNetworkingIdentity();
    networkIdentity.SetSteamID(steamId);
}

SteamConnection::~SteamConnection() {
    SteamNetworkingMessages()->CloseSessionWithUser(networkIdentity);
    while (pending_retry_packets.size()) {
        delete pending_retry_packets.front()->get();
        pending_retry_packets.pop_front();
    }
}
