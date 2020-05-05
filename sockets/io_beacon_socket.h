/*
 *
 * beacons
 *
 */
#ifndef io_beacon_socket_H_
#define io_beacon_socket_H_
#include <io_core.h>

enum {
	IO_BEACON_COMMAND_ANNOUNCE = 1,
};

typedef struct PACK_STRUCTURE io_beacon_frame {
	uint8_t length;
	uint8_t command;
	uint8_t uid[IO_UID_BYTE_LENGTH];
	uint8_t time[sizeof(io_time_t)];
	uint8_t command_parameters[];
} io_beacon_frame_t;

typedef struct PACK_STRUCTURE io_beacon_socket {
	IO_COUNTED_SOCKET_STRUCT_MEMBERS
	
	io_time_t interval;
	
	io_event_t next_beacon;
	io_event_t beacon_interval_error;
	io_alarm_t beacon_timer;
	
	io_socket_constructor_t make;
	io_notify_event_t *notify;
	
} io_beacon_socket_t;

io_socket_t* 	allocate_io_beacon_socket (io_t*);
io_layer_t*		push_io_beacon_transmit_layer (io_encoding_t*);

#ifdef IMPLEMENT_IO_SOCKET_BEACON
//-----------------------------------------------------------------------------
//
// implementaion
//
//-----------------------------------------------------------------------------


static void
io_beacon_socket_beacon_next_beacon (io_event_t *ev) {
}

static void
io_beacon_socket_timer_error (io_event_t *ev) {
}

static io_socket_t*
io_socket_beacon_initialise (io_socket_t *socket,io_t *io,io_settings_t const *C) {
	io_beacon_socket_t *this = (io_beacon_socket_t*) socket;

	initialise_io_counted_socket (socket,io);

	initialise_io_event (
		&this->next_beacon,io_beacon_socket_beacon_next_beacon,this
	);
	initialise_io_event (
		&this->beacon_interval_error,io_beacon_socket_timer_error,this
	);
	initialise_io_alarm (
		&this->beacon_timer,&this->next_beacon,&this->beacon_interval_error,time_zero()
	);
	
	this->make = C->make;
	this->notify = C->notify;

	return socket;
}

static void
io_socket_beacon_free (io_socket_t *socket) {
	io_counted_socket_free (socket);
}

static bool
io_socket_beacon_open (io_socket_t *socket) {
	return false;
}

static void
io_socket_beacon_close (io_socket_t *socket) {
}

static bool
io_socket_beacon_is_closed (io_socket_t const *socket) {
	return false; //??
}

static io_encoding_t*
io_socket_beacon_new_message (io_socket_t *socket) {
	//
	// does not support this
	//
	return NULL;
}

static bool
io_socket_beacon_send_message (io_socket_t *socket,io_encoding_t *encoding) {
	return false;
}

bool
io_socket_beacon_bind_inner (
	io_socket_t *socket,io_address_t a,io_event_t *tx,io_event_t *rx
) {
	return false;
}

static bool
io_socket_beacon_bind_to_outer_socket (io_socket_t *socket,io_socket_t *outer) {
	return false;
}

EVENT_DATA io_socket_implementation_t io_socket_beacon_implementation = {
	.specialisation_of = &io_counted_socket_implementation,
	.reference = io_counted_socket_increment_reference,
	.initialise = io_socket_beacon_initialise,
	.free = io_socket_beacon_free,
	.open = io_socket_beacon_open,
	.close = io_socket_beacon_close,
	.is_closed = io_socket_beacon_is_closed,
	.bind_inner = io_socket_beacon_bind_inner,
	.bind_inner_constructor = NULL,
	.bind_to_outer_socket = io_socket_beacon_bind_to_outer_socket,
	.new_message = io_socket_beacon_new_message,
	.send_message = io_socket_beacon_send_message,
	.iterate_outer_sockets = NULL,
	.iterate_inner_sockets = NULL,
	.mtu = io_leaf_socket_mtu,
};

