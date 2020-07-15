/*
 *
 * just for sniffing announcements for now
 *
 */
#ifndef io_bluetooth_layer_H_
#define io_bluetooth_layer_H_

//
// ble5 data link layer
//
typedef struct PACK_STRUCTURE io_ble5_socket {
	IO_LEAF_SOCKET_STRUCT_MEMBERS
} io_ble5_socket_t;

extern EVENT_DATA io_socket_implementation_t io_ble5_socket_implementation;


typedef struct PACK_STRUCTURE io_ble5_packet {
	IO_LAYER_STRUCT_PROPERTIES

	uint32_t rssi;

} io_ble5_packet_t;


extern EVENT_DATA io_layer_implementation_t io_ble5_layer_implementation;
io_layer_t* push_io_ble5_layer (io_encoding_t*);

#define BLE5_ADV_IND				0x0	// LE1M phy
#define BLE5_ADV_DIRECT_IND	0x1	// LE1M phy
#define BLE5_ADV_NONCONN_IND	0x2	// LE1M phy
#define BLE5_SCAN_REQ			0x3	// LE1M phy
#define BLE5_AUX_SCAN_REQ		0x3	// all phys
#define BLE5_SCAN_RESP			0x4	// LE1M phy
#define BLE5_CONNECT_IND		0x5	// LE1M phy
#define BLE5_AUX_CONNECT_REQ	0x5	// all phys
#define BLE5_ADV_SCAN_IND		0x6	// LE1M phy

typedef struct PACK_STRUCTURE {
	struct PACK_STRUCTURE {
		uint8_t RxAdd:1;
		uint8_t TxAdd:1;
		uint8_t chset:1;
		uint8_t rfu:1;
		uint8_t type:4;
	} flags;
	uint8_t length;
	uint8_t payload[];
} ble5_packet_t;

#define ble5_packet_type(p)	(p)->flags.type
#define ble5_packet_length(p)	(p)->length

#ifdef IMPLEMENT_IO_BLE5
//-----------------------------------------------------------------------------
//
// implementation
//
//-----------------------------------------------------------------------------
//
// ble5 socket
//

static void
io_ble5_socket_outer_receive_event (io_event_t *ev) {
}

static void
io_ble5_socket_outer_transmit_event (io_event_t *ev) {
}

io_socket_t*
io_ble5_socket_initialise (io_socket_t *socket,io_t *io,io_settings_t const *C) {
	io_ble5_socket_t *this = (io_ble5_socket_t*) socket;

	initialise_io_leaf_socket ((io_leaf_socket_t*) this,io,C);

	initialise_io_data_available_event (
		&this->receive_event,io_ble5_socket_outer_receive_event,this
	);

	initialise_io_transmit_available_event (
		&this->transmit_event,io_ble5_socket_outer_transmit_event,this
	);

	return socket;
}

EVENT_DATA io_socket_implementation_t
io_ble5_socket_implementation = {
	SPECIALISE_IO_LEAF_SOCKET_IMPLEMENTATION (
		&io_leaf_socket_implementation
	)
	.initialise = io_ble5_socket_initialise,
};

static void
free_io_ble5_layer (io_layer_t *layer,io_byte_memory_t *bm) {
	io_byte_memory_free (bm,layer);
}

EVENT_DATA io_layer_implementation_t
io_ble5_layer_implementation = {
	SPECIALISE_IO_LAYER_IMPLEMENTATION (&io_layer_implementation)
	.free = free_io_ble5_layer,
};

io_layer_t*
mk_ble5_layer (io_byte_memory_t *bm,io_encoding_t *packet) {
	io_ble5_packet_t *this = io_byte_memory_allocate (
		bm,sizeof(io_ble5_packet_t)
	);
	if (this) {
		this->implementation = &io_ble5_layer_implementation;
		this->layer_offset_in_byte_stream = io_encoding_length (packet);
		this->rssi = 0;
	}

	return (io_layer_t*) this;
}

io_layer_t*
push_io_ble5_layer (io_encoding_t *encoding) {
	return io_encoding_push_layer (encoding,mk_ble5_layer);
}

#endif /* IMPLEMENT_IO_BLE5 */
#endif
/*
Copyright 2020 Gregor Bruce

Permission to use, copy, modify, and/or distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright notice
and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
