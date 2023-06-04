#ifndef STEAM_MULTIPLAYER_PEER_H
#define STEAM_MULTIPLAYER_PEER_H

// Include Godot headers
#include <godot_cpp/classes/multiplayer_peer_extension.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/templates/hash_map.hpp>

// Steam APIs
#include "godotsteam.h"
#include "steam_connection.h"

#define MAX_TIME_WITHOUT_MESSAGE 1000

Dictionary steamIdToDict(CSteamID input);

class SteamMultiplayerPeer : public MultiplayerPeerExtension {
	GDCLASS(SteamMultiplayerPeer, MultiplayerPeerExtension);

private:
	_FORCE_INLINE_ bool _is_active() const { return lobby_state != LobbyState::LOBBY_STATE_NOT_CONNECTED; }

public:
	static String convertEResultToString(EResult e);

	Dictionary get_peer_info(int i);

	uint64_t get_steam64_from_peer_id(int peer);
	int get_peer_id_from_steam64(uint64_t steamid);
	Dictionary get_peer_map();

public:
	// Matchmaking call results ///////////// stolen
	CCallResult<SteamMultiplayerPeer, LobbyCreated_t> callResultCreateLobby;
	void lobby_created_scb(LobbyCreated_t *call_data, bool io_failure);
	CCallResult<SteamMultiplayerPeer, LobbyMatchList_t> callResultLobbyList;
	void lobby_match_list_scb(LobbyMatchList_t *call_data, bool io_failure);

	CSteamID lobby_id = CSteamID();
	CSteamID lobby_owner = CSteamID();
	CSteamID steam_id = CSteamID();

	SteamMultiplayerPeer();
	~SteamMultiplayerPeer();
	uint64 get_lobby_id();

	static void _bind_methods();

	// MultiplayerPeer stuff
	virtual Error _get_packet(const uint8_t * *r_buffer, int32_t *r_buffer_size) override;
	virtual Error _put_packet(const uint8_t *p_buffer, int p_buffer_size) override;
	virtual int32_t _get_available_packet_count() const override;
	virtual int32_t _get_max_packet_size() const override;
	virtual int32_t _get_packet_channel() const override;
	virtual MultiplayerPeer::TransferMode _get_packet_mode() const override;
	virtual void _set_target_peer(int32_t p_peer_id) override;
	virtual int32_t _get_packet_peer() const override;
	virtual bool _is_server() const override;
	virtual void _poll() override;
	virtual void _close() override;
	virtual void _disconnect_peer(int32_t p_peer, bool p_force = false) override;
	virtual int32_t _get_unique_id() const override;
	virtual bool _is_server_relay_supported() const override;
	virtual MultiplayerPeer::ConnectionStatus _get_connection_status() const override;

	enum ChatChange {
		CHAT_CHANGE_ENTERED = k_EChatMemberStateChangeEntered,
		CHAT_CHANGE_LEFT = k_EChatMemberStateChangeLeft,
		CHAT_CHANGE_DISCONNECTED = k_EChatMemberStateChangeDisconnected,
		CHAT_CHANGE_KICKED = k_EChatMemberStateChangeKicked,
		CHAT_CHANGE_BANNED = k_EChatMemberStateChangeBanned
	};

	// all SteamGodot from here on down
	enum LobbyState {
		LOBBY_STATE_NOT_CONNECTED,
		LOBBY_STATE_HOST_PENDING,
		LOBBY_STATE_HOSTING,
		LOBBY_STATE_CLIENT_PENDING,
		LOBBY_STATE_CLIENT
	} lobby_state = LobbyState::LOBBY_STATE_NOT_CONNECTED;
	LobbyState get_state() { return lobby_state; }

	bool no_nagle = false;
	void set_no_nagle(bool value) {
		no_nagle = value;
	}
	bool get_no_nagle() {
		return no_nagle;
	}
	bool no_delay = false;
	void set_no_delay(bool value) {
		no_delay = value;
	}
	bool get_no_delay() {
		return no_delay;
	}
	bool as_relay = false;
	void set_as_relay(bool value) {
		as_relay = value;
	}
	bool get_as_relay() {
		return as_relay;
	}

	int32_t target_peer = -1;
	int32_t unique_id = -1;
	// ConnectionStatus connection_status = ConnectionStatus::CONNECTION_DISCONNECTED;
	// TransferMode transfer_mode = TransferMode::TRANSFER_MODE_RELIABLE;

	
	SteamConnection::Packet *next_send_packet = new SteamConnection::Packet;
	SteamConnection::Packet *next_received_packet = new SteamConnection::Packet; // this packet gets deleted at the first get_packet request
	List<SteamConnection::Packet *> incoming_packets;

	HashMap<int64_t, Ref<SteamConnection>> connections_by_steamId64;
	HashMap<int, Ref<SteamConnection>> peerId_to_steamId;

	int get_peer_by_steam_id(CSteamID steamId);
	CSteamID get_steam_id_by_peer(int peer);
	void set_steam_id_peer(CSteamID steamId, int peer_id);
	Ref<SteamConnection> get_connection_by_peer(int peer_id);

