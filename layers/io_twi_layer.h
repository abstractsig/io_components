/*
 *
 * two wire interface layer
 *
 */
#ifndef io_twi_layer_H_
#define io_twi_layer_H_
#include <io_core.h>

//
// master socket
//

#define IO_TWI_MASTER_SOCKET_STRUCT_MEMBERS \
	IO_MULTIPLEX_SOCKET_STRUCT_MEMBERS \
	io_encoding_implementation_t const *encoding; \
	io_encoding_pipe_t *tx_pipe;
	/**/

typedef struct PACK_STRUCTURE io_twi_master_socket {
	IO_TWI_MASTER_SOCKET_STRUCT_MEMBERS
} io_twi_master_socket_t;

io_socket_t* io_twi_master_socket_initialise (io_socket_t*,io_t*,io_settings_t const*);
void io_twi_master_socket_free_memory (io_twi_master_socket_t*);
io_encoding_t* io_twi_master_new_message (io_socket_t*);

#define  SPECIALISE_IO_TWI_MASTER_SOCKET_IMPLEMENTATION(S) \
	SPECIALISE_IO_MULTIPLEX_SOCKET_IMPLEMENTATION (S)\
	.new_message = io_twi_master_new_message, \
	/**/

//
// slave adapter socket
//

io_encoding_t* io_twi_slave_adapter_new_message (io_socket_t*);

#define SPECIALISE_IO_TWI_SLAVE_ADAPTER_SOCKET_IMPLEMENTATION(S) \
	SPECIALISE_IO_ADAPTER_SOCKET_IMPLEMENTATION (S)\
	.new_message = io_twi_slave_adapter_new_message, \
	/**/

//
//
//
typedef struct PACK_STRUCTURE io_twi_transfer {
	IO_LAYER_STRUCT_PROPERTIES
	struct {
		uint32_t bus_address:8;
		uint32_t tx_length:12;
		uint32_t rx_length:12;
	} cmd;
} io_twi_transfer_t;

#define io_twi_transfer_bus_address(t)	(t)->cmd.bus_address
#define io_twi_transfer_tx_length(t)	(t)->cmd.tx_length
#define io_twi_transfer_rx_length(t)	(t)->cmd.rx_length

extern EVENT_DATA io_layer_implementation_t io_twi_layer_implementation;
extern EVENT_DATA io_encoding_implementation_t io_twi_encoding_implementation;

INLINE_FUNCTION bool
is_io_twi_encoding (io_encoding_t const *encoding) {
	return io_encoding_has_implementation (encoding,&io_twi_encoding_implementation);
}

INLINE_FUNCTION void*
get_twi_layer (io_encoding_t *encoding) {
	return io_encoding_get_layer (encoding,&io_twi_layer_implementation);
}

io_encoding_t* mk_io_twi_encoding (io_byte_memory_t*);
io_layer_t* push_io_twi_transmit_layer (io_encoding_t*);

#ifdef IMPLEMENT_IO_TWI_LAYER
//-----------------------------------------------------------------------------
//
// implementation
//
//-----------------------------------------------------------------------------

//
// master socket
//

io_socket_t*
io_twi_master_socket_initialise (io_socket_t *socket,io_t *io,io_settings_t const *C) {
	io_twi_master_socket_t *this = (io_twi_master_socket_t*) socket;

	initialise_io_multiplex_socket (socket,io,C);
	this->encoding = C->encoding;

	this->tx_pipe = mk_io_encoding_pipe (
		io_get_byte_memory(io),io_settings_transmit_pipe_length(C)
	);

	return socket;
}

void
io_twi_master_socket_free_memory (io_twi_master_socket_t *this) {

	// free mux memory ...
	
	free_io_encoding_pipe (this->tx_pipe,io_socket_byte_memory(this));
}

