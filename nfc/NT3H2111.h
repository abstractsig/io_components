/*
 *
 * Two wire ntag
 *
 */
#ifndef NT3H2111_H_
#define NT3H2111_H_


typedef struct PACK_STRUCTURE nt3h2111_io_socket {
	IO_PROCESS_SOCKET_STRUCT_MEMBERS
} nt3h2111_io_socket_t;

extern EVENT_DATA io_socket_implementation_t nt3h2111_io_socket_implementation;

#ifdef IMPLEMENT_IO_DEVICE
//-----------------------------------------------------------------------------
//
// implementation
//
//-----------------------------------------------------------------------------
/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *
 * socket states
 *
 *               nt3h2111_io_socket_state_closed
 *                 |
 *                 v
 *                 |
 *          <open> |
 *                 v                           <close>
 *               nt3h2111_io_socket_state_open --->
 *
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */
static EVENT_DATA io_socket_state_t nt3h2111_io_socket_state_closed;
static EVENT_DATA io_socket_state_t nt3h2111_io_socket_state_open;

io_socket_state_t const*
nt3h2111_io_socket_state_closed_open (io_socket_t *socket,io_socket_open_flag_t flag) {
	return io_process_socket_state_closed_open_generic (
		socket,flag,&nt3h2111_io_socket_state_open
	);
}

static EVENT_DATA io_socket_state_t nt3h2111_io_socket_state_closed = {
	SPECIALISE_IO_SOCKET_STATE (&io_socket_state)
	.name = "closed",
	.open = nt3h2111_io_socket_state_closed_open,
};

int64_t
read_le_int48 (uint8_t const *ptr8) {
	return (int64_t) (
			(uint64_t)ptr8[0]
		+	((uint64_t)ptr8[1] << 8ULL)
		+	((uint64_t)ptr8[2] << 16ULL)
		+	((uint64_t)ptr8[3] << 24ULL)
		+	((uint64_t)ptr8[4] << 32ULL)
		+	((uint64_t)ptr8[5] << 40ULL)
	);
}


io_socket_state_t const*
nt3h2111_io_socket_state_open_outer_receive_event (io_socket_t *socket) {
	io_process_socket_t *this = (io_process_socket_t*) socket;

	io_encoding_pipe_t* rx = cast_to_io_encoding_pipe (
		io_socket_get_receive_pipe (
			this->outer_socket,io_socket_address (this)
		)
	);
	if (rx) {
		io_encoding_t *next;
		if (io_encoding_pipe_peek (rx,&next)) {
			io_layer_t *twi = get_twi_layer (next);

			/////
			const uint8_t *begin, *end;
			io_layer_get_content (twi,next,&begin,&end);


			io_log (
				io_socket_io (this),
				IO_INFO_LOG_LEVEL,
				"%-*s%-*srx len = %u, 0x%02x 0x%012llx\n",
				DBP_FIELD1,"ntag",
				DBP_FIELD2,"response",
				end - begin,begin[0],read_le_int48(begin + 1)
			);
			/////

			if (this->receive_data_available != NULL) {
				io_enqueue_event (io_socket_io (this),this->receive_data_available);
			}

			io_encoding_pipe_pop_encoding (rx);
		}
	}

	return socket->State;
}

io_socket_state_t const*
nt3h2111_io_socket_state_open_close (io_socket_t *socket) {
	return io_process_socket_state_open_close_generic (
		socket,&nt3h2111_io_socket_state_closed
	);
}

static EVENT_DATA io_socket_state_t nt3h2111_io_socket_state_open = {
	SPECIALISE_IO_SOCKET_STATE (&io_socket_state)
	.name = "ntag open",
	.outer_receive_event = nt3h2111_io_socket_state_open_outer_receive_event,
	.close = nt3h2111_io_socket_state_open_close,
};

io_socket_t*
nt3h2111_io_socket_initialise (io_socket_t *socket,io_t *io,io_settings_t const *C) {
//	nt3h2111_io_socket_t *this = (nt3h2111_io_socket_t*) socket;

	io_process_socket_initialise (socket,io,C);

	socket->State = &nt3h2111_io_socket_state_closed;

	return socket;
}

