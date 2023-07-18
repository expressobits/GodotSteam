/*
 * Notes:
 *
 * I had to convert unsigned long long ints from Steam, in several places, to unsigned long ints, in order to fit them into a Variant... which feels like a bad idea.
 * ^ I have added Green Fox's SteamID class to manage this issue; it should now be implemented properly.
 */

#include "steam_multiplayer_peer.h"
#include "steam_id.h"
#include <godot_cpp/core/class_db.hpp>
#include "../godotsteam.h"

VARIANT_ENUM_CAST(SteamMultiplayerPeer::ChatChange)
VARIANT_ENUM_CAST(SteamMultiplayerPeer::LobbyState)
VARIANT_ENUM_CAST(SteamMultiplayerPeer::LobbyType);

using namespace godot;

void SteamMultiplayerPeer::_set_target_peer(int32_t p_peer)
{
    target_peer = p_peer;
}

int32_t SteamMultiplayerPeer::_get_packet_peer() const
{
    ERR_FAIL_COND_V_MSG(!_is_active(), 1, "The multiplayer instance isn't currently active.");
    ERR_FAIL_COND_V_MSG(incoming_packets.size() == 0, 1, "No packets to receive.");

    return connections_by_steamId64[incoming_packets.front()->get()->sender.to_int()]->peer_id;
}

MultiplayerPeer::TransferMode SteamMultiplayerPeer::_get_packet_mode() const
{
    ERR_FAIL_COND_V_MSG(!_is_active(), TRANSFER_MODE_RELIABLE, "The multiplayer instance isn't currently active.");
    ERR_FAIL_COND_V_MSG(incoming_packets.size() == 0, TRANSFER_MODE_RELIABLE, "No pending packets, cannot get transfer mode.");

    if(incoming_packets.front()->get()->transfer_mode & k_nSteamNetworkingSend_Reliable) {
        return TRANSFER_MODE_RELIABLE;
    } else {
        return TRANSFER_MODE_UNRELIABLE;
    }
}

int32_t SteamMultiplayerPeer::_get_packet_channel() const
{
    ERR_FAIL_COND_V_MSG(!_is_active(), TRANSFER_MODE_RELIABLE, "The multiplayer instance isn't currently active.");
    ERR_FAIL_COND_V_MSG(incoming_packets.size() == 0, TRANSFER_MODE_RELIABLE, "No pending packets, cannot get channel.");

    return incoming_packets.front()->get()->channel;
}

Error SteamMultiplayerPeer::create_lobby(Steam::LobbyType lobbyType, int max_players) {
    ERR_FAIL_COND_V_MSG(SteamMatchmaking() == NULL, ERR_DOES_NOT_EXIST, "`SteamMatchmaking()` is null.");
    ERR_FAIL_COND_V_MSG(lobby_state != LobbyState::LOBBY_STATE_NOT_CONNECTED, ERR_ALREADY_IN_USE, "Cannot create a lobby, while in a lobby.");

    lobby_state = LobbyState::LOBBY_STATE_HOST_PENDING;
    SteamAPICall_t api_call = SteamMatchmaking()->CreateLobby((ELobbyType)lobbyType, max_players);
    callResultCreateLobby.Set(api_call, this, &SteamMultiplayerPeer::lobby_created_scb);
    unique_id = 1;
    return OK;
}

//Error SteamMultiplayerPeer::join_lobby(SteamID lobbyId) {
Error SteamMultiplayerPeer::join_lobby(uint64_t lobbyId) {
    ERR_FAIL_COND_V_MSG(SteamMatchmaking() == NULL, ERR_DOES_NOT_EXIST, "`SteamMatchmaking()` is null.");
    ERR_FAIL_COND_V_MSG(lobby_state != LobbyState::LOBBY_STATE_NOT_CONNECTED, ERR_ALREADY_IN_USE, "Cannot join a lobby, while in a lobby.");

    if(SteamMatchmaking() != NULL)  //Is this necessary?
    {
        lobby_state = LobbyState::LOBBY_STATE_CLIENT_PENDING;
        this->lobby_id.set_from_int(lobbyId);
        //this->set_lobby_id(lobbyId);
        unique_id = generate_unique_id();
        SteamMatchmaking()->JoinLobby(lobby_id.data);
    }
    return OK;
}