io_encoding_t*
io_twi_master_new_message (io_socket_t *socket) {
	io_twi_master_socket_t *this = (io_twi_master_socket_t*) socket;
	io_encoding_t *message = new_io_encoding (
		this->encoding,io_get_byte_memory (io_socket_io (socket))
	);

	if (message != NULL) {
		io_layer_t *layer = push_io_twi_transmit_layer (message);
		if (layer != NULL) {
			io_layer_set_source_address (layer,message,io_socket_address(socket));
		} else {
			io_panic (io_socket_io (socket),IO_PANIC_OUT_OF_MEMORY);
		}
	}

	return reference_io_encoding (message);
}

//
// slave adapter socket
//

io_encoding_t*
io_twi_slave_adapter_new_message (io_socket_t *socket) {
	io_adapter_socket_t *this = (io_adapter_socket_t*) socket;
	io_encoding_t *message = reference_io_encoding (
		io_socket_new_message (this->outer_socket)
	);

	io_layer_t *layer = get_twi_layer (message);
	if (layer != NULL) {
		io_layer_set_destination_address (layer,message,io_socket_address(this));
	}

	return message;
}

EVENT_DATA io_socket_implementation_t 
io_twi_slave_adapter_implementation = {
	SPECIALISE_IO_ADAPTER_SOCKET_IMPLEMENTATION (
		&io_adapter_socket_implementation
	)
	.new_message = io_twi_slave_adapter_new_message,
};

//
// the layer
//
static io_twi_transfer_t*
mk_twi_layer (io_byte_memory_t *bm,io_encoding_t *packet) {
	io_twi_transfer_t *this = io_byte_memory_allocate (
		bm,sizeof(io_twi_transfer_t)
	);
	if (this) {
		this->implementation = &io_twi_layer_implementation;
		this->layer_offset_in_byte_stream = io_encoding_length ((io_encoding_t*) packet);
		this->cmd.bus_address = 0;
		this->cmd.tx_length = 0;
		this->cmd.rx_length = 0;
	}
	
	return this;
}

static io_layer_t*
mk_twi_transmit_layer (io_byte_memory_t *bm,io_encoding_t *packet) {
	io_twi_transfer_t *this = mk_twi_layer (bm,packet);

	if (this) {
		this->layer_offset_in_byte_stream = io_encoding_length ((io_encoding_t*) packet);
	}
	
	return (io_layer_t*) this;
}

static io_layer_t*
mk_twi_receive_layer (io_byte_memory_t *bm,io_encoding_t *packet) {
	io_twi_transfer_t *this = mk_twi_layer (bm,packet);

	if (this) {
//		this->layer_offset_in_byte_stream = io_encoding_increment_decode_offest (
//			packet,0
//		);
	}
	
	return (io_layer_t*) this;
}

static void
free_twi_layer (io_layer_t *layer,io_byte_memory_t *bm) {
	io_byte_memory_free (bm,layer);
}

static bool
twi_layer_set_bus_address (
	io_layer_t *layer,io_encoding_t *message,io_address_t local
) {
	io_twi_transfer_t *cmd = get_twi_layer (message);
	if (cmd) {
		io_twi_transfer_bus_address (cmd) = io_u8_address_value (local);
		return true;
	} else {
		return false;
	}
}

static io_address_t
twi_layer_get_bus_address (
	io_layer_t *layer,io_encoding_t *message
) {
	io_twi_transfer_t *cmd = get_twi_layer (message);
	if (cmd) {
		return def_io_u8_address(io_twi_transfer_bus_address (cmd));
	} else {
		return io_invalid_address();
	}
}

void
twi_layer_get_content (
	io_layer_t *layer,io_encoding_t *encoding,uint8_t const **begin,uint8_t const **end
) {
	*begin = io_layer_get_byte_stream (layer,encoding);
	*end = *begin + io_encoding_length(encoding);
//	io_binary_frame_t *frame = io_layer_get_byte_stream (layer,encoding);
//	*begin = frame->content;
//	*end = frame->content + read_le_uint32 (frame->length) - sizeof(io_binary_frame_t);
}


