#ifndef STEAM_CONNECTION_H
#define STEAM_CONNECTION_H

// Include Godot headers
#include <godot_cpp/classes/multiplayer_peer_extension.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/templates/hash_map.hpp>

// Steam APIs
#include "godotsteam.h"

class SteamConnection : public RefCounted {
	GDCLASS(SteamConnection, RefCounted);

public:
    enum ChannelManagement {
        PING_CHANNEL,
        SIZE
    };

    struct Packet {
        uint8_t data[MAX_STEAM_PACKET_SIZE];
        uint32_t size = 0;
        CSteamID sender = CSteamID();
        int channel = 0;
        int transfer_mode = k_nSteamNetworkingSend_Reliable;
        Packet() {}
        Packet(const void *p_buffer, uint32 p_buffer_size, int transferMode, int channel) {
            ERR_FAIL_COND_MSG(p_buffer_size > MAX_STEAM_PACKET_SIZE, "ERROR TRIED TO SEND A PACKET LARGER THAN MAX_STEAM_PACKET_SIZE");
            memcpy(this->data, p_buffer, p_buffer_size);
            this->size = p_buffer_size;
            this->sender = CSteamID();
            this->channel = channel;
            this->transfer_mode = transferMode;
        }
    };

    struct PingPayload {
        int peer_id = -1;
        CSteamID steam_id = CSteamID();
    };

    int peer_id;
	CSteamID steam_id;
	uint64_t last_msg_timestamp;
	SteamNetworkingIdentity networkIdentity;
	List<Packet *> pending_retry_packets;

protected:
    static void _bind_methods();

public:
	bool operator==(const SteamConnection &data) {
		return steam_id == data.steam_id;
	}
	EResult rawSend(Packet *packet) {
		if (packet->channel == ChannelManagement::PING_CHANNEL) {
			if (packet->size != sizeof(PingPayload)) {
				Steam::get_singleton()->steamworksError("THIS PING IS THE WRONG SIZE, REJECTING!");
				return k_EResultFail;
			}
		}
		return SteamNetworkingMessages()->SendMessageToUser(networkIdentity, packet->data, packet->size, packet->transfer_mode, packet->channel);
	}
	Error sendPending() {
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

	void addPacket(Packet *packet) {
		pending_retry_packets.push_back(packet);
	}
	Error send(Packet *packet) {
		addPacket(packet);
		return sendPending();
	}
	Error ping(const PingPayload &p) {
		last_msg_timestamp = Time::get_singleton()->get_ticks_msec(); // only ping once per maxDeltaT;

		auto packet = new Packet((void *)&p, sizeof(PingPayload), MultiplayerPeer::TRANSFER_MODE_RELIABLE, PING_CHANNEL);
		return send(packet);
	}
	Error ping() {
		auto p = PingPayload();
		return ping(p);
	}
	Dictionary collect_debug_data() {
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

    SteamConnection(CSteamID steamId);
	SteamConnection(){};
	~SteamConnection();
};

#endif // STEAM_CONNECTION_H