#define MAX_MESSAGE_COUNT 255
void SteamMultiplayerPeer::_poll()
{
    ERR_FAIL_COND_MSG(!_is_active(), "The multiplayer instance isn't currently active.");
    {
        SteamNetworkingMessage_t *messages[MAX_MESSAGE_COUNT];
        int count = SteamNetworkingMessages()->ReceiveMessagesOnChannel(SteamConnection::ChannelManagement::SIZE, messages, MAX_MESSAGE_COUNT);
        for(int i = 0; i < count; i++) {
            SteamNetworkingMessage_t *msg = messages[i];
            process_message(msg);
            msg->Release();
        }
    }
    {
        SteamConnection::PingPayload a = SteamConnection::PingPayload();
        for(HashMap<int64_t, Ref<SteamConnection>>::ConstIterator E = connections_by_steamId64.begin(); E; ++E) {
            int64_t key = E->key;
            Ref<SteamConnection> value = E->value;
            uint64_t t = value->last_msg_timestamp + MAX_TIME_WITHOUT_MESSAGE;  //original is pretty sure this will "wrap", but is uncertain if it should be fixed.

            if(value->peer_id == -1 || t < Time::get_singleton()->get_ticks_msec()) {
                value->ping(a);
            }
        }
    }
    {
        SteamNetworkingMessage_t *messages[MAX_MESSAGE_COUNT];
        int count = SteamNetworkingMessages()->ReceiveMessagesOnChannel(SteamConnection::ChannelManagement::PING_CHANNEL, messages, MAX_MESSAGE_COUNT);
        for(int i = 0; i < count; i++) {
            SteamNetworkingMessage_t *msg = messages[i];
            process_ping(msg);
            msg->Release();
        }
    }
}

bool SteamMultiplayerPeer::_is_server() const
{
    return unique_id == 1;
}

bool SteamMultiplayerPeer::_is_server_relay_supported() const   //This provides some evidence of what "as_relay" stands for, but I'm not certain what a server relay is exactly.
{
    return as_relay;
}

void SteamMultiplayerPeer::_disconnect_peer(int32_t p_peer, bool p_force)   //This is a TODO.
{
    Steam::get_singleton()->steamworksError("ERROR: SteamMultiplayerPeer::_disconnect_peer not yet implemented");
}

void SteamMultiplayerPeer::_close()
{
    if(!_is_active()) return;

    SteamMatchmaking()->LeaveLobby(lobby_id.data);
    lobby_state = LOBBY_STATE_NOT_CONNECTED;
    peerId_to_steamId.clear();
    connections_by_steamId64.clear();
    lobby_id.set_from_CSteamID(CSteamID()); // = SteamID();
    lobby_owner.set_from_CSteamID(CSteamID()); // = SteamID();
    steam_id.set_from_CSteamID(CSteamID()); // = SteamID();
}

int32_t SteamMultiplayerPeer::_get_available_packet_count() const
{
    return incoming_packets.size();
}

Error SteamMultiplayerPeer::_get_packet(const uint8_t **r_buffer, int32_t *r_buffer_size)
{
    ERR_FAIL_COND_V_MSG(incoming_packets.size() == 0, ERR_UNAVAILABLE, "No incoming packets available.");

    delete next_received_packet;
    next_received_packet = incoming_packets.front()->get();
    *r_buffer_size = next_received_packet->size;
    *r_buffer = (const uint8_t *)(&next_received_packet->data); //A pointer to a reference? I feel like this is worthy of more consideration.
    incoming_packets.pop_front();

    return OK;
}

Error SteamMultiplayerPeer::_put_packet(const uint8_t *p_buffer, int32_t p_buffer_size)
{
    int transferMode = _get_steam_transfer_flag();
    int32_t channel = get_transfer_channel() + SteamConnection::ChannelManagement::SIZE;

    if(target_peer == 0) {
        Error returnValue = OK;
        for(HashMap<int64_t, Ref<SteamConnection>>::Iterator E = connections_by_steamId64.begin(); E; ++E) {
            SteamConnection::Packet* packet = new SteamConnection::Packet(p_buffer, p_buffer_size, transferMode, channel);
            Error errorCode = E->value->send(packet);
            if(errorCode != OK) {
                DEBUG_DATA_SIGNAL_V("put_packet failed.", errorCode);
                returnValue = errorCode;
            }
        }
        return returnValue;
    } else {
        SteamConnection::Packet* packet = new SteamConnection::Packet(p_buffer, p_buffer_size, transferMode, channel);
        return get_connection_by_peer(target_peer)->send(packet);
    }
}

