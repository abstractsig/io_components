/*
 *
 * beacons
 *
 */
#ifndef io_beacon_socket_H_
#define io_beacon_socket_H_
#include <io_core.h>
#ifdef IMPLEMENT_IO_CORE
# ifndef IMPLEMENT_IO_BEACON_SOCKET
#  define IMPLEMENT_IO_BEACON_SOCKET
# endif
#endif

#define IO_BEACON_FRAME_INNER_ADDRESS_LIMIT	5

typedef struct io_beacon_socket io_beacon_socket_t;

//
// internal socket that does not support inner bindings
//
#define IO_LEAF_SOCKET_STRUCT_MEMBERS \
	IO_COUNTED_SOCKET_STRUCT_MEMBERS \
	io_event_t transmit_event; \
	io_event_t receive_event; \
	io_encoding_pipe_t *receive_pipe;\
	io_socket_t *outer_socket; \
	io_inner_constructor_bindings_t *inner_constructors;\
	/**/

typedef struct PACK_STRUCTURE {
	IO_LEAF_SOCKET_STRUCT_MEMBERS
} io_leaf_socket_t;

bool io_leaf_socket_bind_inner_constructor (io_socket_t *socket,io_address_t address,io_socket_constructor_t make,io_notify_event_t *);


struct PACK_STRUCTURE io_beacon_socket {
	IO_LEAF_SOCKET_STRUCT_MEMBERS
	
//	io_event_t open_event;
	io_event_t close_event;
	
	io_time_t interval;
	
	io_event_t timer_event;
	io_event_t timer_error;
	io_alarm_t beacon_timer;
	
	io_socket_constructor_t make;
	io_notify_event_t *notify;
	
};

io_socket_t* 	allocate_io_beacon_socket (io_t*,io_address_t);
io_layer_t*		push_io_beacon_transmit_layer (io_encoding_t*);
io_layer_t*		push_io_beacon_receive_layer (io_encoding_t*);
io_layer_t*		get_io_beacon_layer (io_encoding_t*);
bool				io_beacon_socket_set_interval (io_socket_t*,io_time_t);

extern EVENT_DATA io_socket_implementation_t io_beacon_socket_implementation;

INLINE_FUNCTION io_beacon_socket_t*
cast_to_io_beacon_socket (io_socket_t *socket) {
	if (is_io_socket_of_type (socket,&io_beacon_socket_implementation)) {
		return (io_beacon_socket_t*) socket;
	} else {
		return NULL;
	}
}

#ifdef IMPLEMENT_IO_BEACON_SOCKET
//-----------------------------------------------------------------------------
//
// implementaion
//
//-----------------------------------------------------------------------------

void
initialise_io_leaf_socket (io_leaf_socket_t *this,io_t *io,io_settings_t const *C) {

	initialise_io_counted_socket ((io_counted_socket_t*) this,io);

	this->outer_socket = NULL;
	this->inner_constructors = mk_io_inner_constructor_bindings(io);
	this->receive_pipe = mk_io_encoding_pipe (
		io_get_byte_memory(io),io_settings_receive_pipe_length(C)
	);

}

void
io_leaf_socket_free (io_socket_t *socket) {
	io_leaf_socket_t *this = (io_leaf_socket_t *) socket;
	free_io_encoding_pipe (this->receive_pipe,io_socket_byte_memory(socket));
	free_io_inner_constructor_bindings (this->inner_constructors,io_socket_io (this));
	io_counted_socket_free (socket);
}

