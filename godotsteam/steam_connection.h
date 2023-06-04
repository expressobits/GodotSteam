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
	bool operator==(const SteamConnection &data);
	EResult rawSend(Packet *packet);
	Error sendPending();
	void addPacket(Packet *packet);
	Error send(Packet *packet);
	Error ping(const PingPayload &p);
	Error ping();
	Dictionary collect_debug_data();

    SteamConnection(CSteamID steamId);
	SteamConnection(){};
	~SteamConnection();
};

#endif // STEAM_CONNECTION_H