int32_t SteamMultiplayerPeer::_get_max_packet_size() const
{
    return k_cbMaxSteamNetworkingSocketsMessageSizeSend;
}

MultiplayerPeer::ConnectionStatus SteamMultiplayerPeer::_get_connection_status() const
{
    //There's a strong argument for a switch case here.

    if(lobby_state == LobbyState::LOBBY_STATE_NOT_CONNECTED) {
        return ConnectionStatus::CONNECTION_DISCONNECTED;
    } else if(lobby_state == LobbyState::LOBBY_STATE_CLIENT || lobby_state == LobbyState::LOBBY_STATE_HOSTING) {
        return ConnectionStatus::CONNECTION_CONNECTED;
    } else {
        return ConnectionStatus::CONNECTION_CONNECTING;
    }
}

int32_t SteamMultiplayerPeer::_get_unique_id() const
{
    ERR_FAIL_COND_V_MSG(!_is_active(), 0, "The multiplayer instance isn't currently active.");
    return unique_id;
}

int SteamMultiplayerPeer::get_peer_by_steam_id(SteamID steam_id) {
    ERR_FAIL_COND_V_MSG(connections_by_steamId64.has(steam_id.to_int()) == false, -1, "Steam ID not connected");
    return connections_by_steamId64[steam_id.to_int()]->peer_id;
}

SteamID SteamMultiplayerPeer::get_steam_id_by_peer(int peer) {
    ERR_FAIL_COND_V_MSG(peerId_to_steamId.has(peer) == false, SteamID(), "Peer does not exist");
    return peerId_to_steamId[peer]->steam_id;
}

//Hmmm... this should probably be a uint64_t, which is an unrelated type guaranteed to be 64 bits long; as opposed to non-standard uint64.
SteamID SteamMultiplayerPeer::get_lobby_id() {
    return lobby_id;
}

void SteamMultiplayerPeer::set_lobby_id(const SteamID& new_id) {
    this->lobby_id.set_from_CSteamID(new_id.data); // = new_id;
}

int SteamMultiplayerPeer::_get_steam_transfer_flag() {
    MultiplayerPeer::TransferMode transfer_mode = get_transfer_mode();

    int32_t flags = (k_nSteamNetworkingSend_NoNagle * no_nagle) | (k_nSteamNetworkingSend_NoDelay * no_delay);//interesting use

    switch(transfer_mode) {
    case TransferMode::TRANSFER_MODE_RELIABLE:
        return k_nSteamNetworkingSend_Reliable | flags;
        break;
    case TransferMode::TRANSFER_MODE_UNRELIABLE:
        return k_nSteamNetworkingSend_Unreliable | flags;
        break;
    case TransferMode::TRANSFER_MODE_UNRELIABLE_ORDERED:
        //Unreliable order not supported here!
        return k_nSteamNetworkingSend_Reliable | flags;
        break;
    }

    ERR_FAIL_V_MSG(-1, "Flags error. Switch fallthrough in _get_steam_transfer_flag");
}

void SteamMultiplayerPeer::process_message(const SteamNetworkingMessage_t *msg) {
    ERR_FAIL_COND_MSG(msg->GetSize() > MAX_STEAM_PACKET_SIZE, "Packet too large to send,");

    SteamConnection::Packet* packet = new SteamConnection::Packet;
    packet->channel = 0;
    packet->sender.set_from_CSteamID(msg->m_identityPeer.GetSteamID());
    packet->size = msg->GetSize();
    packet->transfer_mode = -1;

    uint8_t* rawData = (uint8_t *)msg->GetData();
    memcpy(packet->data, rawData, packet->size);
    incoming_packets.push_back(packet);
}

