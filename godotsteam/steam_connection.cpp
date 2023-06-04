#include "steam_connection.h"

void SteamConnection::_bind_methods() {
}

bool SteamConnection::operator==(const SteamConnection &data) {
    return steam_id == data.steam_id;
}

EResult SteamConnection::rawSend(Packet *packet) {
    if (packet->channel == ChannelManagement::PING_CHANNEL) {
        if (packet->size != sizeof(PingPayload)) {
            Steam::get_singleton()->steamworksError("THIS PING IS THE WRONG SIZE, REJECTING!");
            return k_EResultFail;
        }
    }
    return SteamNetworkingMessages()->SendMessageToUser(networkIdentity, packet->data, packet->size, packet->transfer_mode, packet->channel);
}

Error SteamConnection::sendPending() {
    while (pending_retry_packets.size() != 0) {
        auto packet = pending_retry_packets.front()->get();
        auto errorCode = rawSend(packet);
        if (errorCode == k_EResultOK) {
            delete packet;
            pending_retry_packets.pop_front();
        } else {
            auto errorString = "";//SteamMultiplayerPeer::convertEResultToString(errorCode);
            if (packet->transfer_mode & k_nSteamNetworkingSend_Reliable) {
                WARN_PRINT(String("Send Error! (reliable: will retry):") + errorString);
                break;
                //break and try resend later
            } else {
                WARN_PRINT(String("Send Error! (unreliable: won't retry):") + errorString);
                delete packet;
                pending_retry_packets.pop_front();
                //toss the unreliable packet and move on?
            }
        }
    }
    return OK;
}

void SteamConnection::addPacket(Packet *packet) {
    pending_retry_packets.push_back(packet);
}

Error SteamConnection::send(Packet *packet) {
    addPacket(packet);
    return sendPending();
}

Error SteamConnection::ping(const PingPayload &p) {
    last_msg_timestamp = Time::get_singleton()->get_ticks_msec(); // only ping once per maxDeltaT;

    auto packet = new Packet((void *)&p, sizeof(PingPayload), MultiplayerPeer::TRANSFER_MODE_RELIABLE, PING_CHANNEL);
    return send(packet);
}

Error SteamConnection::ping() {
    auto p = PingPayload();
    return ping(p);
}

SteamConnection::SteamConnection(CSteamID steamId) {
    this->peer_id = -1;
    this->steam_id = steamId;
    this->last_msg_timestamp = 0;
    networkIdentity = SteamNetworkingIdentity();
    networkIdentity.SetSteamID(steamId);
}

Dictionary SteamConnection::collect_debug_data() {
    Dictionary output;
    output["peer_id"] = peer_id;
    output["steam_id"] = steam_id.GetAccountID();
    output["pending_packet_count"] = pending_retry_packets.size();
    SteamNetConnectionRealTimeStatus_t info;
    SteamNetworkingMessages()->GetSessionConnectionInfo(networkIdentity, nullptr, &info);
    switch (info.m_eState) {
        case k_ESteamNetworkingConnectionState_None:
            output["connection_status"] = "None";
            break;
        case k_ESteamNetworkingConnectionState_Connecting:
            output["connection_status"] = "Connecting";
            break;
        case k_ESteamNetworkingConnectionState_FindingRoute:
            output["connection_status"] = "FindingRoute";
            break;
        case k_ESteamNetworkingConnectionState_Connected:
            output["connection_status"] = "Connected";
            break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer:
            output["connection_status"] = "ClosedByPeer";
            break;
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            output["connection_status"] = "ProblemDetectedLocally";
            break;
    }
    output["packets_per_sec"] = info.m_flOutPacketsPerSec;
    output["bytes_per_sec"] = info.m_flOutBytesPerSec;
    output["packets_per_sec"] = info.m_flInPacketsPerSec;
    output["bytes_per_sec"] = info.m_flInBytesPerSec;
    output["connection_quality_local"] = info.m_flConnectionQualityLocal;
    output["connection_quality_remote"] = info.m_flConnectionQualityRemote;
    output["send_rate_bytes_per_second"] = info.m_nSendRateBytesPerSecond;
    output["pending_unreliable"] = info.m_cbPendingUnreliable;
    output["pending_reliable"] = info.m_cbPendingReliable;
    output["sent_unacked_reliable"] = info.m_cbSentUnackedReliable;
    output["queue_time"] = info.m_usecQueueTime;

    output["ping"] = info.m_nPing;
    return output;
}

SteamConnection::~SteamConnection() {
    SteamNetworkingMessages()->CloseSessionWithUser(networkIdentity);
    while (pending_retry_packets.size()) {
        delete pending_retry_packets.front()->get();
        pending_retry_packets.pop_front();
    }
}
