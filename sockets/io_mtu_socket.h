/*
 *
 * content segmenter limits maximum stream length to outer mtu
 *
 */
#ifndef io_mtu_socket_H_
#define io_mtu_socket_H_
#include <io_core.h>

//
// is a procesing socket
//
typedef struct PACK_STRUCTURE io_mtu_socket {
	IO_ADAPTER_SOCKET_STRUCT_MEMBERS	
} io_mtu_socket_t;


#ifdef IMPLEMENT_IO_SOCKET_SEGMENTER
//-----------------------------------------------------------------------------
//
// implementaion
//
//-----------------------------------------------------------------------------

static io_socket_t*
io_mtu_socket_initialise (
	io_socket_t *socket,io_t *io,io_settings_t const *C
) {
	io_mtu_socket_t *this = (io_mtu_socket_t*) socket;

	initialise_io_socket (socket,io);
	
	this->outer_socket = NULL;
	
	return socket;
}

static void
io_mtu_socket_free (io_socket_t *socket) {
}

static bool
io_mtu_socket_open (io_socket_t *socket) {
	return false;
}

static void
io_mtu_socket_close (io_socket_t *socket) {
}

static bool
io_mtu_socket_is_closed (io_socket_t const *socket) {
	return false;
}

static io_encoding_t*
io_mtu_socket_new_message (io_socket_t *socket) {
	return NULL;
}

static bool
io_mtu_socket_send_message (io_socket_t *socket,io_encoding_t *encoding) {
	return false;
}

bool
io_mtu_socket_bind_inner (
	io_socket_t *socket,io_address_t a,io_event_t *tx,io_event_t *rx
) {
	return false;
}

static bool
io_mtu_socket_bind_to_outer_socket (io_socket_t *socket,io_socket_t *outer) {
	return false;
}

EVENT_DATA io_socket_implementation_t io_mtu_socket_implementation = {
	.specialisation_of = &io_socket_implementation_base,
	.initialise = io_mtu_socket_initialise,
	.reference = io_counted_socket_increment_reference,
	.free = io_mtu_socket_free,
	.open = io_mtu_socket_open,
	.close = io_mtu_socket_close,
	.is_closed = io_mtu_socket_is_closed,
	.bind_inner = io_mtu_socket_bind_inner,
	.bind_to_outer_socket = io_mtu_socket_bind_to_outer_socket,
	.new_message = io_mtu_socket_new_message,
	.send_message = io_mtu_socket_send_message,
	.iterate_inner_sockets = NULL,
	.iterate_outer_sockets = NULL,
	.mtu = io_adapter_socket_mtu,
};


#endif /* IMPLEMENT_IO_SOCKET_SEGMENTER */
#endif
