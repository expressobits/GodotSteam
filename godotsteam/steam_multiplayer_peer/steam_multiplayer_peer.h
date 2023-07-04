#ifndef STEAM_MULTIPLAYER_PEER_H
#define STEAM_MULTIPLAYER_PEER_H

//#include <godot_cpp/classes/wrapped.hpp>
#include <godot_cpp/classes/multiplayer_peer_extension.hpp>
//#include <godot_cpp/classes/os.hpp>
//#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/templates/hash_map.hpp>
//#include <godot_cpp/variant/dictionary.hpp>

#include "steam_connection.h"
#include "steam_id.h"
#include "../godotsteam.h"

#define STEAM_BUFFER_SIZE 255
#define MAX_TIME_WITHOUT_MESSAGE 1000   //should this be a const instead?

Dictionary steamIdToDict(SteamID input);   //What?

namespace godot {

class SteamMultiplayerPeer : public MultiplayerPeerExtension {
    GDCLASS(SteamMultiplayerPeer, MultiplayerPeerExtension)

private:
    _FORCE_INLINE_ bool _is_active() const { return lobby_state != LobbyState::LOBBY_STATE_NOT_CONNECTED; }

protected:
    static void _bind_methods();

    // Wrapped interface
//protected:
//    const GDExtensionInstanceBindingCallbacks *_get_bindings_callbacks() const override;

    // MultiplayerPeerExtension interface
public:
    static String convertEResultToString(EResult e);    //EResults are Steam Error Result codes

    Dictionary get_peer_info(int i);

    uint64_t get_steam64_from_peer_id(int peer);    //Steam64 is a Steam ID
    int get_peer_id_from_steam64(uint64_t steamid);
    int get_peer_id_from_steam_id(SteamID& steamid) const;

    Dictionary get_peer_map();

public:
    //Matchmaking call results
    CCallResult<SteamMultiplayerPeer, LobbyCreated_t> callResultCreateLobby;
    void lobby_created_scb(LobbyCreated_t *call_data, bool io_failure); //What's an SCB? I'm assuming it's something like "Steam CallBack" here.
    CCallResult<SteamMultiplayerPeer, LobbyMatchList_t> callResultLobbyList;
    void lobby_match_list_scb(LobbyMatchList_t *call_data, bool io_failure);

    SteamID lobby_id = SteamID();
    SteamID lobby_owner = SteamID();
    SteamID steam_id = SteamID();

    SteamMultiplayerPeer();
    ~SteamMultiplayerPeer();
    SteamID get_lobby_id();
    void set_lobby_id(const SteamID& new_id);

    //TODO: Double-check this list against reference
    Error _get_packet(const uint8_t **r_buffer, int32_t *r_buffer_size) override;
    Error _put_packet(const uint8_t *p_buffer, int32_t p_buffer_size) override;
    int32_t _get_available_packet_count() const override;
    int32_t _get_max_packet_size() const override;
//    PackedByteArray _get_packet_script() override;  //What the heck is a packet script? Why is it unimplemented in reference?
//    Error _put_packet_script(const PackedByteArray &p_buffer) override;
    int32_t _get_packet_channel() const override;
    MultiplayerPeer::TransferMode _get_packet_mode() const override;
//    void _set_transfer_channel(int32_t p_channel) override;
//    int32_t _get_transfer_channel() const override;
//    void _set_transfer_mode(TransferMode p_mode) override;
//    MultiplayerPeer::TransferMode _get_transfer_mode() const override;
    void _set_target_peer(int32_t p_peer_id) override;
    int32_t _get_packet_peer() const override;
    bool _is_server() const override;
    void _poll() override;
    void _close() override;
    void _disconnect_peer(int32_t p_peer, bool p_force = false) override;
    int32_t _get_unique_id() const override;
//    void _set_refuse_new_connections(bool p_enable) override;   //new_connections stuff is unimplemented in reference, but would be useful potentially
//    bool _is_refusing_new_connections() const override;
    bool _is_server_relay_supported() const override;
    MultiplayerPeer::ConnectionStatus _get_connection_status() const override;

