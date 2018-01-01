/* server.h
 * Server socket
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

typedef struct _Server Server;


Server *server_create(const char *str);

void server_destroy(Server *server);

typedef enum
{
	SERVER_SESSION_OPEN = 0,
	SERVER_SESSION_CLOSE = 1
} ServerEvent;

typedef void (*ServerEventCB)(Server *server, ServerEvent event, void *data);

void server_set_cb(Server *server, ServerEventCB cb, void *cb_data);


