/*
 *
 * data link control
 *
 */
#ifndef io_dlc_socket_H_
#define io_dlc_socket_H_
#include <io_core.h>
#ifdef IMPLEMENT_IO_CORE
# ifndef IMPLEMENT_IO_DLC_LAYER
#  define IMPLEMENT_IO_DLC_LAYER
# endif
#endif

typedef struct io_dlc_socket_state io_dlc_socket_state_t;
typedef struct io_dlc_socket io_dlc_socket_t;

typedef struct io_dlc_socket_state {
	io_dlc_socket_state_t const* (*enter) (io_dlc_socket_t*);
	io_dlc_socket_state_t const* (*receive) (io_dlc_socket_t*);	
} io_dlc_socket_state_t;

static EVENT_DATA io_dlc_socket_state_t io_dlc_socket_state_closed;

typedef struct PACK_STRUCTURE io_dlc_socket {
	IO_MULTIPLEXER_SOCKET_STRUCT_MEMBERS
	
	io_dlc_socket_state_t const *state;
	
} io_dlc_socket_t;

io_socket_t*	allocate_io_dlc_socket (io_t*,io_address_t);

typedef struct PACK_STRUCTURE io_dlc_layer {
	IO_LAYER_STRUCT_PROPERTIES
	io_address_t remote;
	io_byte_memory_t *bm;
} io_dlc_layer_t;

io_layer_t* push_io_dlc_transmit_layer (io_encoding_t*);

typedef struct PACK_STRUCTURE io_dlc_frame {
	uint8_t length;
	uint8_t command;
	uint8_t content[];
} io_dlc_frame_t;

#ifdef IMPLEMENT_IO_DLC_LAYER
//-----------------------------------------------------------------------------
//
// implementaion
//
//-----------------------------------------------------------------------------

INLINE_FUNCTION io_dlc_socket_state_t const*
call_io_dlc_socket_state_enter (io_dlc_socket_t *dlc) {
	return dlc->state->enter (dlc);
}

static void io_dlc_socket_call_state (io_dlc_socket_t*,io_dlc_socket_state_t const* (*fn) (io_dlc_socket_t*));

INLINE_FUNCTION void
io_dlc_socket_enter_current_state (io_dlc_socket_t *this) {
	io_dlc_socket_call_state (this,this->state->enter);
}

static void
io_dlc_socket_call_state (io_dlc_socket_t *this,io_dlc_socket_state_t const* (*fn) (io_dlc_socket_t*)) {
	io_dlc_socket_state_t const *current = this->state;
	io_dlc_socket_state_t const *next = fn (this);
	if (next != current) {
		this->state = next;
		 io_dlc_socket_enter_current_state (this);
	}
}

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *
 * dlc socket states
 *
 *               io_dlc_socket_state_closed
 *                 |                             
 *                 v
 *               
 *
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */
static EVENT_DATA io_dlc_socket_state_t io_dlc_socket_state_closed;


static io_dlc_socket_state_t const*
io_dlc_socket_state_closed_enter (io_dlc_socket_t *this) {
	return this->state;
}

static io_dlc_socket_state_t const*
io_dlc_socket_state_closed_receive (io_dlc_socket_t *this) {
	return this->state;
}

static EVENT_DATA io_dlc_socket_state_t io_dlc_socket_state_closed = {
	.enter = io_dlc_socket_state_closed_enter,
	.receive = io_dlc_socket_state_closed_receive,
};

//
// socket
//

static void	
io_dlc_socket_tx_event (io_event_t *ev) {
}

static void	
io_dlc_socket_rx_event (io_event_t *ev) {
}

static io_socket_t*
io_dlc_socket_initialise (io_socket_t *socket,io_t *io,io_settings_t const *C) {
	io_dlc_socket_t *this = (io_dlc_socket_t*) socket;

	initialise_io_multiplexer_socket (socket,io,C);

	initialise_io_event (
		&this->transmit_event,io_dlc_socket_tx_event,this
	);
	initialise_io_event (
		&this->receive_event,io_dlc_socket_rx_event,this
	);

	this->state = &io_dlc_socket_state_closed;
	
	io_dlc_socket_enter_current_state (this);
	
	return socket;
}

static void
io_dlc_socket_free (io_socket_t *socket) {
	io_socket_close (socket);
	io_multiplex_socket_free (socket);
//	io_byte_memory_free (io_get_byte_memory (io_socket_io (socket)),socket);
}

