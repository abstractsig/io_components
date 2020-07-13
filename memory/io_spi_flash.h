/*
 *
 * Two wire ntag
 *
 */
#ifndef spi_nor_flash_H_
#define spi_nor_flash_H_

#define IO_SPI_SOCKET_STRUCT_MEMBERS \
	IO_SOCKET_STRUCT_MEMBERS \
	uint32_t maximum_speed; \
	io_pin_t mosi_pin; \
	io_pin_t miso_pin; \
	io_pin_t clock_pin; \
	io_pin_t slave_select_pin; \
	/**/

typedef struct PACK_STRUCTURE io_spi_socket {
	IO_SPI_SOCKET_STRUCT_MEMBERS
} io_spi_socket_t;

void shutdown_spi_nor_flash (io_socket_t*);
extern EVENT_DATA io_socket_implementation_t io_spi_socket_implementation;


INLINE_FUNCTION io_spi_socket_t*
cast_to_io_spi_socket (io_socket_t *socket) {
	if (is_io_socket_of_type (socket,&io_spi_socket_implementation)) {
		return (io_spi_socket_t*) socket;
	} else {
		return NULL;
	}
}

#ifdef IMPLEMENT_IO_DEVICE
//-----------------------------------------------------------------------------
//
// implementation
//
//-----------------------------------------------------------------------------

//
// serial mode 0, CPOL = 0, CPAH = 0
//
void
write_one_byte (io_t *io,io_spi_socket_t *spi,uint8_t byte) {
   uint32_t i;

	write_to_io_pin (io,spi->slave_select_pin,IO_PIN_ACTIVE);

   for (i = 0; i < 8; i++) {
     	write_to_io_pin (io,spi->clock_pin,IO_PIN_INACTIVE);
   	write_to_io_pin (io,spi->mosi_pin,(byte >> (7 - i)) & 0x01);
     	write_to_io_pin (io,spi->clock_pin,IO_PIN_ACTIVE);

   }

  	write_to_io_pin (io,spi->clock_pin,IO_PIN_INACTIVE);
	write_to_io_pin (io,spi->slave_select_pin,IO_PIN_INACTIVE);
}

//  To wake up we need to toggle the chip select at
//  least 20 ns and ten wait at least 35 us.

#define NOR_FLASH_COMMAND_DEEP_POWERDOWN			0xb9

void
shutdown_spi_nor_flash (io_socket_t *socket) {
	io_spi_socket_t *spi = cast_to_io_spi_socket (socket);
	if (spi) {
		io_t *io = io_socket_io(socket);

		io_set_pin_to_output (io,spi->slave_select_pin);
		io_set_pin_to_output (io,spi->mosi_pin);
		io_set_pin_to_output (io,spi->clock_pin);

		//
		// first wake the chip
		//
		// SS active for >= 35ns
		//
		write_to_io_pin (io,spi->slave_select_pin,IO_PIN_ACTIVE);
		write_to_io_pin (io,spi->slave_select_pin,IO_PIN_ACTIVE);
		write_to_io_pin (io,spi->slave_select_pin,IO_PIN_INACTIVE);

		write_one_byte (io,spi,NOR_FLASH_COMMAND_DEEP_POWERDOWN);

		// really need to keep SPI SS inactive?
		//release_io_pin (io,spi->slave_select_pin);
		release_io_pin (io,spi->mosi_pin);
		release_io_pin (io,spi->clock_pin);
	}
}

EVENT_DATA io_socket_implementation_t io_spi_socket_implementation = {
	SPECIALISE_IO_SOCKET_IMPLEMENTATION (
		&io_physical_socket_implementation
	)
};

#endif /* IMPLEMENT_VERIFY_IO_DEVICE */
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