void SteamMultiplayerPeer::process_ping(const SteamNetworkingMessage_t *msg) {
    if(sizeof(SteamConnection::PingPayload) != msg->GetSize()) {
        Steam::get_singleton()->steamworksError("Payload is the wrong size for a ping.");
        return;
    }

    SteamConnection::PingPayload* data = (SteamConnection::PingPayload *)msg->GetData();
    if(data->peer_id == -1) {
        //response to ping
        SteamConnection::PingPayload p = SteamConnection::PingPayload();
        p.peer_id = unique_id;
        p.steam_id.set_from_CSteamID(SteamUser()->GetSteamID());
        Error err = connections_by_steamId64[msg->m_identityPeer.GetSteamID64()]->ping(p);
        if(err != OK) {
            DEBUG_DATA_SIGNAL_V("Process Ping: ping failed.", err);
        }
    } else {
        Ref<SteamConnection> connection = connections_by_steamId64[data->steam_id.to_int()];
        if(connection->peer_id == -1) {
            set_steam_id_peer(data->steam_id, data->peer_id);
        }
        //collect ping data
    }
}

void SteamMultiplayerPeer::set_steam_id_peer(SteamID steam_id, int peer_id) {
    ERR_FAIL_COND_MSG(connections_by_steamId64.has(steam_id.to_int()) == false, "Steam ID missing");

    Ref<SteamConnection> con = connections_by_steamId64[steam_id.to_int()];
    if(con->peer_id == -1) {
        con->peer_id = peer_id;
        peerId_to_steamId[peer_id] = con;
        emit_signal("peer_connected", peer_id);
    } else if(con->peer_id == peer_id) {
        //peer already exists, so nothing happens
    } else {
        DEBUG_DATA_SIGNAL_V("Steam ID detected with wrong peer ID: ", (long int)steam_id.to_int());
        DEBUG_DATA_SIGNAL_V("Peer ID was: ", con->peer_id);
        DEBUG_DATA_SIGNAL_V("Trying to set as: ", peer_id);
    }
}

Ref<SteamConnection> SteamMultiplayerPeer::get_connection_by_peer(int peer_id) {
    if(peerId_to_steamId.has(peer_id))
        return peerId_to_steamId[peer_id];

    return nullptr;
}

void SteamMultiplayerPeer::add_connection_peer(const SteamID &steamId, int peer_id) {
    ERR_FAIL_COND_MSG(steamId == SteamUser()->GetSteamID(), "Cannot add self as a new peer.");

    Ref<SteamConnection> connectionData = Ref<SteamConnection>(memnew(SteamConnection(steamId)));
    connections_by_steamId64[steamId.to_int()] = connectionData;

    Error a = connectionData->ping();

    if(a != OK) {
        DEBUG_DATA_SIGNAL_V("add_connection_peer: Error sending ping. ", a);    //shouldn't this be DEBUG_DATA_COND_SIGNAL_V or something like that?
    }
    ERR_FAIL_COND_MSG(a != OK, "Message failed to join.");
}

void SteamMultiplayerPeer::add_pending_peer(const SteamID &steamId) {
    add_connection_peer(steamId, -1);
}

void SteamMultiplayerPeer::removed_connection_peer(const SteamID &steamId) {
    int peerId = get_peer_by_steam_id(steamId);
    peerId_to_steamId.erase(peerId);

    emit_signal("peer_disconnected", peerId);
    connections_by_steamId64.erase(steamId.to_int());
}

void SteamMultiplayerPeer::lobby_created_scb(LobbyCreated_t *call_data, bool io_failure) {
    if(io_failure) {
        lobby_state = LobbyState::LOBBY_STATE_NOT_CONNECTED;
        Steam::get_singleton()->steamworksError("lobby_created failed.");
    } else {
        lobby_state = LobbyState::LOBBY_STATE_HOSTING;
        int connect = call_data->m_eResult;
        lobby_id.set_from_CSteamID(call_data->m_ulSteamIDLobby);
        uint64_t lobby = lobby_id.to_int();
        emit_signal("lobby_created", connect, lobby);   //Orig is not entirely sure why he's doing this. Worth looking up.
    }
}