	void add_connection_peer(const CSteamID &steamId, int peer_id);
	void add_pending_peer(const CSteamID &steamId);
	void removed_connection_peer(const CSteamID &steamId);

	Error create_lobby(Steam::LobbyType lobbyType, int max_players);
	Error join_lobby(uint64 lobbyId);

	STEAM_CALLBACK(SteamMultiplayerPeer, lobby_message_scb, LobbyChatMsg_t, callbackLobbyMessage);
	STEAM_CALLBACK(SteamMultiplayerPeer, lobby_chat_update_scb, LobbyChatUpdate_t, callbackLobbyChatUpdate);
	STEAM_CALLBACK(SteamMultiplayerPeer, network_messages_session_request_scb, SteamNetworkingMessagesSessionRequest_t, callbackNetworkMessagesSessionRequest);
	STEAM_CALLBACK(SteamMultiplayerPeer, network_messages_session_failed_scb, SteamNetworkingMessagesSessionFailed_t, callbackNetworkMessagesSessionFailed);
	STEAM_CALLBACK(SteamMultiplayerPeer, lobby_joined_scb, LobbyEnter_t, callbackLobbyJoined);
	STEAM_CALLBACK(SteamMultiplayerPeer, lobby_data_update_scb, LobbyDataUpdate_t, callbackLobbyDataUpdate);

	int _get_steam_transfer_flag();

	void process_message(const SteamNetworkingMessage_t *msg);
	void process_ping(const SteamNetworkingMessage_t *msg);
	// void poll_channel(int nLocalChannel, void (*func)(SteamNetworkingMessage_t));

	Dictionary collect_debug_data() {
		auto output = Dictionary();

		output["lobby_id"] = steamIdToDict(lobby_id);
		output["lobby_owner"] = steamIdToDict(lobby_owner);
		output["steam_id"] = steamIdToDict(SteamUser()->GetSteamID());
		output["lobby_state"] = lobby_state;
		output["no_nagle"] = no_nagle;
		output["no_delay"] = no_delay;
		output["target_peer"] = target_peer;
		output["unique_id"] = unique_id;

		Array connections;
		for (auto E = connections_by_steamId64.begin(); E; ++E) {
			auto qwer = E->value->collect_debug_data();
			connections.push_back(qwer);
		}
		output["connections"] = connections;

		return output;
	}
	bool send_direct_message(PackedByteArray a) {
		return SteamMatchmaking()->SendLobbyChatMsg(steam_id, (void *)a.ptr(), a.size());
	}
	Array get_direct_messages() {
		Array output;
		return output;
	}

	String get_lobby_data(String key) {
		ERR_FAIL_COND_V_MSG(lobby_id.ConvertToUint64() == 0, "null", "CANNOT GET LOBBY DATA IF NOT IN LOBBY");
		return SteamMatchmaking()->GetLobbyData(lobby_id, (const char *)key.ptr());
		// String output(a);
		// return a;
	}
	bool set_lobby_data(String key, String data) {
		ERR_FAIL_COND_V_MSG(lobby_id.ConvertToUint64() == 0, false, "CANNOT SET LOBBY DATA IF NOT IN LOBBY");
		return SteamMatchmaking()->SetLobbyData(lobby_id, (const char *)key.ptr(), (const char *)data.ptr());
	}
	Dictionary get_all_lobby_data() {
		Dictionary output;
		ERR_FAIL_COND_V_MSG(lobby_id.ConvertToUint64() == 0, output, "CANNOT GET LOBBY DATA IF NOT IN LOBBY");
		auto c = SteamMatchmaking()->GetLobbyDataCount(lobby_id);
		for (int i = 0; i < c; i++) {
			char key[STEAM_BUFFER_SIZE];
			char value[STEAM_BUFFER_SIZE];
			SteamMatchmaking()->GetLobbyDataByIndex(lobby_id, i, key, STEAM_BUFFER_SIZE, value, STEAM_BUFFER_SIZE);
			output[key] = value;
		}
		return output;
	}
	void set_lobby_joinable(bool value){
		SteamMatchmaking()->SetLobbyJoinable(lobby_id,value);
	}
};

// todo: make these empty for release builds
#define DEBUG_DATA_SIGNAL_V(msg, value) \
	{                                   \
		Dictionary a;                   \
		a["msg"] = msg;                 \
		a["value"] = value;             \
		emit_signal("debug_data", a);   \
	}

#define DEBUG_DATA_SIGNAL(msg)        \
	{                                 \
		Dictionary a;                 \
		a["msg"] = msg;               \
		emit_signal("debug_data", a); \
	}

#define DEBUG_CON_DATA_SIGNAL(con, msg) \
	if (unlikely(con)) {                \
		Dictionary a;                   \
		a["msg"] = msg;                 \
		emit_signal("debug_data", a);   \
	}

#define DEBUG_CON_DATA_SIGNAL_V(con, msg, value) \
	if (unlikely(con)) {                         \
		Dictionary a;                            \
		a["msg"] = msg;                          \
		a["value"] = value;                      \
		emit_signal("debug_data", a);            \
	}

#endif
