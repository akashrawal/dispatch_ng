/* channel.h
 * Event source to join two sockets together so that data transfer
 * takes place between them
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
 
#ifndef CHANNEL_H
#define CHANNEL_H

#include "utils.h"

#define CHANNEL_BUFFER (1024)

//Add a channel to default context
guint channel_add(int fd1, int fd2, Interface iface);

#endif //CHANNEL_H
