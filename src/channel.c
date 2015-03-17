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

#include "channel.h"

//'Joins' two sockets together
typedef struct
{
	GSource source;
	
	struct {
		GPollFD in_fd;
		char buffer[CHANNEL_BUFFER];
		int start, end;
	} lanes[2];
	
	Interface *interface;
	
} Channel;

//GSourceFuncs::prepare
static gboolean channel_prepare(GSource *source, gint *timeout)
{
	Channel *channel = (Channel *) source;
	int i, opposite;
	
	for (i = 0; i < 2; i++)
		channel->lanes[i].in_fd.events = G_IO_HUP | G_IO_ERR;
	
	for (i = 0; i < 2; i++)
	{
		opposite = 1 - i;
		
		if (channel->lanes[i].end < CHANNEL_BUFFER)
			channel->lanes[i].in_fd.events |= G_IO_IN;
		if (channel->lanes[i].start < channel->lanes[i].end)
			channel->lanes[opposite].in_fd.events |= G_IO_OUT;
	}
	
	*timeout = -1;
	return FALSE;
}

//GSourceFuncs::check
static gboolean channel_check(GSource *source)
{
	Channel *channel = (Channel *) source;
	int i;
	
	for (i = 0; i < 2; i++)
		if (channel->lanes[i].in_fd.revents)
			return TRUE;
	
	return FALSE;
}

//GSourceFuncs::dispatch
static gboolean channel_dispatch
	(GSource *source, GSourceFunc callback, gpointer user_data)
{
	Channel *channel = (Channel *) source;
	int i, opposite, io_res;
	
	//Check for errors
	for (i = 0; i < 2; i++)
		if (channel->lanes[i].in_fd.revents & (G_IO_HUP | G_IO_ERR))
			return G_SOURCE_REMOVE;
	
	for (i = 0; i < 2; i++)
	{
		opposite = 1 - i;
		
		//Read data into buffer
		if (channel->lanes[i].in_fd.revents & G_IO_IN)
		{
			io_res = read(channel->lanes[i].in_fd.fd,
				channel->lanes[i].buffer + channel->lanes[i].end,
				CHANNEL_BUFFER - channel->lanes[i].end);
			
			//Error handling
			if (io_res < 0)
			{
				if (! IO_TEMP_ERROR(errno))
					return G_SOURCE_REMOVE;
			}
			else if (io_res == 0)
				return G_SOURCE_REMOVE;
			
			channel->lanes[i].end += io_res;
		}
		
		//Write it to opposite lane
		if (channel->lanes[opposite].in_fd.revents & G_IO_OUT)
		{
			io_res = write(channel->lanes[opposite].in_fd.fd,
				channel->lanes[i].buffer + channel->lanes[i].start,
				channel->lanes[i].end - channel->lanes[i].start);
			
			//Error handling
			if (io_res < 0)
			{
				if (! IO_TEMP_ERROR(errno))
					return G_SOURCE_REMOVE;
			}
			else if (io_res == 0)
				return G_SOURCE_REMOVE;
			
			channel->lanes[i].start += io_res;
			
			//If start of buffer crosses middle,
			//shift the buffer contents to beginning
			if (channel->lanes[i].start >= (CHANNEL_BUFFER / 2))
			{
				int j, new_end;
				
				new_end = channel->lanes[i].end - channel->lanes[i].start;
				
				for (j = 0; j < new_end; j++)
					channel->lanes[i].buffer[j] 
						= channel->lanes[i].buffer
							[j + channel->lanes[i].start];
				
				channel->lanes[i].start = 0;
				channel->lanes[i].end = new_end;
			}
		}
	}
	
	return G_SOURCE_CONTINUE;
}

//GSourceFuncs::finalize
static void channel_finalize(GSource *source)
{
	Channel *channel = (Channel *) source;
	int i;
	
	for (i = 0; i < 2; i++)
		close(channel->lanes[i].in_fd.fd);
	
	interface_close(channel->interface);
}

static GSourceFuncs channel_funcs =
{
	channel_prepare,
	channel_check,
	channel_dispatch,
	channel_finalize
};

//Add a channel to default context
guint channel_add(int fd1, int fd2, Interface *interface)
{
	Channel *channel;
	guint tag;
	
	//Create GSource
	channel = (Channel *) g_source_new(&channel_funcs, sizeof(Channel));
	
	//Initialize lanes
	channel->lanes[0].in_fd.fd = fd1;
	g_source_add_poll((GSource *) channel, &(channel->lanes[0].in_fd));
	channel->lanes[1].in_fd.fd = fd2;
	g_source_add_poll((GSource *) channel, &(channel->lanes[1].in_fd));
	
	//Add interface
	channel->interface = interface;
	
	//Add to default context
	tag = g_source_attach((GSource *) channel, NULL);
	g_source_unref((GSource *) channel);
	
	return tag;
}