    enum ChatChange {   //What is this?
        CHAT_CHANGE_ENTERED = k_EChatMemberStateChangeEntered,
        CHAT_CHANGE_LEFT = k_EChatMemberStateChangeLeft,
        CHAT_CHANGE_DISCONNECTED = k_EChatMemberStateChangeDisconnected,
        CHAT_CHANGE_KICKED = k_EChatMemberStateChangeKicked,
        CHAT_CHANGE_BANNED = k_EChatMemberStateChangeBanned
    };

    enum LobbyType {
        LOBBY_TYPE_PRIVATE = ELobbyType::k_ELobbyTypePrivate,
        LOBBY_TYPE_FRIENDS_ONLY = ELobbyType::k_ELobbyTypeFriendsOnly,
        LOBBY_TYPE_PUBLIC = ELobbyType::k_ELobbyTypePublic,
        LOBBY_TYPE_INVISIBLE = ELobbyType::k_ELobbyTypeInvisible
    };

    enum LobbyState {
        LOBBY_STATE_NOT_CONNECTED,
        LOBBY_STATE_HOST_PENDING,
        LOBBY_STATE_HOSTING,
        LOBBY_STATE_CLIENT_PENDING,
        LOBBY_STATE_CLIENT
    } lobby_state = LobbyState::LOBBY_STATE_NOT_CONNECTED;
    LobbyState get_state() { return lobby_state; }

    // Nagle's Algorithm: Inhibit the sending of new TCP segments, when new outgoing data arrives from the user,
    // if any previously transmitted data on the connection remains unacknowledged
    //
    // Exists to reduce small packets and avoid counterproductive overhead
    bool no_nagle = false;
    void set_no_nagle(bool value) {
        no_nagle = value;
    }
    bool get_no_nagle() {
        return no_nagle;
    }

    bool no_delay = false;  //What?
    void set_no_delay(bool value) {
        no_delay = value;
    }
    bool get_no_delay() {
        return no_delay;
    }

    bool as_relay = false;  //Again, what?
    void set_as_relay(bool value) {
        as_relay = value;
    }
    bool get_as_relay() {
        return as_relay;
    }

    int32_t target_peer = -1;
    int32_t unique_id = -1;

    // I'm not sure why he commented these out; they look usable.
    // ConnectionStatus connection_status = ConnectionStatus::CONNECTION_DISCONNECTED;
    // TransferMode transfer_mode = TransferMode::TRANSFER_MODE_RELIABLE;


    //TODO: Add steam_connection.h and include it. Note that "new" may be a problem, and Godot
    //may prefer we use a Ref<>, or failing that, Stroustrup would prefer a unique pointer of
    //some kind.
    SteamConnection::Packet *next_send_packet = new SteamConnection::Packet;
    SteamConnection::Packet *next_received_packet = new SteamConnection::Packet;    // gets deleted at the very first get_packet request
    List<SteamConnection::Packet *> incoming_packets;

    HashMap<int64_t, Ref<SteamConnection>> connections_by_steamId64;
    HashMap<int, Ref<SteamConnection>> peerId_to_steamId;

    int get_peer_by_steam_id(SteamID steam_id);
    SteamID get_steam_id_by_peer(int peer);
    void set_steam_id_peer(SteamID steam_id, int peer_id);
    Ref<SteamConnection> get_connection_by_peer(int peer_id);

    void add_connection_peer(const SteamID &steamId, int peer_id);
    void add_pending_peer(const SteamID &steamId);
    void removed_connection_peer(const SteamID &steamId);

    Error create_lobby(Steam::LobbyType lobbyType, int max_players);
    //Error join_lobby(SteamID lobbyId);
    Error join_lobby(uint64_t lobbyId);

    // This is the last part that isn't compiling. I am not 100% on what this code is meant for, but I've got a good idea.
    //
    // Note that we're getting a "No matching function" build-time exception. This may be because we do not specify callback*** in our
    // definition; but, I'm not entirely sure what type that is or what meaning it has yet.
    //
    // Could it be that we simply do not have it implemented properly, and the libraries are fine?

