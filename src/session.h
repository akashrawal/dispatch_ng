/* session.h
 * Handles client connections.
 * 
 * Copyright 2015 Akash Rawal
 * This file is part of dispatch_ng.
 * 
 * dispatch_ng is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * dispatch_ng is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with dispatch_ng.  If not, see <http://www.gnu.org/licenses/>.
 */

typedef struct _Session Session;

typedef enum
{
	SESSION_AUTH, //< waiting for client to send SOCKS5 handshake
	SESSION_REQUEST, //< waiting for client to send destination address
	SESSION_CONNECTING, //< waiting for connection to remote host to establish
	SESSION_CONNECTED, //< as it says, connection established
	SESSION_SHUTDOWN, //< Only transmitting unsent data
	SESSION_CLOSED //< dead state
} SessionState;

Session *session_create(SocketHandle hd);

SessionState session_get_state(Session *session);

void session_shutdown(Session *session);

void session_destroy(Session *session);

typedef void (*SessionStateChangeCB)
	(Session *session, SessionState state, void *data);

void session_set_callback
	(Session *session, SessionStateChangeCB cb, void *data);