void SteamMultiplayerPeer::lobby_message_scb(LobbyChatMsg_t *call_data) {
    if(lobby_id.data != call_data->m_ulSteamIDLobby) {
        DEBUG_DATA_SIGNAL("lobby_message_scb: received a message for a different lobby.")
        return;
    }

    SteamConnection::Packet *packet = new SteamConnection::Packet;

    packet->sender.set_from_int(call_data->m_ulSteamIDUser);
    if(SteamUser()->GetSteamID() == packet->sender.data)
        return;

    //These two following lines could probably be merged...
    uint8 chat_type = call_data->m_eChatEntryType;

    //convert chat type to target type
    EChatEntryType type = (EChatEntryType)chat_type;

    //Get chat message data
    packet->size = SteamMatchmaking()->GetLobbyChatEntry(lobby_id.data, call_data->m_iChatID, &(packet->sender.data), &(packet->data), MAX_STEAM_PACKET_SIZE, &type);
    packet->channel = -1;


    incoming_packets.push_back(packet);
}

//Should occur when someone joins, or leaves
void SteamMultiplayerPeer::lobby_chat_update_scb(LobbyChatUpdate_t *call_data) {
    if(lobby_id.data != call_data->m_ulSteamIDLobby)
        return;

    SteamID userChanged = SteamID(call_data->m_ulSteamIDUserChanged);

    switch(ChatChange(call_data->m_rgfChatMemberStateChange)) {
    case ChatChange::CHAT_CHANGE_ENTERED:
        if (userChanged != SteamUser()->GetSteamID()) {
            add_pending_peer(userChanged);
        }
        break;

    case ChatChange::CHAT_CHANGE_LEFT:
    case ChatChange::CHAT_CHANGE_DISCONNECTED:
    case ChatChange::CHAT_CHANGE_KICKED:
    case ChatChange::CHAT_CHANGE_BANNED:
        if(userChanged != SteamUser()->GetSteamID()) {
            removed_connection_peer(userChanged);
        }
        break;
    default:
        ERR_PRINT("Fell through switch in lobby_chat_update_scb");
    }
}

//This happens when a message update request is received from another user
void SteamMultiplayerPeer::network_messages_session_request_scb(SteamNetworkingMessagesSessionRequest_t *t) {
    DEBUG_CON_DATA_SIGNAL(get_state() != LOBBY_STATE_HOSTING && get_state() != LOBBY_STATE_CLIENT, "Received connection before properly in a lobby.");

    CSteamID requester = t->m_identityRemote.GetSteamID();
    int currentLobbySize = SteamMatchmaking()->GetNumLobbyMembers(lobby_id.data);
    for(int i = 0; i < currentLobbySize; i++) {
        if(SteamMatchmaking()->GetLobbyMemberByIndex(lobby_id.data, i) == requester) {
            bool didWork = SteamNetworkingMessages()->AcceptSessionWithUser(t->m_identityRemote);
            ERR_FAIL_COND_MSG(didWork, "Message failed to join.");
            return;
        }
    }

    ERR_PRINT(String("Connection attempted by a player not in lobby:") + String::num_uint64(requester.GetAccountID()));
}

void SteamMultiplayerPeer::network_messages_session_failed_scb(SteamNetworkingMessagesSessionFailed_t *call_data) {
    SteamNetConnectionInfo_t info = call_data->m_info;

    //Parse out the reason for failure
    DEBUG_DATA_SIGNAL_V("Error: network_messages_session_failed_scb", info.m_eEndReason);
}

//What is it that this function is supposed to do? It doesn't look like it does anything.
void SteamMultiplayerPeer::lobby_data_update_scb(LobbyDataUpdate_t *call_data) {
    uint64_t member_id = call_data->m_ulSteamIDMember;
    uint64_t lobby_id = call_data->m_ulSteamIDLobby;
    uint8 success = call_data->m_bSuccess;
    return;
}