static bool
io_dlc_socket_open (io_socket_t *socket) {
	
	return true;
}

static void
io_dlc_socket_close (io_socket_t *socket) {
	io_dlc_socket_t *this = (io_dlc_socket_t*) socket;

	close_io_multiplexer_socket ((io_multiplexer_socket_t*) this);
	
	// unbind inner_socket
}

static bool
io_dlc_socket_is_closed (io_socket_t const *socket) {
	return true;
}

static io_encoding_t*
io_dlc_socket_new_message (io_socket_t *socket) {	
	io_multiplexer_socket_t *this = (io_multiplexer_socket_t*) socket;
	io_encoding_t *message = io_socket_new_message (this->outer_socket);
	io_layer_t *dlc = push_io_dlc_transmit_layer (message);
	io_layer_t *outer = io_encoding_get_outer_layer (message,dlc);
	
	if (dlc && outer) {
		io_layer_set_inner_address (outer,message,io_socket_address (socket));
		io_layer_set_remote_address (outer,message,io_layer_any_address (outer));
	} else {
		io_encoding_free(message);
		message = NULL;
	}

	return reference_io_encoding(message);
}

bool
io_dlc_socket_constructor (io_t *io,io_address_t a,io_socket_t **outer,io_socket_t **inner) {
	return false;
}

EVENT_DATA io_socket_implementation_t io_dlc_socket_implementation = {
	.specialisation_of = &io_multiplexer_socket_implementation,
	.initialise = io_dlc_socket_initialise,
	.reference = io_counted_socket_increment_reference,
	.free = io_dlc_socket_free,
	.open = io_dlc_socket_open,
	.close = io_dlc_socket_close,
	.is_closed = io_dlc_socket_is_closed,
	.bind_inner = io_multiplex_socket_bind_inner,
	.unbind_inner = io_multiplex_socket_unbind_inner,
	.bind_inner_constructor = io_virtual_socket_bind_inner_constructor,
	.bind_to_outer_socket = io_multiplexer_socket_bind_to_outer,
	.new_message = io_dlc_socket_new_message,
	.send_message = io_multiplexer_socket_send_message,
	.get_receive_pipe = NULL,
	.iterate_inner_sockets = NULL,
	.iterate_outer_sockets = NULL,
	.mtu = io_multiplexer_socket_mtu,
};

io_socket_t*
allocate_io_dlc_socket (io_t *io,io_address_t address) {
	io_socket_t *socket = io_byte_memory_allocate (
		io_get_byte_memory (io),sizeof(io_dlc_socket_t)
	);
	socket->implementation = &io_dlc_socket_implementation;
	socket->address = duplicate_io_address (io_get_byte_memory (io),address);
	return socket;
}

//
// layer
//

EVENT_DATA io_layer_implementation_t io_dlc_layer_implementation = {
	SPECIALISE_VIRTUAL_IO_LAYER_IMPLEMENTATION(NULL)
};

static io_address_t
io_dlc_layer_any_address (void) {
	return io_invalid_address();
}

io_layer_t*
push_io_dlc_transmit_layer (io_encoding_t *encoding) {
	extern EVENT_DATA io_layer_implementation_t io_dlc_layer_transmit_implementation;
	return io_encoding_push_layer (
		encoding,&io_dlc_layer_transmit_implementation
	);
}

