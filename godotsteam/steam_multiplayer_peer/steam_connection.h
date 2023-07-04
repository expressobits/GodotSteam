#ifndef STEAM_CONNECTION_H
#define STEAM_CONNECTION_H

#include <godot_cpp/classes/multiplayer_peer_extension.hpp>
//#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>
//#include <godot_cpp/templates/hash_map.hpp>

#include <memory>

#include "../godotsteam.h"
#include "steam_id.h"

#define MAX_STEAM_PACKET_SIZE k_cbMaxSteamNetworkingSocketsMessageSizeSend  //This was previously in godotsteam.h, but no longer...

class SteamConnection : public RefCounted
{
    GDCLASS(SteamConnection, RefCounted)

public:
    enum ChannelManagement {
        PING_CHANNEL,
        SIZE
    };

    struct Packet {
        uint8_t data[MAX_STEAM_PACKET_SIZE];
        uint32_t size = 0;
        SteamID sender = SteamID();
        int channel = 0;
        int transfer_mode = k_nSteamNetworkingSend_Reliable;    //Looks like a spot that might be served by an enum, eventually.
        Packet() {}
        Packet(const void *p_buffer, uint32 p_buffer_size, int transferMode, int channel) {
            ERR_FAIL_COND_MSG(p_buffer_size > MAX_STEAM_PACKET_SIZE, "Error: Tried to send a packet larger than MAX_STEAM_PACKET_SIZE");
            memcpy(this->data, p_buffer, p_buffer_size);
            this->size = p_buffer_size;
            this->sender.set_from_CSteamID(CSteamID());
            this->channel = channel;
            this->transfer_mode = transferMode;
        }
    };

    struct PingPayload {
        int peer_id = -1;
        SteamID steam_id = SteamID();
    };

    int peer_id;
    SteamID steam_id;
    uint64_t last_msg_timestamp;
    SteamNetworkingIdentity networkIdentity;

    //How is this best used with smart pointers?
    //
    //Looks like it can be done, but we'll need to look at where the packets come from, and how they're used, to determine the right kind.
    List<Packet *> pending_retry_packets;

protected:
    static void _bind_methods();

public:
    bool operator==(const SteamConnection &data);
    EResult raw_send(Packet* packet);
//    EResult raw_send(std::unique_ptr<Packet> packet);
    Error send_pending();
    void add_packet(Packet *packet);
    Error send(Packet *packet);
    Error ping(const PingPayload &p);
    Error ping();
    Dictionary collect_debug_data() const;

    String convert_eresult_to_string(EResult e);


    //SteamConnection(CSteamID steam_id);
    SteamConnection(SteamID steam_id);
    SteamConnection(){}
    ~SteamConnection();
};

#endif // STEAM_CONNECTION_H