void SteamMultiplayerPeer::lobby_joined_scb(LobbyEnter_t *lobbyData) {
    if(lobbyData->m_ulSteamIDLobby != this->lobby_id.to_int())
        //joined a lobby that isn't this one, though apparently there are cases requiring presence in more than one lobby
        return;

    if(lobbyData->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess) {
        ISteamMatchmaking* sm = SteamMatchmaking();   //Is this meant to be a singleton?

        lobby_owner.set_from_CSteamID(sm->GetLobbyOwner(lobby_id.data));
        if(unique_id == 1) {
            //already the hose, no need to proceed further
        } else {
            lobby_state = LobbyState::LOBBY_STATE_CLIENT;
            add_pending_peer(lobby_owner);

            int count = sm->GetNumLobbyMembers(lobby_id.data);
            for(int i = 0; i < count; i++) {
                SteamID member = sm->GetLobbyMemberByIndex(lobby_id.data, i);
                if(member != SteamUser()->GetSteamID() && member != lobby_owner) {
                    add_pending_peer(member);
                }
            }
        }
    } else {
        String output = "";
        switch (lobbyData->m_EChatRoomEnterResponse) {
        // k_EChatRoomEnterResponseSuccess: 			output = "k_EChatRoomEnterResponseSuccess"; break;
        case k_EChatRoomEnterResponseDoesntExist:
            output = "Doesn't Exist";
            break;
        case k_EChatRoomEnterResponseNotAllowed:
            output = "Not Allowed";
            break;
        case k_EChatRoomEnterResponseFull:
            output = "Full";
            break;
        case k_EChatRoomEnterResponseError:
            output = "Error";
            break;
        case k_EChatRoomEnterResponseBanned:
            output = "Banned";
            break;
        case k_EChatRoomEnterResponseLimited:
            output = "Limited";
            break;
        case k_EChatRoomEnterResponseClanDisabled:
            output = "Clan Disabled";
            break;
        case k_EChatRoomEnterResponseCommunityBan:
            output = "Community Ban";
            break;
        case k_EChatRoomEnterResponseMemberBlockedYou:
            output = "Member Blocked You";
            break;
        case k_EChatRoomEnterResponseYouBlockedMember:
            output = "You Blocked Member";
            break;
        case k_EChatRoomEnterResponseRatelimitExceeded:
            output = "Ratelimit Exceeded";
            break;
        }

        if(output.length() != 0) {
            ERR_PRINT("Joined lobby failed. " + output);
            lobby_state = LobbyState::LOBBY_STATE_NOT_CONNECTED;
            DEBUG_DATA_SIGNAL_V(output, lobbyData->m_EChatRoomEnterResponse);
            return;
        }
    }
}

Dictionary steamIdToDict(SteamID input) {
    Dictionary output = Dictionary();
    output["GetAccountID"] = input.data.GetAccountID();
    output["GetUnAccountInstance"] = input.data.GetUnAccountInstance();
    output["GetEAccountType"] = input.data.GetEAccountType();
    output["GetEUniverse"] = input.data.GetEUniverse();
    return output;
}

Dictionary SteamMultiplayerPeer::get_peer_info(int i) {
    return peerId_to_steamId[i]->collect_debug_data();
}

uint64_t SteamMultiplayerPeer::get_steam64_from_peer_id(int peer) {
    if(peer == this->unique_id) {
        return SteamUser()->GetSteamID().ConvertToUint64();
    } else if(peerId_to_steamId.find(peer) == peerId_to_steamId.end()) {
        return peerId_to_steamId[peer]->steam_id.to_int();
    } else return -1;
}

int SteamMultiplayerPeer::get_peer_id_from_steam64(uint64_t steamid) {
    if(steamid == SteamUser()->GetSteamID().ConvertToUint64()) {
        return this->unique_id;
    } else if(connections_by_steamId64.has(steamid)) {
        return connections_by_steamId64[steamid]->peer_id;
    } else return -1;
}

//Should this be by reference?
int SteamMultiplayerPeer::get_peer_id_from_steam_id(SteamID& steamid) const {
    if(steamid == SteamID(SteamUser()->GetSteamID())) {
        return this->unique_id;
    } else if(connections_by_steamId64.has(steamid.to_int())) {
        return connections_by_steamId64[steamid.to_int()]->peer_id;
    } else return -1;
}

Dictionary SteamMultiplayerPeer::get_peer_map() {
    Dictionary output;
    for(HashMap<int64_t, Ref<SteamConnection>>::ConstIterator E = connections_by_steamId64.begin(); E; ++E) {
        output[E->value->peer_id] = E->value->steam_id.to_int();
    }
    return output;
}