    STEAM_CALLBACK(SteamMultiplayerPeer, lobby_message_scb, LobbyChatMsg_t, callbackLobbyMessage);
    STEAM_CALLBACK(SteamMultiplayerPeer, lobby_chat_update_scb, LobbyChatUpdate_t, callbackLobbyChatUpdate);
    STEAM_CALLBACK(SteamMultiplayerPeer, network_messages_session_request_scb, SteamNetworkingMessagesSessionRequest_t, callbackNetworkMessagesSessionRequest);
    STEAM_CALLBACK(SteamMultiplayerPeer, network_messages_session_failed_scb, SteamNetworkingMessagesSessionFailed_t, callbackNetworkMessagesSessionFailed);
    STEAM_CALLBACK(SteamMultiplayerPeer, lobby_joined_scb, LobbyEnter_t, callbackLobbyJoined);
    STEAM_CALLBACK(SteamMultiplayerPeer, lobby_data_update_scb, LobbyDataUpdate_t, callbackLobbyDataUpdate);

    int _get_steam_transfer_flag();

    void process_message(const SteamNetworkingMessage_t *msg);
    void process_ping(const SteamNetworkingMessage_t *msg);

    Dictionary collect_debug_data() {
        Dictionary output = Dictionary();

        output["lobby_id"] = steamIdToDict(lobby_id);
        output["lobby_owner"] = steamIdToDict(lobby_owner);
        output["steam_id"] = steamIdToDict(SteamUser()->GetSteamID());
        output["lobby_state"] = lobby_state;
        output["no_nagle"] = no_nagle;
        output["no_delay"] = no_delay;
        output["as_relay"] = as_relay;
        output["target_peer"] = target_peer;
        output["unique_id"] = unique_id;

        Array connections;
        for(HashMap<int64_t, Ref<SteamConnection>>::ConstIterator E = connections_by_steamId64.begin(); E; ++E) {
            Dictionary connect = E->value->collect_debug_data();
            connections.push_back(connect);
        }
        output["connections"] = connections;

        return output;
    }

    bool send_direct_message(PackedByteArray a) {
        return SteamMatchmaking()->SendLobbyChatMsg(steam_id.data, (void *)a.ptr(), a.size());
    }

    String get_lobby_data(String key) {
        ERR_FAIL_COND_V_MSG(lobby_id.to_int() == 0, "null", "Cannot get lobby data if not in lobby");
        return SteamMatchmaking()->GetLobbyData(lobby_id.data, (const char *)key.ptr());
    }

    bool set_lobby_data(String key, String data) {
        ERR_FAIL_COND_V_MSG(lobby_id.to_int() == 0, "null", "Cannot set lobby data if not in lobby");
        return SteamMatchmaking()->SetLobbyData(lobby_id.data, (const char *)key.ptr(), (const char *)data.ptr());
    }

    Dictionary get_all_lobby_data() {
        Dictionary output;
        ERR_FAIL_COND_V_MSG(lobby_id.to_int() == 0, output, "Cannot get lobby data if not in lobby");  //Oddity: here, the second parameter is a dictionary; before, it was a string.
        int c = SteamMatchmaking()->GetLobbyDataCount(lobby_id.data);
        for(int i = 0; i < c; i++) {
            char key[STEAM_BUFFER_SIZE];    //...what?
            char value[STEAM_BUFFER_SIZE];
            SteamMatchmaking()->GetLobbyDataByIndex(lobby_id.data, i, key, STEAM_BUFFER_SIZE, value, STEAM_BUFFER_SIZE);
            output[key] = value;
        }
        return output;
    }

    void set_lobby_joinable(bool value) {
        SteamMatchmaking()->SetLobbyJoinable(lobby_id.data, value);
    }

};

//Apparently these should be removed for release builds
#define DEBUG_DATA_SIGNAL_V(msg, value) \
{ \
Dictionary a; \
a["msg"] = msg; \
a["value"] = value; \
emit_signal("debug_data", a); \
}

#define DEBUG_DATA_SIGNAL(msg) \
{ \
Dictionary a; \
a["msg"] = msg; \
emit_signal("debug_data", a); \
}

#define DEBUG_CON_DATA_SIGNAL(con, msg) \
if(unlikely(con)) { \
Dictionary a; \
a["msg"] = msg; \
emit_signal("debug_data", a); \
}

#define DEBUG_CON_DATA_SIGNAL_V(con, msg, value) \
if(unlikely(con)) { \
Dictionary a; \
a["msg"] = msg; \
a["value"] = value; \
emit_signal("debug_data", a); \
}

}

#endif
