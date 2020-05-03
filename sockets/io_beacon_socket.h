/*
 *
 * beacons and followers should be layers
 *
 */
#ifndef io_beacon_socket_H_
#define io_beacon_socket_H_
#include <io_core.h>

typedef struct PACK_STRUCTURE io_beacon_socket {
	IO_SOCKET_STRUCT_MEMBERS
	
	io_time_t interval;
	
	io_event_t next_beacon;
	io_event_t beacon_interval_error;
	io_alarm_t beacon_timer;
	
} io_beacon_socket_t;

io_socket_t* allocate_io_beacon_socket (io_t*);

typedef struct PACK_STRUCTURE io_follower_socket {
	IO_SOCKET_STRUCT_MEMBERS
	
	io_time_t interval;
	
	io_event_t next_beacon;
	io_event_t beacon_interval_error;
	io_alarm_t beacon_timer;
	
} io_follower_socket_t;


#ifdef IMPLEMENT_IO_SOCKET_BEACON
//-----------------------------------------------------------------------------
//
// implementaion
//
//-----------------------------------------------------------------------------



static void	io_socket_beacon_next (io_event_t *ev) {
}

static void	io_socket_beacon_error (io_event_t *ev) {
}

static io_socket_t*
io_socket_beacon_initialise (io_socket_t *socket,io_t *io,io_settings_t const *C) {
	io_beacon_socket_t *this = (io_beacon_socket_t*) socket;

	initialise_io_socket (socket,io);

	initialise_io_event (
		&this->next_beacon,io_socket_beacon_next,this
	);
	initialise_io_event (
		&this->beacon_interval_error,io_socket_beacon_error,this
	);
	initialise_io_alarm (
		&this->beacon_timer,&this->next_beacon,&this->beacon_interval_error,time_zero()
	);

	return socket;
}

static void
io_socket_beacon_free (io_socket_t *socket) {
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
	.specialisation_of = &io_socket_implementation_base,
	.initialise = io_socket_beacon_initialise,
	.free = io_socket_beacon_free,
	.open = io_socket_beacon_open,
	.close = io_socket_beacon_close,
	.is_closed = io_socket_beacon_is_closed,
	.bind_inner = io_socket_beacon_bind_inner,
	.bind_to_outer_socket = io_socket_beacon_bind_to_outer_socket,
	.new_message = io_socket_beacon_new_message,
	.send_message = io_socket_beacon_send_message,
	.iterate_outer_sockets = NULL,
	.mtu = io_leaf_socket_mtu,
};


#endif /* IMPLEMENT_IO_SOCKET_BEACON */
#endif