void SteamMultiplayerPeer::_bind_methods() {
    //DEFVAL simply surrounds teh value in parentheses to avoid repeating a default value multiple times, overriding operator precedence
    ClassDB::bind_method(D_METHOD("create_lobby", "lobby_type", "max_players"), &SteamMultiplayerPeer::create_lobby, DEFVAL(32));
    ClassDB::bind_method(D_METHOD("connect_lobby", "lobby_id"), &SteamMultiplayerPeer::join_lobby);
    ClassDB::bind_method(D_METHOD("get_state"), &SteamMultiplayerPeer::get_state);
    ClassDB::bind_method(D_METHOD("collect_debug_data"), &SteamMultiplayerPeer::collect_debug_data);

    ClassDB::bind_method(D_METHOD("get_no_nagle"), &SteamMultiplayerPeer::get_no_nagle);
    ClassDB::bind_method(D_METHOD("set_no_nagle"), &SteamMultiplayerPeer::set_no_nagle);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "no_nagle"), "set_no_nagle", "get_no_nagle");

    ClassDB::bind_method(D_METHOD("get_no_delay"), &SteamMultiplayerPeer::get_no_delay);
    ClassDB::bind_method(D_METHOD("set_no_delay"), &SteamMultiplayerPeer::set_no_delay);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "no_delay"), "set_no_delay", "get_no_delay");

    ClassDB::bind_method(D_METHOD("get_as_relay"), &SteamMultiplayerPeer::get_as_relay);
    ClassDB::bind_method(D_METHOD("set_as_relay"), &SteamMultiplayerPeer::set_as_relay);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "as_relay"), "set_as_relay", "get_as_relay");

    ClassDB::bind_method(D_METHOD("get_steam64_from_peer_id", "peer_id"), &SteamMultiplayerPeer::get_steam64_from_peer_id);
    ClassDB::bind_method(D_METHOD("get_peer_id_from_steam64", "steam_id"), &SteamMultiplayerPeer::get_peer_id_from_steam64);
    ClassDB::bind_method(D_METHOD("get_peer_map"), &SteamMultiplayerPeer::get_peer_map);

    ClassDB::bind_method(D_METHOD("send_direct_message", "message"), &SteamMultiplayerPeer::send_direct_message);
    ClassDB::bind_method(D_METHOD("get_lobby_data", "key"), &SteamMultiplayerPeer::get_lobby_data);
    ClassDB::bind_method(D_METHOD("set_lobby_data", "key", "value"), &SteamMultiplayerPeer::set_lobby_data);
    ClassDB::bind_method(D_METHOD("get_all_lobby_data"), &SteamMultiplayerPeer::get_all_lobby_data);

    ClassDB::bind_method(D_METHOD("set_lobby_joinable", "joinable"), &SteamMultiplayerPeer::set_lobby_joinable);

    //Next up, bind enums

    //LobbyType
    BIND_ENUM_CONSTANT(LOBBY_TYPE_PRIVATE)
    BIND_ENUM_CONSTANT(LOBBY_TYPE_FRIENDS_ONLY)
    BIND_ENUM_CONSTANT(LOBBY_TYPE_PUBLIC)
    BIND_ENUM_CONSTANT(LOBBY_TYPE_INVISIBLE)

    //ChatChange
    BIND_ENUM_CONSTANT(CHAT_CHANGE_ENTERED)
    BIND_ENUM_CONSTANT(CHAT_CHANGE_LEFT)
    BIND_ENUM_CONSTANT(CHAT_CHANGE_DISCONNECTED)
    BIND_ENUM_CONSTANT(CHAT_CHANGE_KICKED)
    BIND_ENUM_CONSTANT(CHAT_CHANGE_BANNED)

    //LobbyState
    BIND_ENUM_CONSTANT(LOBBY_STATE_NOT_CONNECTED)
    BIND_ENUM_CONSTANT(LOBBY_STATE_HOST_PENDING)
    BIND_ENUM_CONSTANT(LOBBY_STATE_HOSTING)
    BIND_ENUM_CONSTANT(LOBBY_STATE_CLIENT_PENDING)
    BIND_ENUM_CONSTANT(LOBBY_STATE_CLIENT)

    //Now, bind default signals
    ADD_SIGNAL(MethodInfo("favorites_list_account_updated", PropertyInfo(Variant::INT, "result")));
    ADD_SIGNAL(MethodInfo("favorites_list_changed", PropertyInfo(Variant::DICTIONARY, "favorite")));
    ADD_SIGNAL(MethodInfo("lobby_message", PropertyInfo(Variant::INT, "lobby_id"), PropertyInfo(Variant::INT, "user"),
                          PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::INT, "chat_type")));
    ADD_SIGNAL(MethodInfo("lobby_chat_update", PropertyInfo(Variant::INT, "lobby_id"), PropertyInfo(Variant::INT, "changed_id"),
                          PropertyInfo(Variant::INT, "making_change_id"), PropertyInfo(Variant::INT, "chat_state")));
    ADD_SIGNAL(MethodInfo("lobby_created", PropertyInfo(Variant::INT, "connect"), PropertyInfo(Variant::INT, "lobby_id")));
    ADD_SIGNAL(MethodInfo("lobby_data_update"));
    ADD_SIGNAL(MethodInfo("lobby_joined", PropertyInfo(Variant::INT, "lobby"), PropertyInfo(Variant::INT, "permissions"),
                          PropertyInfo(Variant::BOOL, "locked"), PropertyInfo(Variant::INT, "response")));
    ADD_SIGNAL(MethodInfo("lobby_game_created", PropertyInfo(Variant::INT, "lobby_id"), PropertyInfo(Variant::INT, "server_id"),
                          PropertyInfo(Variant::STRING, "server_ip"), PropertyInfo(Variant::INT, "port")));

    //Should these have "_id" suffixes?
    ADD_SIGNAL(MethodInfo("lobby_invite", PropertyInfo(Variant::INT, "inviter"), PropertyInfo(Variant::INT, "lobby"), PropertyInfo(Variant::INT, "game")));
    ADD_SIGNAL(MethodInfo("lobby_match_list", PropertyInfo(Variant::ARRAY, "lobbies")));
    ADD_SIGNAL(MethodInfo("lobby_kicked", PropertyInfo(Variant::INT, "lobby_id"), PropertyInfo(Variant::INT, "admin_id"), PropertyInfo(Variant::INT, "due_to_disconnect")));

    //lastly the debug signal
    ADD_SIGNAL(MethodInfo("debug_data", PropertyInfo(Variant::DICTIONARY, "data")));
}

