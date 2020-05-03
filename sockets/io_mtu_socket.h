/*
 *
 * payload segmenter limits maximum payload size to outer mtu
 *
 */
#ifndef io_mtu_socket_H_
#define io_mtu_socket_H_
#include <io_core.h>


typedef struct PACK_STRUCTURE io_socket_segmenter {
	IO_SOCKET_STRUCT_MEMBERS
	
	io_socket_t *outer_socket;
	
} io_socket_segmenter_t;


#ifdef IMPLEMENT_IO_SOCKET_SEGMENTER
//-----------------------------------------------------------------------------
//
// implementaion
//
//-----------------------------------------------------------------------------

static io_socket_t*
io_socket_segmenter_initialise (
	io_socket_t *socket,io_t *io,io_settings_t const *C
) {
	io_socket_segmenter_t *this = (io_socket_segmenter_t*) socket;

	initialise_io_socket (socket,io);
	
	this->outer_socket = NULL;
	
	return socket;
}

static void
io_socket_segmenter_free (io_socket_t *socket) {
}

static bool
io_socket_segmenter_open (io_socket_t *socket) {
	return false;
}

static void
io_socket_segmenter_close (io_socket_t *socket) {
}

static bool
io_socket_segmenter_is_closed (io_socket_t const *socket) {
	return false;
}

static io_encoding_t*
io_socket_segmenter_new_message (io_socket_t *socket) {
	return NULL;
}

static bool
io_socket_segmenter_send_message (io_socket_t *socket,io_encoding_t *encoding) {
	return false;
}

bool
io_socket_segmenter_bind_inner (
	io_socket_t *socket,io_address_t a,io_event_t *tx,io_event_t *rx
) {
	return false;
}

static bool
io_socket_segmenter_bind_to_outer_socket (io_socket_t *socket,io_socket_t *outer) {
	return false;
}

EVENT_DATA io_socket_implementation_t io_socket_segmenter_implementation = {
	.specialisation_of = &io_socket_implementation_base,
	.initialise = io_socket_segmenter_initialise,
	.free = io_socket_segmenter_free,
	.open = io_socket_segmenter_open,
	.close = io_socket_segmenter_close,
	.is_closed = io_socket_segmenter_is_closed,
	.bind_inner = io_socket_segmenter_bind_inner,
	.bind_to_outer_socket = io_socket_segmenter_bind_to_outer_socket,
	.new_message = io_socket_segmenter_new_message,
	.send_message = io_socket_segmenter_send_message,
	.iterate_outer_sockets = NULL,
	.mtu = io_leaf_socket_mtu,
};


#endif /* IMPLEMENT_IO_SOCKET_SEGMENTER */
#endif