static io_layer_t*
mk_io_dlc_layer_type (io_packet_encoding_t *packet,io_layer_implementation_t const *T) {
	io_dlc_layer_t *this = io_byte_memory_allocate (
		packet->bm,sizeof(io_dlc_layer_t)
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
free_io_dlc_layer (io_layer_t *layer,io_byte_memory_t *bm) {
	io_dlc_layer_t *this = (io_dlc_layer_t*) layer;
	free_io_address (this->bm,this->remote);
	io_byte_memory_free (bm,layer);
}

static bool
io_dlc_layer_match_address (io_layer_t *layer,io_address_t address) {
	return compare_io_addresses (IO_DLC_LAYER_ID,address) == 0;
}	

static io_address_t
io_dlc_layer_get_remote_address (io_layer_t *layer,io_encoding_t *encoding) {
	io_dlc_layer_t *this = (io_dlc_layer_t*) layer;
	return this->remote;
}

static bool
io_dlc_layer_set_remote_address (
	io_layer_t *layer,io_encoding_t *message,io_address_t address
) {
	io_dlc_layer_t *this = (io_dlc_layer_t*) layer;
	this->remote = duplicate_io_address (this->bm,address);
	return true;
}

static io_layer_t*
mk_io_dlc_layer_transmit (io_packet_encoding_t *packet) {
	extern EVENT_DATA io_layer_implementation_t io_dlc_layer_transmit_implementation;
	return mk_io_dlc_layer_type (packet,&io_dlc_layer_transmit_implementation);
}

static io_layer_t*
io_dlc_layer_swap_tx (io_layer_t *layer,io_encoding_t *encoding) {
	extern EVENT_DATA io_layer_implementation_t io_dlc_layer_receive_implementation;
	return io_encoding_push_layer (
		encoding,&io_dlc_layer_receive_implementation
	);
}

EVENT_DATA io_layer_implementation_t io_dlc_layer_transmit_implementation = {
	.specialisation_of = &io_dlc_layer_implementation,
	.any = io_dlc_layer_any_address,
	.make = mk_io_dlc_layer_transmit,
	.free =  free_io_dlc_layer,
	.swap =  io_dlc_layer_swap_tx,
	.decode = NULL,
	.match_address =  io_dlc_layer_match_address,
	.get_remote_address =  io_dlc_layer_get_remote_address,
	.set_remote_address =  io_dlc_layer_set_remote_address,
	.get_local_address =  NULL,
	.set_local_address =  NULL,
	.get_inner_address =  NULL,
	.set_inner_address =  NULL,
};

static io_layer_t*
mk_io_dlc_layer_receive (io_packet_encoding_t *packet) {
	extern EVENT_DATA io_layer_implementation_t io_dlc_layer_receive_implementation;
	return mk_io_dlc_layer_type (packet,&io_dlc_layer_receive_implementation);
}

static io_inner_port_t*
io_dlc_layer_receive_decode (
	io_layer_t *layer,io_encoding_t *encoding,io_multiplex_socket_t* socket
) {
	io_dlc_frame_t *packet = io_layer_get_byte_stream (layer,encoding);

	if (packet->length >= 2) {
		// process in dlc_socket state
		
	/*
		io_address_t addr = io_layer_get_inner_address (layer,encoding);
		io_inner_port_binding_t *inner = io_multiplex_socket_find_inner_binding (socket,addr);

		if (inner) {
			return inner->port;
		}
	*/
	}
	
	return NULL;
}

EVENT_DATA io_layer_implementation_t io_dlc_layer_receive_implementation = {
	.specialisation_of = &io_dlc_layer_implementation,
	.any = io_dlc_layer_any_address,
	.make = mk_io_dlc_layer_receive,
	.free =  free_io_dlc_layer,
	.swap =  NULL,
	.decode = io_dlc_layer_receive_decode,
	.match_address =  io_dlc_layer_match_address,
	.get_remote_address =  io_dlc_layer_get_remote_address,
	.set_remote_address =  io_dlc_layer_set_remote_address,
	.get_local_address =  NULL,
	.set_local_address =  NULL,
	.get_inner_address =  NULL,
	.set_inner_address =  NULL,
};
#endif /* IMPLEMENT_IO_DLC_LAYER */
#ifdef IMPLEMENT_VERIFY_IO_DLC_SOCKET

UNIT_SETUP(setup_io_dlc_socket_unit_test) {
	return VERIFY_UNIT_CONTINUE;
}

UNIT_TEARDOWN(teardown_io_dlc_socket_unit_test) {
}

static void
io_dlc_socket_unit_test (V_unit_test_t *unit) {
	static V_test_t const tests[] = {
		0
	};
	unit->name = "io dlc sockets";
	unit->description = "io dlc sockets unit test";
	unit->tests = tests;
	unit->setup = setup_io_dlc_socket_unit_test;
	unit->teardown = teardown_io_dlc_socket_unit_test;
}

void
run_ut_io_dlc_socket (V_runner_t *runner) {
	static const unit_test_t test_set[] = {
		io_dlc_socket_unit_test,
		0
	};
	V_run_unit_tests(runner,test_set);
}

#endif /* IMPLEMENT_VERIFY_IO_DLC_SOCKET */
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