EVENT_DATA io_layer_implementation_t io_twi_layer_implementation = {
	SPECIALISE_IO_LAYER_IMPLEMENTATION (&io_layer_implementation)
	.free = free_twi_layer,
	.get_content = twi_layer_get_content,
	.set_source_address = twi_layer_set_bus_address,
	.get_source_address = twi_layer_get_bus_address,
	.set_destination_address = twi_layer_set_bus_address,
	.get_destination_address = twi_layer_get_bus_address,
};

io_layer_t*
push_io_twi_transmit_layer (io_encoding_t *encoding) {
	return io_encoding_push_layer (encoding,mk_twi_transmit_layer);
}

io_layer_t*
push_io_twi_receive_layer (io_encoding_t *encoding) {
	return io_encoding_push_layer (encoding,mk_twi_receive_layer);
}

io_encoding_t* 
mk_io_twi_encoding (io_byte_memory_t *bm) {
	io_packet_encoding_t *this = io_byte_memory_allocate (
		bm,sizeof(io_packet_encoding_t)
	);

	if (this != NULL) {
		this->implementation = &io_twi_encoding_implementation;
		this->bm = bm;
		this = initialise_io_packet_encoding ((io_packet_encoding_t*) this);
	}

	return (io_encoding_t*) this;
};

EVENT_DATA io_encoding_implementation_t io_twi_encoding_implementation = {
	SPECIALISE_IO_PACKET_ENCODING_IMPLEMENTATION (
		&io_packet_encoding_implementation
	)
	.make_encoding = mk_io_twi_encoding,
};

#endif /* IMPLEMENT_IO_TWI_LAYER */
#ifdef IMPLEMENT_VERIFY_IO_CORE_TWI_LAYER
//-----------------------------------------------------------------------------
//
// test implementation
//
//-----------------------------------------------------------------------------
#include <io_verify.h>

TEST_BEGIN(test_io_twi_packet_encoding_1) {
	io_byte_memory_t *bm = io_get_byte_memory (TEST_IO);
	memory_info_t begin,end;

	io_byte_memory_get_info (bm,&begin);
	
	io_encoding_t *encoding = mk_io_twi_encoding (bm);

	if (VERIFY(encoding != NULL,NULL)) {
		const uint8_t *b,*e;
		io_twi_transfer_t *h;

		push_io_twi_transmit_layer (encoding);

		encoding = reference_io_encoding (encoding);
		VERIFY(encoding != NULL,NULL);
		
		VERIFY (is_io_twi_encoding (encoding),NULL);

		io_encoding_get_content (encoding,&b,&e);
		VERIFY ((e - b) == 0,NULL);

		h = get_twi_layer (encoding);

		if (VERIFY (h != NULL,NULL)) {
			h->cmd.bus_address = 0x42;
			h->cmd.tx_length = 1;
			h->cmd.rx_length = 0;
		}

		unreference_io_encoding(encoding);
	}

	io_byte_memory_get_info (bm,&end);
	VERIFY (end.used_bytes == begin.used_bytes,NULL);
}
TEST_END

UNIT_SETUP(setup_io_twi_layer_unit_test) {
	return VERIFY_UNIT_CONTINUE;
}

UNIT_TEARDOWN(teardown_io_twi_layer_unit_test) {
}

void
io_core_twi_layer_unit_test (V_unit_test_t *unit) {
	static V_test_t const tests[] = {
		test_io_twi_packet_encoding_1,
		0
	};
	unit->name = "twi";
	unit->description = "twi socket layer unit test";
	unit->tests = tests;
	unit->setup = setup_io_twi_layer_unit_test;
	unit->teardown = teardown_io_twi_layer_unit_test;
}

void
run_ut_io_core_twi_layer (V_runner_t *runner) {
	static const unit_test_t test_set[] = {
		io_core_twi_layer_unit_test,
		0
	};
	V_run_unit_tests(runner,test_set);
}

#endif /* IMPLEMENT_VERIFY_IO_CORE_TWI_LAYER */
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