io_socket_t*
allocate_io_beacon_socket (io_t *io) {
	io_beacon_socket_t *socket = io_byte_memory_allocate (
		io_get_byte_memory (io),sizeof(io_beacon_socket_t)
	);
	socket->implementation = &io_socket_beacon_implementation;
	socket->address = IO_NIM_LAYER_ID;
	return (io_socket_t*) socket;
}

//
// layer
//
EVENT_DATA io_layer_implementation_t io_beacon_layer_implementation = {
	SPECIALISE_VIRTUAL_IO_LAYER_IMPLEMENTATION(NULL)
};

io_layer_t*
push_io_beacon_transmit_layer (io_encoding_t *encoding) {
	extern EVENT_DATA io_layer_implementation_t io_beacon_layer_transmit_implementation;
	return io_encoding_push_layer (
		encoding,&io_beacon_layer_transmit_implementation
	);
}

static io_address_t
io_beacon_layer_any_address (void) {
	return io_invalid_address();
}

static io_layer_t*
mk_io_beacon_layer_type (io_packet_encoding_t *packet,io_layer_implementation_t const *T) {
	io_beacon_layer_t *this = io_byte_memory_allocate (
		packet->bm,sizeof(io_beacon_layer_t)
	);

	if (this) {
		this->implementation = T;
		this->bm = packet->bm;
		this->layer_offset_in_byte_stream = io_encoding_length ((io_encoding_t*) packet);
		this->remote = io_invalid_address();
	}
	
	return (io_layer_t*) this;
}

static void
free_io_beacon_layer (io_layer_t *layer,io_byte_memory_t *bm) {
	io_beacon_layer_t *this = (io_beacon_layer_t*) layer;
	free_io_address (this->bm,this->remote);
	io_byte_memory_free (bm,layer);
}

static bool
io_beacon_layer_match_address (io_layer_t *layer,io_address_t address) {
	return compare_io_addresses (IO_NIM_LAYER_ID,address) == 0;
}	

static io_layer_t*
mk_io_beacon_layer_transmit (io_packet_encoding_t *packet) {
	extern EVENT_DATA io_layer_implementation_t io_beacon_layer_transmit_implementation;
	return mk_io_beacon_layer_type (packet,&io_beacon_layer_transmit_implementation);
}

static io_layer_t*
io_beacon_layer_swap_tx (io_layer_t *layer,io_encoding_t *encoding) {
	extern EVENT_DATA io_layer_implementation_t io_beacon_layer_receive_implementation;
	return io_encoding_push_layer (
		encoding,&io_beacon_layer_receive_implementation
	);
}

EVENT_DATA io_layer_implementation_t io_beacon_layer_transmit_implementation = {
	.specialisation_of = &io_beacon_layer_implementation,
	.any = io_beacon_layer_any_address,
	.make = mk_io_beacon_layer_transmit,
	.free =  free_io_beacon_layer,
	.swap =  io_beacon_layer_swap_tx,
	.decode = NULL,
	.match_address =  io_beacon_layer_match_address,
	.get_remote_address =  NULL,
	.set_remote_address =  NULL,
	.get_local_address =  NULL,
	.set_local_address =  NULL,
	.get_inner_address =  NULL,
	.set_inner_address =  NULL,
};

EVENT_DATA io_layer_implementation_t io_beacon_layer_receive_implementation = {
	.specialisation_of = &io_beacon_layer_implementation,
	.any = io_beacon_layer_any_address,
	.make = mk_io_beacon_layer_transmit,
	.free =  free_io_beacon_layer,
	.swap =  io_beacon_layer_swap_tx,
	.decode = NULL,
	.match_address =  io_beacon_layer_match_address,
	.get_remote_address =  NULL,
	.set_remote_address =  NULL,
	.get_local_address =  NULL,
	.set_local_address =  NULL,
	.get_inner_address =  NULL,
	.set_inner_address =  NULL,
};


#endif /* IMPLEMENT_IO_SOCKET_BEACON */
#endif
/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2020 Gregor Bruce
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