SteamMultiplayerPeer::SteamMultiplayerPeer() :
    callbackLobbyMessage(this,                  &SteamMultiplayerPeer::lobby_message_scb),
    callbackLobbyChatUpdate(this,                &SteamMultiplayerPeer::lobby_chat_update_scb),
    callbackNetworkMessagesSessionRequest(this, &SteamMultiplayerPeer::network_messages_session_request_scb),
    callbackNetworkMessagesSessionFailed(this,  &SteamMultiplayerPeer::network_messages_session_failed_scb),
    callbackLobbyJoined(this,                   &SteamMultiplayerPeer::lobby_joined_scb),
    callbackLobbyDataUpdate(this,               &SteamMultiplayerPeer::lobby_data_update_scb)
{
    if(SteamUser() != NULL)
        steam_id.set_from_CSteamID(SteamUser()->GetSteamID());
}

SteamMultiplayerPeer::~SteamMultiplayerPeer()
{
    if(lobby_id != SteamID())
        SteamMatchmaking()->LeaveLobby(lobby_id.data);
}


//PackedByteArray SteamMultiplayerPeer::_get_packet_script()
//{

//}

//Error SteamMultiplayerPeer::_put_packet_script(const PackedByteArray &p_buffer)
//{

//}

//void SteamMultiplayerPeer::_set_transfer_channel(int32_t p_channel)
//{

//}

//int32_t SteamMultiplayerPeer::_get_transfer_channel() const
//{

//}

//void SteamMultiplayerPeer::_set_transfer_mode(TransferMode p_mode)
//{

//}

//MultiplayerPeer::TransferMode SteamMultiplayerPeer::_get_transfer_mode() const
//{

//}

//void SteamMultiplayerPeer::_set_refuse_new_connections(bool p_enable)
//{

//}

//bool SteamMultiplayerPeer::_is_refusing_new_connections() const
//{

//}
