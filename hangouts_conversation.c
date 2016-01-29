#include "hangouts_conversation.h"

#include "hangouts.pb-c.h"
#include "hangouts_connection.h"

#include "debug.h"

// From hangouts_pblite
gchar *pblite_dump_json(ProtobufCMessage *message);

RequestHeader *
hangouts_get_request_header(HangoutsAccount *ha)
{
	RequestHeader *header = g_new0(RequestHeader, 1);
	request_header__init(header);
	
	if (ha->client_id != NULL) {
		ClientIdentifier *client_identifier = g_new0(ClientIdentifier, 1);
		client_identifier__init(client_identifier);
		
		header->client_identifier = client_identifier;
		header->client_identifier->resource = g_strdup(ha->client_id);
	}
	
	return header;
}

EventRequestHeader *
hangouts_get_event_request_header(HangoutsAccount *ha, const gchar *conversation_id)
{
	EventRequestHeader *header = g_new0(EventRequestHeader, 1);
	event_request_header__init(header);
	
	if (conversation_id != NULL) {
		ConversationId *conv_id = g_new0(ConversationId, 1);
		conversation_id__init(conv_id);
		
		conv_id->id = g_strdup(conversation_id);
	}
	
	//todo off the record status
	//todo delivery medium
	//todo client generated id
	
	return header;
}

void
hangouts_request_header_free(RequestHeader *header)
{
	if (header->client_identifier) {
		g_free(header->client_identifier->resource);
		g_free(header->client_identifier);
	}
	
	g_free(header);
}

static void 
hangouts_got_self_info(HangoutsAccount *ha, GetSelfInfoResponse *response, gpointer user_data)
{
	Entity *self_entity = response->self_entity;
	
	g_return_if_fail(self_entity);
	
	g_free(ha->self_gaia_id);
	ha->self_gaia_id = g_strdup(self_entity->id->gaia_id);
	
	hangouts_get_buddy_list(ha);
}

void
hangouts_get_self_info(HangoutsAccount *ha)
{
	GetSelfInfoRequest request;
	get_self_info_request__init(&request);
	
	request.request_header = hangouts_get_request_header(ha);
	
	hangouts_pblite_get_self_info(ha, &request, hangouts_got_self_info, NULL);
	
	hangouts_request_header_free(request.request_header);
}


static void
hangouts_got_conversation_list(HangoutsAccount *ha, SyncRecentConversationsResponse *response, gpointer user_data)
{
	guint i, j;
	GHashTable *one_to_ones = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	GHashTable *one_to_ones_rev = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	GHashTable *group_chats = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	//TODO GSList *user_ids;
	/*
struct  _ConversationState
{
  ProtobufCMessage base;
  ConversationId *conversation_id;
  Conversation *conversation;
  size_t n_event;
  Event **event;
  EventContinuationToken *event_continuation_token;
}; */
	for (i = 0; i < response->n_conversation_state; i++) {
		ConversationState *conversation_state = response->conversation_state[i];
		Conversation *conversation = conversation_state->conversation;
		
		purple_debug_info("hangouts", "got conversation state %s\n", pblite_dump_json((ProtobufCMessage *)conversation_state));
		
		if (conversation->has_type && conversation->type == CONVERSATION_TYPE__CONVERSATION_TYPE_ONE_TO_ONE) {
			const gchar *first_person = conversation->current_participant[0]->gaia_id;
			const gchar *second_person = conversation->current_participant[1]->gaia_id;
			const gchar *other_person;
			
			if (g_strcmp0(first_person, conversation->self_conversation_state->self_read_state->participant_id->gaia_id)) {
				other_person = first_person;
			} else {
				other_person = second_person;
			}
			
			g_hash_table_replace(one_to_ones, g_strdup(conversation->conversation_id->id), g_strdup(other_person));
			g_hash_table_replace(one_to_ones_rev, g_strdup(other_person), g_strdup(conversation->conversation_id->id));
		} else {
			g_hash_table_replace(group_chats, g_strdup(conversation->conversation_id->id), NULL);
		}
		
		
		for (j = 0; j < conversation->n_participant_data; j++) {
			ConversationParticipantData *participant_data = conversation->participant_data[j];
			
			purple_serv_got_alias(ha->pc, participant_data->id->gaia_id, participant_data->fallback_name);
			//TODO user_ids = g_slist_append(user_ids, participant_data->id->gaia_id);
		}
	}
	
	ha->one_to_ones = one_to_ones;
	ha->one_to_ones_rev = one_to_ones_rev;
	ha->group_chats = group_chats;
	//todo mark the account as connected
}

