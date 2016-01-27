#ifndef _PURPLECOMPAT_H_
#define _PURPLECOMPAT_H_

#include <glib.h>
#include "version.h"

#if PURPLE_VERSION_CHECK(3, 0, 0)
#include <glib-object.h>

#define purple_circular_buffer_destroy		g_object_unref
#define purple_hash_destroy			g_object_unref

#else

#include "connection.h"

#define PURPLE_TYPE_CONNECTION	purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION)
#define PURPLE_IS_CONNECTION	PURPLE_CONNECTION_IS_VALID

#define PURPLE_CONNECTION_CONNECTED		PURPLE_CONNECTED

#define purple_request_cpar_from_connection(a)  purple_connection_get_account(a), NULL, NULL
#define purple_connection_get_protocol		purple_connection_get_prpl
#define purple_connection_error                 purple_connection_error_reason
#define purple_connection_is_disconnecting(c)   FALSE

#define purple_protocol_got_user_status		purple_prpl_got_user_status

#define purple_account_set_password(account, password, dummy1, dummy2) \
		purple_account_set_password(account, password);

#define purple_proxy_info_get_proxy_type        purple_proxy_info_get_type

#endif

#endif /*_PURPLECOMPAT_H_*/