bool
io_leaf_socket_bind_inner_constructor (
	io_socket_t *socket,io_address_t address,io_socket_constructor_t make,io_notify_event_t *notify
) {
	io_leaf_socket_t *this = (io_leaf_socket_t*) socket;
	return io_inner_constructor_bindings_bind (
		this->inner_constructors,io_socket_io (this),address,make,notify
	);
}

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *
 * beacon states
 *
 *               io_beacon_socket_state_closed <-----.
 *                 |                                 |                             
 *                 v                                 |
 *         .---> io_beacon_socket_state_send_delay   |
 *         |       |                                 |                             
 *         |       v                                 |
 *         |     io_beacon_socket_state_send         |
 *         |       |                                 | 
 *         |       v                                 |
 *         |     io_beacon_socket_state_wait  ->-----+
 *         |       |                                 |                            
 *         |       v                                 |
 *         |     io_beacon_socket_state_sleep ->-----'
 *         |       |                             
 *         `-------'
 *
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */
static EVENT_DATA io_socket_state_t io_beacon_socket_state_closed;
static EVENT_DATA io_socket_state_t io_beacon_socket_state_send_delay;
static EVENT_DATA io_socket_state_t io_beacon_socket_state_send;
static EVENT_DATA io_socket_state_t io_beacon_socket_state_wait;

static io_socket_state_t const*
io_beacon_socket_state_closed_open (
	io_socket_t *socket,io_socket_open_flag_t flag
) {
	return &io_beacon_socket_state_send;
}

static EVENT_DATA io_socket_state_t io_beacon_socket_state_closed = {
	SPECIALISE_IO_SOCKET_STATE (&io_socket_state)
	.open = io_beacon_socket_state_closed_open,
};

void
set_alarm_time (io_t *io,io_alarm_t *alarm,io_time_t delay) {
	io_time_t t = io_get_time (io);
	alarm->when = (io_time_t) {t.ns + delay.ns};
}

static io_socket_state_t const*
io_beacon_socket_state_send_delay_enter (io_socket_t *socket) {
	io_beacon_socket_t *this = (io_beacon_socket_t*) socket;
	io_t *io = io_socket_io (this);
	
	uint32_t rand = io_get_next_prbs_u32(io) % 32;
	
	set_alarm_time (io,&this->beacon_timer,millisecond_time(20 + rand));
	io_enqueue_alarm (io,&this->beacon_timer);
	
	return socket->State;
}

static io_socket_state_t const*
io_beacon_socket_state_send_delay_done (io_socket_t *socket) {
	return &io_beacon_socket_state_send;
}

static EVENT_DATA io_socket_state_t io_beacon_socket_state_send_delay = {
	SPECIALISE_IO_SOCKET_STATE (&io_socket_state)
	.enter = io_beacon_socket_state_send_delay_enter,
	.timer = io_beacon_socket_state_send_delay_done,
};

static io_socket_state_t const*
io_beacon_socket_state_send_enter (io_socket_t *socket) {
	io_encoding_t *message = io_socket_new_message (socket);
	if (message) {
		io_layer_t *layer = get_io_beacon_layer (message);
		if (layer) {
			io_layer_load_header (layer,message);
			io_socket_send_message (socket,message);
			return &io_beacon_socket_state_wait;
		}
	}
	return socket->State;
}

static EVENT_DATA io_socket_state_t io_beacon_socket_state_send = {
	SPECIALISE_IO_SOCKET_STATE (&io_socket_state)
	.enter = io_beacon_socket_state_send_enter,
};

static io_socket_state_t const*
io_beacon_socket_state_wait_enter (io_socket_t *socket) {
	return socket->State;
}

static io_socket_state_t const*
io_beacon_socket_state_wait_receive (io_socket_t *socket) {
	io_beacon_socket_t *this = (io_beacon_socket_t*) socket;
	io_encoding_t *next;
	
	while (io_encoding_pipe_peek (this->receive_pipe,&next)) {
		io_layer_t *layer = get_io_beacon_layer (next);
		io_layer_t *outer = io_encoding_get_outer_layer (next,layer);
		if (outer) {
			io_layer_get_source_address (outer,next);
		}
	
		io_encoding_pipe_pop_encoding (this->receive_pipe);
	}
	
	return this->State;
}

static io_socket_state_t const*
io_beacon_socket_state_wait_transmit (io_socket_t *socket) {
	return socket->State;
}

static EVENT_DATA io_socket_state_t io_beacon_socket_state_wait = {
	SPECIALISE_IO_SOCKET_STATE (&io_socket_state)
	.enter = io_beacon_socket_state_wait_enter,
	.receive = io_beacon_socket_state_wait_receive,
	.transmit = io_beacon_socket_state_wait_transmit,
};

//
// now the socket
//
static void
io_beacon_socket_beacon_timer_event (io_event_t *ev) {
	io_socket_t *socket = ev->user_value;
	io_socket_call_state (socket,socket->State->timer);
}

static void
io_beacon_socket_timer_error (io_event_t *ev) {
}

static void
io_beacon_socket_outer_transmit_event (io_event_t *ev) {
	io_socket_t *socket = ev->user_value;
	io_socket_call_state (socket,socket->State->transmit);
}

static void
io_beacon_socket_outer_receive_event (io_event_t *ev) {
	io_beacon_socket_t *this = ev->user_value;
	
	if (this->outer_socket) {
		io_encoding_pipe_t* rx = cast_to_io_encoding_pipe (
			io_socket_get_receive_pipe (
				this->outer_socket,io_socket_address (this)
			)
		);
		if (rx) {
			io_encoding_t *next;
			if (io_encoding_pipe_peek (rx,&next)) {
				io_layer_t *base = push_io_beacon_receive_layer (next);
				if (base) {
					if (io_encoding_pipe_put_encoding (this->receive_pipe,next)) {
						io_socket_call_state ((io_socket_t*)this,this->State->receive);
					}
				}
				io_encoding_pipe_pop_encoding (rx);
			}
		}
	}
}

static void
io_beacon_socket_open_event (io_event_t *ev) {
//	io_socket_t *this = ev->user_value;
//	io_socket_call_state (this,this->State->open);
	io_socket_open_event_t *open = (io_socket_open_event_t*) ev;	
	io_socket_t *socket = ev->user_value;
	io_socket_call_open (socket,open->flag);
	free_io_socket_open_event (io_socket_byte_memory (socket),open);
}

static void
io_beacon_socket_close_event (io_event_t *ev) {
	io_socket_t *socket = ev->user_value;
	io_socket_call_state (socket,socket->State->close);
}

static io_socket_t*
io_beacon_socket_initialise (io_socket_t *socket,io_t *io,io_settings_t const *C) {
	io_beacon_socket_t *this = (io_beacon_socket_t*) socket;

	initialise_io_leaf_socket ((io_leaf_socket_t*) socket,io,C);
	
	initialise_io_event (
		&this->receive_event,io_beacon_socket_outer_receive_event,this
	);

	initialise_io_event (
		&this->transmit_event,io_beacon_socket_outer_transmit_event,this
	);
	
	initialise_io_event (
		&this->close_event,io_beacon_socket_close_event,this
	);

	initialise_io_event (
		&this->timer_event,io_beacon_socket_beacon_timer_event,this
	);

	initialise_io_event (
		&this->timer_error,io_beacon_socket_timer_error,this
	);
	initialise_io_alarm (
		&this->beacon_timer,&this->timer_event,&this->timer_error,time_zero()
	);
	
	this->interval = time_zero();
	this->make = C->make;
	this->notify = C->notify;
	this->State = &io_beacon_socket_state_closed;
	
	io_socket_enter_current_state (socket);

	return socket;
}

static void
io_beacon_socket_free (io_socket_t *socket) {
	io_beacon_socket_t *this = (io_beacon_socket_t*) socket;

	io_dequeue_alarm (io_socket_io (this),&this->beacon_timer);
	
	io_leaf_socket_free (socket);
}

bool
io_beacon_socket_set_interval (io_socket_t *socket,io_time_t interval) {
	io_beacon_socket_t *this = cast_to_io_beacon_socket (socket);
	if (this) {
		bool cs = enter_io_critical_section (io_socket_io (socket));
		this->interval = interval;
		exit_io_critical_section (io_socket_io (socket),cs);
		return true;
	} else {
		return false;
	}
}

static bool
io_beacon_socket_open (io_socket_t *socket,io_socket_open_flag_t flag) {
	io_beacon_socket_t *this = (io_beacon_socket_t*)  (socket);
	if (
			this->interval.ns > 0 
		&& this->outer_socket 
		&& io_socket_open (this->outer_socket,flag)
	) {
		io_event_t *ev = mk_io_socket_open_event (
			io_socket_byte_memory(socket),IO_SOCKET_OPEN_CONNECT
		);
		initialise_io_event (ev,io_beacon_socket_open_event,this);
		io_enqueue_event (io_socket_io(this),ev);
		return true;
	} else {
		return false;
	}
}

static void
io_beacon_socket_close (io_socket_t *socket) {
	// unbind...
}

static bool
io_beacon_socket_is_closed (io_socket_t const *socket) {
	io_beacon_socket_t *this = (io_beacon_socket_t*)  (socket);
	return this->State == &io_beacon_socket_state_closed;
}

static io_encoding_t*
io_beacon_socket_new_message (io_socket_t *socket) {
	io_beacon_socket_t *this = (io_beacon_socket_t*) socket;
	if (this->outer_socket != NULL) {
		io_encoding_t *message = reference_io_encoding (
			io_socket_new_message (this->outer_socket)
		);
		io_layer_t *beacon = push_io_beacon_transmit_layer (message);
		
		if (beacon) {
			io_layer_t *outer = io_encoding_get_outer_layer (message,beacon);
			if (outer) {
				io_layer_set_destination_address (
					outer,message,io_layer_any_address(outer)
				);
				io_layer_set_inner_address (
					outer,message,io_socket_address(socket)
				);
			}
		} else {
			io_panic (io_socket_io (this),IO_PANIC_OUT_OF_MEMORY);
		}
		
		return message;
	} else {
		return NULL;
	}
}

static bool
io_beacon_socket_send_message (io_socket_t *socket,io_encoding_t *encoding) {
	io_beacon_socket_t *this = (io_beacon_socket_t*) socket;
	bool ok = false;
	
	if (this->outer_socket != NULL) {
		ok = io_socket_send_message (this->outer_socket,encoding);
	}
	
	unreference_io_encoding(encoding);
	return ok;
}

static bool
io_beacon_socket_bind (
	io_socket_t *socket,io_address_t a,io_event_t *tx,io_event_t *rx
) {
	return false;
}

static bool
io_beacon_socket_bind_to_outer (io_socket_t *socket,io_socket_t *outer) {
	io_beacon_socket_t *this = (io_beacon_socket_t*) socket;
	this->outer_socket = outer;
	return io_socket_bind_inner (
		outer,
		io_socket_address (socket),
		&this->transmit_event,
		&this->receive_event
	);
}

static size_t
io_beacon_socket_mtu (io_socket_t const *socket) {
	io_beacon_socket_t const *this = (io_beacon_socket_t const*) socket;
	if (this->outer_socket) {
		return io_socket_mtu (this->outer_socket);
	} else {
		return 0;
	}
}

EVENT_DATA io_socket_implementation_t io_beacon_socket_implementation = {
	.specialisation_of = &io_counted_socket_implementation,
	.reference = io_counted_socket_increment_reference,
	.initialise = io_beacon_socket_initialise,
	.free = io_beacon_socket_free,
	.open = io_beacon_socket_open,
	.close = io_beacon_socket_close,
	.is_closed = io_beacon_socket_is_closed,
	.bind_inner = io_beacon_socket_bind,
	.bind_inner_constructor = io_leaf_socket_bind_inner_constructor,
	.unbind_inner = io_virtual_socket_unbind_inner,
	.bind_to_outer_socket = io_beacon_socket_bind_to_outer,
	.new_message = io_beacon_socket_new_message,
	.send_message = io_beacon_socket_send_message,
	.get_receive_pipe = io_virtual_socket_get_receive_pipe,
	.iterate_outer_sockets = NULL,
	.iterate_inner_sockets = NULL,
	.mtu = io_beacon_socket_mtu,
};

io_socket_t*
allocate_io_beacon_socket (io_t *io,io_address_t address) {
	io_beacon_socket_t *socket = io_byte_memory_allocate (
		io_get_byte_memory (io),sizeof(io_beacon_socket_t)
	);
	socket->implementation = &io_beacon_socket_implementation;
	socket->address = duplicate_io_address (io_get_byte_memory (io),address);
	return (io_socket_t*) socket;
}

//
// layer
//

typedef struct PACK_STRUCTURE {
	IO_LAYER_STRUCT_PROPERTIES
} io_beacon_layer_t;

typedef struct PACK_STRUCTURE io_beacon_frame {
	uint8_t inner_address[IO_BEACON_FRAME_INNER_ADDRESS_LIMIT];
	uint8_t uid[IO_UID_BYTE_LENGTH];
	uint8_t time[sizeof(io_time_t)];
} io_beacon_frame_t;

static io_beacon_layer_t*
mk_io_beacon_layer (io_byte_memory_t *bm,io_encoding_t *packet) {
	extern EVENT_DATA io_layer_implementation_t io_beacon_layer_implementation;
	io_beacon_layer_t *this = io_byte_memory_allocate (
		bm,sizeof(io_beacon_layer_t)
	);

	if (this) {
		this->implementation = &io_beacon_layer_implementation;
	}
	
	return this;
}

static io_layer_t*
mk_io_beacon_transmit_layer (io_byte_memory_t *bm,io_encoding_t *packet) {
	io_beacon_layer_t *this = mk_io_beacon_layer (bm,packet);

	if (this) {
		this->layer_offset_in_byte_stream = io_encoding_length (packet);
		io_encoding_fill (packet,0,sizeof(io_beacon_frame_t));
	}
	
	return (io_layer_t*) this;
}

static io_layer_t*
mk_io_beacon_receive_layer (io_byte_memory_t *bm,io_encoding_t *packet) {
	io_beacon_layer_t *this = mk_io_beacon_layer (bm,packet);

	if (this) {
		this->layer_offset_in_byte_stream = io_encoding_increment_decode_offest (
			packet,sizeof(io_a4_link_frame_t)
		);
		io_beacon_frame_t *frame = io_layer_get_byte_stream ((io_layer_t*) this,packet);

		io_address_t address;
		uint8_t *read_cursor = frame->inner_address;
		uint8_t *end_of_buffer = read_cursor + IO_BEACON_FRAME_INNER_ADDRESS_LIMIT;
	
		address = io_invalid_address();
		read_cursor += read_le_io_address (bm,read_cursor,end_of_buffer - read_cursor,&address);
		io_layer_set_inner_address ((io_layer_t*) this,packet,address);
	}
	
	return (io_layer_t*) this;
}

static void
free_io_beacon_layer (io_layer_t *layer,io_byte_memory_t *bm) {
	io_byte_memory_free (bm,layer);
}

static io_address_t
io_beacon_layer_any_address (void) {
	return io_invalid_address();
}

static bool
io_beacon_layer_match_address (io_layer_t *layer,io_address_t address) {
	return compare_io_addresses (IO_BEACON_LAYER_ID,address) == 0;
}

static io_layer_t*
io_beacon_layer_push_receive_layer (io_layer_t *layer,io_encoding_t *encoding) {
	return push_io_beacon_receive_layer (encoding);
}

static io_inner_port_binding_t*
io_beacon_layer_select_inner_binding (
	io_layer_t *layer,io_encoding_t *encoding,io_socket_t* socket
) {
	return NULL;
}

static bool
io_beacon_layer_load_header (io_layer_t *layer,io_encoding_t *encoding) {
	io_beacon_frame_t *frame = io_layer_get_byte_stream (layer,encoding);
	io_t *io = io_encoding_get_io (encoding);
	
	if (io) {
	
		if (
			IO_BEACON_FRAME_INNER_ADDRESS_LIMIT >= write_le_io_address (
				frame->inner_address,
				IO_BEACON_FRAME_INNER_ADDRESS_LIMIT,
				io_layer_get_inner_address (layer,encoding)
			)
		) {
			memcpy (frame->uid,io_uid (io)->bytes,IO_UID_BYTE_LENGTH);
			memcpy (frame->time,io_get_time (io).bytes,IO_UID_BYTE_LENGTH);
			return true;
		}
	}

	return false;
}

static io_address_t
io_beacon_layer_get_source_address (
	io_layer_t *layer,io_encoding_t *message
) {
	return IO_BEACON_LAYER_ID;
}

static bool
io_beacon_layer_set_source_address (
	io_layer_t *layer,io_encoding_t *message,io_address_t local
) {
	return false;
}

static io_address_t
io_beacon_layer_get_destination_address (
	io_layer_t *layer,io_encoding_t *message
) {
	return IO_BEACON_LAYER_ID;
}

static bool
io_beacon_layer_set_destination_address (
	io_layer_t *layer,io_encoding_t *message,io_address_t local
) {
	return false;
}

static io_address_t
io_beacon_layer_get_inner_address (
	io_layer_t *layer,io_encoding_t *message
) {
	return IO_DLC_LAYER_ID;
}

static bool
io_beacon_layer_set_inner_address (
	io_layer_t *layer,io_encoding_t *message,io_address_t local
) {
	return false;
}

EVENT_DATA io_layer_implementation_t io_beacon_layer_implementation = {
	.specialisation_of = &io_layer_implementation,
	.any = io_beacon_layer_any_address,
	.free =  free_io_beacon_layer,
	.push_receive_layer =  io_beacon_layer_push_receive_layer,
	.select_inner_binding = io_beacon_layer_select_inner_binding,
	.match_address =  io_beacon_layer_match_address,
	.load_header = io_beacon_layer_load_header,
	.get_destination_address = io_beacon_layer_get_destination_address,
	.set_destination_address = io_beacon_layer_set_destination_address,
	.get_source_address = io_beacon_layer_get_source_address,
	.set_source_address = io_beacon_layer_set_source_address,
	.get_inner_address = io_beacon_layer_get_inner_address,
	.set_inner_address = io_beacon_layer_set_inner_address,
};

io_layer_t*
push_io_beacon_transmit_layer (io_encoding_t *encoding) {
	return io_encoding_push_layer_2 (encoding,mk_io_beacon_transmit_layer);
}

io_layer_t*
push_io_beacon_receive_layer (io_encoding_t *encoding) {
	//
	// we also decode encoding ...
	//
	return io_encoding_push_layer_2 (encoding,mk_io_beacon_receive_layer);
}

io_layer_t*
get_io_beacon_layer (io_encoding_t *encoding) {
	return io_encoding_get_layer (
		encoding,&io_beacon_layer_implementation
	);
}

#endif /* IMPLEMENT_IO_BEACON_SOCKET */
#ifdef IMPLEMENT_VERIFY_IO_BEACON_SOCKET
#define IMPLEMENT_IO_DLC_SOCKET
#include <sockets/io_dlc_socket.h>

void
test_io_beacon_socket_1_notify (io_event_t *ev) {
}

bool
test_io_beacon_socket_1_make_socket (
	io_t *io,io_address_t address,io_socket_t** inner,io_socket_t **outer
) {
	static const socket_builder_t build[] = {
		{0,allocate_io_dlc_socket,IO_DLC_LAYER_ID,NULL,false,NULL},
	};

	io_socket_t **stack = io_byte_memory_allocate (
		io_get_byte_memory(io),sizeof(io_socket_t*) * SIZEOF(build)
	);
	
	// need to allocate a socket
	
	build_io_sockets (io,stack,build,SIZEOF(build));
	
	inner = stack;
	outer = inner + (SIZEOF(build) - 1);
	
	return true;
}

TEST_BEGIN(test_io_beacon_socket_1) {
	io_byte_memory_t *bm = io_get_byte_memory (TEST_IO);
	memory_info_t bmbegin,bmend;
	const io_settings_t bus = {
		.transmit_pipe_length = 3,
		.receive_pipe_length = 3,
	};
	
	io_byte_memory_get_info (bm,&bmbegin);

	const socket_builder_t build[] = {
		{0,allocate_io_beacon_socket,IO_BEACON_LAYER_ID,&bus,false,BINDINGS({0,1},END_OF_BINDINGS)},
		{1,allocate_io_socket_link_emulator,def_io_u8_address(11),&bus,false,BINDINGS({1,2},END_OF_BINDINGS)},
		{2,allocate_io_shared_media,io_invalid_address(),&bus,false,NULL},
		{3,allocate_io_beacon_socket,IO_BEACON_LAYER_ID,&bus,false,BINDINGS({3,4},END_OF_BINDINGS)},
		{4,allocate_io_socket_link_emulator,def_io_u8_address(22),&bus,false,BINDINGS({4,2},END_OF_BINDINGS)},
	};
	
	io_socket_t* net[SIZEOF(build)];
	io_notify_event_t ev;
	
	initialise_io_notify (&ev,test_io_beacon_socket_1_notify,NULL,NULL);
	
	build_io_sockets(TEST_IO,net,build,SIZEOF(build));
	VERIFY (cast_to_io_counted_socket(net[0]) != NULL,NULL);

	if (
		VERIFY (
			io_socket_bind_inner_constructor (
				net[1],IO_DLC_LAYER_ID,test_io_beacon_socket_1_make_socket,&ev
			),
			NULL
		)
	) {
		VERIFY (io_socket_is_closed (net[0]),NULL);
		VERIFY (io_beacon_socket_set_interval (net[0],millisecond_time(10)),NULL);
		VERIFY (io_socket_open (net[0],IO_SOCKET_OPEN_CONNECT),NULL);

		VERIFY (io_beacon_socket_set_interval (net[3],millisecond_time(10)),NULL);
		//VERIFY (io_socket_open (net[3]),NULL);
		
		io_wait_for_all_events (TEST_IO);
	}
	
	free_io_sockets (net,net + SIZEOF(net));
	io_byte_memory_get_info (bm,&bmend);
	VERIFY (bmend.used_bytes == bmbegin.used_bytes,NULL);	
}
TEST_END


UNIT_SETUP(setup_io_beacon_socket_unit_test) {
	return VERIFY_UNIT_CONTINUE;
}

UNIT_TEARDOWN(teardown_io_beacon_socket_unit_test) {
}

static void
io_beacon_socket_unit_test (V_unit_test_t *unit) {
	static V_test_t const tests[] = {
		test_io_beacon_socket_1,
		0
	};
	unit->name = "io beacon sockets";
	unit->description = "io beacon sockets unit test";
	unit->tests = tests;
	unit->setup = setup_io_beacon_socket_unit_test;
	unit->teardown = teardown_io_beacon_socket_unit_test;
}

void
run_ut_io_beacon_socket (V_runner_t *runner) {
	static const unit_test_t test_set[] = {
		io_beacon_socket_unit_test,
		0
	};
	V_run_unit_tests(runner,test_set);
}
#endif /* IMPLEMENT_VERIFY_IO_BEACON_SOCKET */
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