void
hangouts_get_conversation_list(HangoutsAccount *ha)
{
	SyncRecentConversationsRequest request;
	SyncFilter sync_filter[1];
	sync_recent_conversations_request__init(&request);
	
	request.request_header = hangouts_get_request_header(ha);
	request.has_max_conversations = TRUE;
	request.max_conversations = 100;
	request.has_max_events_per_conversation = TRUE;
	request.max_events_per_conversation = 1;
	
	sync_filter[0] = SYNC_FILTER__SYNC_FILTER_INBOX;
	request.sync_filter = sync_filter;
	request.n_sync_filter = 1;  // Back streets back, alright!
	
	hangouts_pblite_get_recent_conversations(ha, &request, hangouts_got_conversation_list, NULL);
	
	hangouts_request_header_free(request.request_header);
}

void
hangouts_get_buddy_list(HangoutsAccount *ha)
{
	/*
	POST https://clients6.google.com/rpc/plusi?key={KEY}&alt=json 
Authorization: SAPISIDHASH {AUTH HEADER}
Content-Type: application/json
Accept: application/json
Cookie: {COOKIES}
Pragma: no-cache
Cache-Control: no-cache

{"method":"plusi.ozinternal.listmergedpeople","id":"ListMergedPeople","apiVersion":"v2","jsonrpc":"2.0","params":{"pageSelection":{"maxResults":1000},"params":{"personId":"{MY_USER_ID}","collection":6,"hasField":[8,11],"requestMask":{"includeField":[1,2,3,8,9,11,32]},"commonParams":{"includeAffinity":[3]},"extensionSet":{"extensionNames":[4]}}}} 
*/

/*
{
	"id": "ListMergedPeople",
	"result": {
		"requestMetadata": {
			"serverTimeMs": "527",
		},
		"selection": {
			"totalCount": "127",
		},
		"mergedPerson": [{
			"personId": "{USER ID}",
			"metadata": {
				"contactGroupId": [
					"family",
					"myContacts",
					"starred"
				 ],
			},
			name": [{
				"displayName": "{USERS NAME}",
			}],
			"photo": [{
				"url": "https://lh5.googleusercontent.com/-iPLHmUq4g_0/AAAAAAAAAAI/AAAAAAAAAAA/j1C9pusixPY/photo.jpg",
				"photoToken": "CAASFTEwOTE4MDY1MTIyOTAyODgxNDcwOBih9d_CAg=="
			}],
			"inAppReachability": [
			 {
			  "metadata": {
			   "container": "PROFILE",
			   "encodedContainerId": "{USER ID}"
			  },
			  "appType": "BABEL",
			  "status": "REACHABLE"
			 }]
		}
*/
}


gint
hangouts_send_im(PurpleConnection *pc, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleMessage *msg)
{
	const gchar *who = purple_message_get_recipient(msg);
	const gchar *message = purple_message_get_contents(msg);
	PurpleMessageFlags flags = purple_message_get_flags(msg);
#else
const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
#endif

	HangoutsAccount *ha;
	SendChatMessageRequest request;
	MessageContent message_content;
	const gchar *conv_id;
	
	ha = purple_connection_get_protocol_data(pc);
	conv_id = g_hash_table_lookup(ha->one_to_ones_rev, who);
	g_return_val_if_fail(conv_id, -1); //TODO create new conversation for this new person
	
	send_chat_message_request__init(&request);
	message_content__init(&message_content);
	
	request.request_header = hangouts_get_request_header(ha);
	request.event_request_header = hangouts_get_event_request_header(ha, conv_id);
	
	//TODO listen to response
	hangouts_pblite_send_chat_message(ha, &request, NULL, NULL);
	
	return 1;
}