io_encoding_t*
nt3h2111_io_socket_new_message (io_socket_t *socket) {
	io_encoding_t *message = io_process_socket_new_message (socket);
	io_layer_t *layer = get_twi_layer (message);
	if (layer != NULL) {
		io_layer_set_destination_address (layer,message,io_socket_address(socket));
	}

	return message;
}

bool
nt3h2111_io_socket_is_closed (io_socket_t const *socket) {
	return socket->State == &nt3h2111_io_socket_state_closed;
}

EVENT_DATA io_socket_implementation_t nt3h2111_io_socket_implementation = {
	SPECIALISE_IO_PROCESS_SOCKET_IMPLEMENTATION (
		&io_process_socket_implementation
	)
	.initialise = nt3h2111_io_socket_initialise,
	.new_message = nt3h2111_io_socket_new_message,
	.is_closed = nt3h2111_io_socket_is_closed,
};

#endif /* IMPLEMENT_IO_DEVICE */
#ifdef IMPLEMENT_VERIFY_IO_DEVICE
#include <io_verify.h>

uint32_t test_nt3h2111_io_socket_1_result;

void
test_nt3h2111_io_socket_1_rx_event (io_event_t *ev) {
	test_nt3h2111_io_socket_1_result = 1;
}

TEST_BEGIN(test_nt3h2111_io_socket_1) {
	io_socket_t *ntag = io_get_socket (TEST_IO,NT3H2111_TEST_SOCKET_ID);

	if (VERIFY (ntag != NULL,NULL) && VERIFY (io_socket_is_closed(ntag),NULL)) {

		if (VERIFY (io_socket_open (ntag,IO_SOCKET_OPEN_CONNECT),NULL)) {
			io_event_t ev;
			initialise_io_event (&ev,test_nt3h2111_io_socket_1_rx_event,NULL);
			io_socket_bind_inner (ntag,io_invalid_address(),NULL,&ev);
			test_nt3h2111_io_socket_1_result = 0;

			// open is asynchronous
			io_wait_for_all_events (TEST_IO);

			if (VERIFY (io_socket_is_open(ntag),NULL)) {

				io_encoding_t *message = io_socket_new_message (ntag);
				if (message) {
					io_twi_transfer_t *cmd = get_twi_layer (message);

					io_twi_transfer_tx_length(cmd) = 1;
					io_twi_transfer_rx_length(cmd) = 16;

					io_binary_encoding_append_byte (message,0);

					io_socket_send_message (ntag,message);
					io_wait_for_all_events (TEST_IO);
				}

			}

			while (test_nt3h2111_io_socket_1_result == 0);

			io_socket_close (ntag);
			io_wait_for_all_events (TEST_IO);
			VERIFY (io_socket_is_closed(ntag),NULL);

		}
	}
	io_flush_log (TEST_IO);
}
TEST_END


UNIT_SETUP(setup_nt3h2111_io_socket_unit_test) {
	return VERIFY_UNIT_CONTINUE;
}

UNIT_TEARDOWN(teardown_nt3h2111_io_socket_unit_test) {
}

void
nt3h2111_io_socket_unit_test (V_unit_test_t *unit) {
	static V_test_t const tests[] = {
		test_nt3h2111_io_socket_1,
		0
	};
	unit->name = "nt3h2111_io_socket";
	unit->description = "nt3h2111 io socket unit test";
	unit->tests = tests;
	unit->setup = setup_nt3h2111_io_socket_unit_test;
	unit->teardown = teardown_nt3h2111_io_socket_unit_test;
}


#endif /* IMPLEMENT_IO_DEVICE */
#endif
/*---------------------------------------------------------------------------
	Copyright (c) 2020 Gregor Bruce

   All rights reserved.
	
   Use of this copyrighted material in any form, with or without modification, 
   is only permitted on terms separately agreed in writing by the	user and 
	copyright holder.
   ---------------------------------------------------------------------------*/
