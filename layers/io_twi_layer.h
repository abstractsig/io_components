/*
 *
 * two wire interface layer
 *
 */
#ifndef io_twi_layer_H_
#define io_twi_layer_H_
#include <io_core.h>

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

INLINE_FUNCTION io_encoding_t*
mk_io_twi_encoding (io_byte_memory_t *bm) {
	return io_twi_encoding_implementation.make_encoding(bm);
}

INLINE_FUNCTION bool
is_io_twi_encoding (io_encoding_t const *encoding) {
	return io_is_encoding_of_type (encoding,&io_twi_encoding_implementation);
}

INLINE_FUNCTION void*
get_twi_layer (io_encoding_t *encoding) {
	return io_encoding_get_layer (encoding,&io_twi_layer_implementation);
}


#ifdef IMPLEMENT_IO_TWI_LAYER
//-----------------------------------------------------------------------------
//
// implementaion
//
//-----------------------------------------------------------------------------

static io_layer_t*
make_twi_layer (io_packet_encoding_t *packet) {
	io_twi_transfer_t *this = io_byte_memory_allocate (
		packet->bm,sizeof(io_twi_transfer_t)
	);
	if (this) {
		this->implementation = &io_twi_layer_implementation;
		this->layer_offset_in_byte_stream = io_encoding_length ((io_encoding_t*) packet);
		this->cmd.bus_address = 0;
		this->cmd.tx_length = 0;
		this->cmd.rx_length = 0;
	}
	
	return (io_layer_t*) this;
}

static void
free_twi_layer (io_layer_t *layer,io_byte_memory_t *bm) {
	io_byte_memory_free (bm,layer);
}

static bool
twi_layer_set_local_address (
	io_layer_t *layer,io_encoding_t *message,io_address_t local
) {
	return false;
}


EVENT_DATA io_layer_implementation_t io_twi_layer_implementation = {
	.specialisation_of = NULL,
	.make = make_twi_layer,
	.free = free_twi_layer,
	.get_remote_address = NULL,
	.get_local_address = NULL,
	.set_local_address = twi_layer_set_local_address,
};


static io_encoding_t* 
io_twi_encoding_new (io_byte_memory_t *bm) {
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

EVENT_DATA io_encoding_layer_api_t twi_layer_api = {
	.get_inner_layer = NULL,
	.get_outer_layer = NULL,
	.get_layer = get_packet_encoding_layer,
	.push_layer = io_packet_encoding_push_layer,
};


EVENT_DATA io_encoding_implementation_t io_twi_encoding_implementation = {
	.specialisation_of = &io_binary_encoding_implementation,
	.decode_to_io_value = io_binary_encoding_decode_to_io_value,
	.make_encoding = io_twi_encoding_new,
	.free = io_packet_encoding_free,
	.get_io = io_binary_encoding_get_io,
	.grow = io_binary_encoding_grow,
	.grow_increment = default_io_encoding_grow_increment,
	.layer = &twi_layer_api,
	.fill = io_binary_encoding_fill_bytes,
	.append_byte = io_binary_encoding_append_byte,
	.append_bytes = io_binary_encoding_append_bytes,
	.pop_last_byte = io_binary_encoding_pop_last_byte,
	.print = io_binary_encoding_print,
	.reset = io_binary_encoding_reset,//
	.get_byte_stream = io_binary_encoding_get_byte_stream,
	.get_content = io_binary_encoding_get_content,
	.length = io_binary_encoding_length,
	.limit = io_binary_encoding_nolimit,
};
#endif /* IMPLEMENT_IO_TWI_LAYER */
#ifdef IMPLEMENT_VERIFY_IO_TWI_LAYER

TEST_BEGIN(test_io_twi_packet_encoding_1) {
	io_byte_memory_t *bm = io_get_byte_memory (TEST_IO);
	memory_info_t begin,end;

	io_byte_memory_get_info (bm,&begin);
	
	io_encoding_t *encoding = mk_io_twi_encoding (bm);

	if (VERIFY(encoding != NULL,NULL)) {
		const uint8_t *b,*e;
		io_twi_transfer_t *h;

		io_encoding_push_layer (
			encoding,&io_twi_layer_implementation
		);

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
io_twi_layer_unit_test (V_unit_test_t *unit) {
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

# define IO_TWI_LAYER_UNIT_TESTS io_twi_layer_unit_test,
#else
# define IO_TWI_LAYER_UNIT_TESTS
#endif
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

