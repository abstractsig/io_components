/*
 *
 * io graphics context for the ssd1306 display controller connected
 * via a I2C or SPI socket.
 *
 */
#ifndef io_ssd1306_H_
#define io_ssd1306_H_
#include <io_core.h>
#include <io_graphics.h>


#define SSD1306_IO_GRAPHICS_CONTEXT_STRUCT_MEMBERS \
	IO_GRAPHICS_CONTEXT_STRUCT_MEMBERS \
	io_t *io; \
	io_socket_t *ssd1306_socket; \
	uint32_t initialised; \
	io_colour_t current_drawing_colour; \
	uint32_t height_in_pixels; \
	uint32_t width_in_pixels; \
	uint8_t *oled_pixels; \
	io_graphics_command_stack_t *stack;\
	/**/

typedef struct PACK_STRUCTURE ssd1306_io_graphics_context {
	SSD1306_IO_GRAPHICS_CONTEXT_STRUCT_MEMBERS
} ssd1306_io_graphics_context_t;


io_graphics_context_t* mk_ssd1306_io_graphics_context (io_t*,io_socket_t*,uint32_t,uint32_t,uint32_t);
io_graphics_context_t* mk_ssd1306_io_graphics_context_twi (io_t*,io_socket_t*,uint32_t,uint32_t,uint32_t,uint32_t);


#ifdef IMPLEMENT_IO_COMPONENT_GRAPHICS_SSD1306

//-----------------------------------------------------------------------------
//
// implementation
//
//-----------------------------------------------------------------------------

static uint32_t ssd1306_io_graphics_context_get_width_in_pixels (io_graphics_context_t*);
static uint32_t ssd1306_io_graphics_context_get_height_in_pixels (io_graphics_context_t*);
static void	ssd1306_io_graphics_context_set_gamma_correction (io_graphics_context_t*,io_graphics_float_t);
static io_graphics_float_t ssd1306_io_graphics_context_get_gamma_correction (io_graphics_context_t*);
static void free_ssd1306_io_graphics_context (io_graphics_context_t*);
static void ssd1306_io_graphics_context_set_drawing_colour (io_graphics_context_t*,io_colour_t);
static io_colour_t ssd1306_io_graphics_context_get_drawing_colour (io_graphics_context_t*);
static void ssd1306_io_graphics_context_draw_pixel (io_graphics_context_t*,io_i32_point_t);
static void ssd1306_io_graphics_context_fill_rectangle (io_graphics_context_t*,io_i32_point_t,io_i32_point_t);
static void ssd1306_io_graphics_context_render_spi (io_graphics_context_t*);
static void ssd1306_io_graphics_context_begin (io_graphics_context_t*);
static void ssd1306_io_graphics_context_run (io_graphics_context_t*);

#define cast_to_ssd1306_context(gfx) (ssd1306_io_graphics_context_t*) (gfx)


EVENT_DATA io_graphics_context_implementation_t ssd1306_graphics_context_implementation = {
	.free = free_ssd1306_io_graphics_context,
	.select_font_by_name = io_graphics_context_select_ttf_font_by_name,
	.set_drawing_colour = ssd1306_io_graphics_context_set_drawing_colour,
	.get_drawing_colour = ssd1306_io_graphics_context_get_drawing_colour,
	.get_pixel = NULL,
	.fill = io_graphics_context_fill_with_colour,
	.fill_rectangle = ssd1306_io_graphics_context_fill_rectangle,
	.draw_pixel = ssd1306_io_graphics_context_draw_pixel,
	.draw_character = io_graphics_context_draw_character_with_current_font,
	.get_pixel_height = ssd1306_io_graphics_context_get_height_in_pixels,
	.get_pixel_width = ssd1306_io_graphics_context_get_width_in_pixels,
	.set_gamma_correction = ssd1306_io_graphics_context_set_gamma_correction,
	.get_gamma_correction = ssd1306_io_graphics_context_get_gamma_correction,
	.begin = ssd1306_io_graphics_context_begin,
	.run = ssd1306_io_graphics_context_run,
	.render = ssd1306_io_graphics_context_render_spi,
};

io_graphics_context_t*
initialise_ssd1306_io_graphics_context (
	ssd1306_io_graphics_context_t *this,
	io_t *io,
	io_socket_t *ssd,
	uint32_t pixel_width,
	uint32_t pixel_height,
	uint32_t stack_length
) {
	this->io = io;
	this->ssd1306_socket = ssd;
	this->width_in_pixels = pixel_width;
	this->height_in_pixels = pixel_height;
	this->initialised = 0;
	this->current_drawing_colour = IO_WHITE_1BIT_MONOCHROME;
	
	this->stack = mk_io_graphics_command_stack (
		io_get_byte_memory (io),stack_length
	);
	
	this->oled_pixels = NULL;
	
	return (io_graphics_context_t*) this;
}

io_graphics_context_t*
mk_ssd1306_io_graphics_context (
	io_t *io,io_socket_t *ssd,uint32_t pixel_width,uint32_t pixel_height,uint32_t stack_length
) {
	ssd1306_io_graphics_context_t *this = io_byte_memory_allocate (
		io_get_byte_memory (io),sizeof (ssd1306_io_graphics_context_t)
	);
	
	if (this) {
		this->implementation = &ssd1306_graphics_context_implementation;
		return initialise_ssd1306_io_graphics_context (
			this,io,ssd,pixel_width,pixel_height,stack_length
		);
	} else {
		return NULL;
	}
}

void
free_ssd1306_io_graphics_context (io_graphics_context_t *gfx) {
	ssd1306_io_graphics_context_t *this = cast_to_ssd1306_context (gfx);
	io_socket_close (this->ssd1306_socket);
	io_byte_memory_free (io_get_byte_memory (this->io),this);
}

static uint32_t
ssd1306_io_graphics_context_get_width_in_pixels (io_graphics_context_t *gfx) {
	ssd1306_io_graphics_context_t *this = cast_to_ssd1306_context (gfx);
	return this->width_in_pixels;
}

static uint32_t
ssd1306_io_graphics_context_get_height_in_pixels (
	io_graphics_context_t *gfx
) {
	ssd1306_io_graphics_context_t *this = cast_to_ssd1306_context (gfx);
	return this->height_in_pixels;
}

static void
ssd1306_io_graphics_context_set_drawing_colour (
	io_graphics_context_t *gfx,io_colour_t colour
) {
	ssd1306_io_graphics_context_t *this = cast_to_ssd1306_context (gfx);
	this->current_drawing_colour = convert_io_colour (
		colour,this->current_drawing_colour
	);
}

static io_colour_t
ssd1306_io_graphics_context_get_drawing_colour (io_graphics_context_t *gfx) {
	ssd1306_io_graphics_context_t *this = cast_to_ssd1306_context (gfx);
	return this->current_drawing_colour;
}

static void
ssd1306_io_graphics_context_set_gamma_correction (
	io_graphics_context_t *gfx,io_graphics_float_t g
) {
}

static io_graphics_float_t
ssd1306_io_graphics_context_get_gamma_correction (io_graphics_context_t *gfx) {
	return 0;
}

static void
ssd1306_io_graphics_context_fill_rectangle (
	io_graphics_context_t *gfx,io_i32_point_t p1,io_i32_point_t p2
) {
	ssd1306_io_graphics_context_t *this = cast_to_ssd1306_context (gfx);
	if (this->oled_pixels && io_points_not_equal(p1,p2)) {
		int dx = p2.x - p1.x;
		int dy = p2.y - p1.y;
		int ix = dx < 0 ? -1 : 1;
		int iy = dy < 0 ? -1 : 1;
		int xe = p1.x + dx + ix;
		int ye = p1.y + dy + iy;
		int y = p1.y;
		uint8_t colour = io_colour_monochrome_level (
			this->current_drawing_colour
		);
		
		do {
			int x = p1.x;
			int yy = (y/8 * this->width_in_pixels);
			do {
				this->oled_pixels[x + yy] &= ~(1 << (y % 8));
				this->oled_pixels[x + yy] |= colour << (y % 8);
				x += ix;
			} while (x != xe);
			y += iy;
		} while (y != ye);
	}
}

static void
ssd1306_io_graphics_context_draw_pixel (
	io_graphics_context_t *gfx,io_i32_point_t pt
) {
	ssd1306_io_graphics_context_t *this = cast_to_ssd1306_context (gfx);
	if (
			this->oled_pixels
		&&	(pt.x >= 0 && pt.x < io_graphics_context_get_width_in_pixels(gfx))
		&&	(pt.y >= 0 && pt.y < io_graphics_context_get_height_in_pixels(gfx))
	) {
		uint8_t colour = io_colour_monochrome_level (
			this->current_drawing_colour
		);
		this->oled_pixels[pt.x + (pt.y/8 * this->width_in_pixels)] = colour << (pt.y % 8);
	}
}

static void
ssd1306_io_graphics_context_begin (io_graphics_context_t *gfx) {
	ssd1306_io_graphics_context_t *this = (ssd1306_io_graphics_context_t*) gfx;
	reset_io_graphics_command_stack (this->stack);
}

static void
ssd1306_io_graphics_context_run (io_graphics_context_t *gfx) {
}

void
ssd1306_io_graphics_context_render_spi (io_graphics_context_t *gfx) {
	// to be the spi version ...
}

//
// Two wire interface
//

typedef struct PACK_STRUCTURE ssd1306_io_graphics_context_twi {
	SSD1306_IO_GRAPHICS_CONTEXT_STRUCT_MEMBERS
	uint32_t bus_address;
	
	io_event_t command_complete;
	
	uint16_t const *current_command;
	uint16_t const *end_of_command_sequence;
	void (*end_of_command_action) (struct ssd1306_io_graphics_context_twi*);
} ssd1306_io_graphics_context_twi_t;

static void ssd1306_io_graphics_context_render_twi (io_graphics_context_t*);

static io_graphics_command_stack_t*
ssd1306_io_graphics_context_twi_get_command_stack (io_graphics_context_t *gfx) {
	ssd1306_io_graphics_context_twi_t *this = (ssd1306_io_graphics_context_twi_t*) gfx;
	return this->stack;
}

EVENT_DATA io_graphics_context_implementation_t ssd1306_graphics_context_twi_implementation = {
	.free = free_ssd1306_io_graphics_context,
	.select_font_by_name = io_graphics_context_select_ttf_font_by_name,
	.set_drawing_colour = ssd1306_io_graphics_context_set_drawing_colour,
	.get_drawing_colour = ssd1306_io_graphics_context_get_drawing_colour,
	.get_pixel = NULL,
	.get_command_stack = ssd1306_io_graphics_context_twi_get_command_stack,
	.fill = io_graphics_context_fill_with_colour,
	.fill_rectangle = ssd1306_io_graphics_context_fill_rectangle,
	.draw_pixel = ssd1306_io_graphics_context_draw_pixel,
	.draw_character = io_graphics_context_draw_character_with_current_font,
	.get_pixel_height = ssd1306_io_graphics_context_get_height_in_pixels,
	.get_pixel_width = ssd1306_io_graphics_context_get_width_in_pixels,
	.set_gamma_correction = ssd1306_io_graphics_context_set_gamma_correction,
	.get_gamma_correction = ssd1306_io_graphics_context_get_gamma_correction,
	.begin = ssd1306_io_graphics_context_begin,
	.run = ssd1306_io_graphics_context_run,
	.render = ssd1306_io_graphics_context_render_twi,
};

//#define SSD1306_EXTERNALVCC				0x1
//#define SSD1306_SWITCHCAPVCC			0x2

#define SSD1306_MEMORYMODE					0x20
#define SSD1306_COLUMNADDR					0x21
#define SSD1306_PAGEADDR					0x22

#define SSD1306_SETSTARTLINE				0x40

#define SSD1306_SETCONTRAST				0x81
#define SSD1306_CHARGEPUMP					0x8D

#define SSD1306_SEGREMAP					0xA0
#define SSD1306_DISPLAYALLON_RESUME		0xA4
#define SSD1306_DISPLAYALLON				0xA5
#define SSD1306_NORMALDISPLAY				0xA6
#define SSD1306_INVERTDISPLAY				0xA7
#define SSD1306_SETMULTIPLEX				0xA8
#define SSD1306_DISPLAYOFF					0xAE
#define SSD1306_DISPLAYON					0xAF

#define SSD1306_COMSCANINC					0xC0
#define SSD1306_COMSCANDEC					0xC8

#define SSD1306_SETDISPLAYOFFSET			0xD3
#define SSD1306_SETDISPLAYCLOCKDIV		0xD5
#define SSD1306_SETPRECHARGE				0xD9
#define SSD1306_SETCOMPINS					0xDA
#define SSD1306_SETVCOMDETECT				0xDB

#define SSD1306_ACTIVATE_SCROLL								0x2F
#define SSD1306_DEACTIVATE_SCROLL							0x2E
#define SSD1306_SET_VERTICAL_SCROLL_AREA					0xA3
#define SSD1306_RIGHT_HORIZONTAL_SCROLL					0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL						0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL	0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL	0x2A

// extra commands .. consider using a uint16_t!
#define X_SSD1306_HEIGHT						0x1f0
#define X_SSD1306_HEIGHT_DIV_8				0x1f1
#define X_SSD1306_WIDTH							0x1f2

static const uint16_t startup_commands[] = {
	SSD1306_DISPLAYOFF,
	SSD1306_SETDISPLAYCLOCKDIV,
	0x80,
	SSD1306_SETMULTIPLEX,
	X_SSD1306_HEIGHT,
	SSD1306_SETDISPLAYOFFSET,
	0,
	SSD1306_SETSTARTLINE | 0x0,
	SSD1306_CHARGEPUMP,
	0x14,
	SSD1306_MEMORYMODE,
	0,	// horizontal
	SSD1306_SEGREMAP | 0x1,
	SSD1306_COMSCANDEC,
	SSD1306_SETCOMPINS,
	2,
	SSD1306_SETCONTRAST,
	0x8f,
	SSD1306_SETPRECHARGE,
	0xf1,
	SSD1306_SETVCOMDETECT,
	0x40,
	SSD1306_DISPLAYALLON_RESUME,
	SSD1306_NORMALDISPLAY,
	SSD1306_DEACTIVATE_SCROLL,
	SSD1306_DISPLAYON,
};

static void
ssd1306_io_graphics_display_initialised (ssd1306_io_graphics_context_twi_t *this) {
	this->initialised = 1;
	
	#if 0
	// just testing
	io_graphics_context_t *gfx = (io_graphics_context_t*) this;
	io_graphics_context_begin (gfx);
	io_graphics_stack_append_line (
		io_graphics_context_get_command_stack(gfx),
		def_i32_point(0,0),
		def_i32_point(16,16)
	);

	io_graphics_stack_append_rectangle (
		io_graphics_context_get_command_stack(gfx),
		def_i32_point(32,0),def_i32_point(3,2),true
	);
	#endif
	
	// this clears display
	io_graphics_context_render ((io_graphics_context_t*) this);
}

static void
ssd1306_command (io_socket_t *twi,uint8_t command_byte) {
	io_encoding_t *message = io_socket_new_message (twi);
	if (message) {
		io_twi_transfer_t *cmd = get_twi_layer (message);
		
		io_twi_transfer_tx_length(cmd) = 2;
		io_twi_transfer_rx_length(cmd) = 0;

		io_binary_encoding_append_byte (message,0);// Co = 0, D/C = 0
		io_binary_encoding_append_byte (message,command_byte);
		
		io_socket_send_message (twi,message);
	}
}

static void
ssd1306_io_graphics_context_twi_command_complete (io_event_t *ev) {
	ssd1306_io_graphics_context_twi_t *this = ev->user_value;

	if (this->current_command < this->end_of_command_sequence) {
		uint16_t cmd = *this->current_command++;
		switch (cmd) {
			case X_SSD1306_HEIGHT:
				cmd = this->height_in_pixels - 1;
			break;
			
			case X_SSD1306_HEIGHT_DIV_8:
				cmd = (this->height_in_pixels/8) - 1;
			break;
			
			case X_SSD1306_WIDTH:
				cmd = this->width_in_pixels - 1;
			break;

		}
		ssd1306_command (this->ssd1306_socket,cmd);
	} else {
		this->end_of_command_action (this);
	}
}

static void
ssd1306_io_graphics_context_initialise_twi (ssd1306_io_graphics_context_twi_t *this) {
	if (io_socket_open (this->ssd1306_socket)) {
		io_socket_t *twi = this->ssd1306_socket;

		io_socket_open (twi);
		
		this->current_command = startup_commands;
		this->end_of_command_sequence = startup_commands + SIZEOF(startup_commands);
		this->end_of_command_action = ssd1306_io_graphics_display_initialised;
		
		ssd1306_command (twi,*this->current_command++);
	}
}

static void
ssd1306_io_graphics_end_of_pixels (ssd1306_io_graphics_context_twi_t *this) {
}

static void
ssd1306_io_graphics_context_render_twi (io_graphics_context_t *gfx) {
	ssd1306_io_graphics_context_twi_t *this = (ssd1306_io_graphics_context_twi_t*) (gfx);
	io_socket_t *twi = this->ssd1306_socket;
	io_encoding_t *message = io_socket_new_message (twi);

	if (message) {
		io_twi_transfer_t *cmd = get_twi_layer (message);

		uint32_t bitmap_size = (this->width_in_pixels * this->height_in_pixels/8);
		io_twi_transfer_rx_length(cmd) = 0;

		io_binary_encoding_append_byte (message,0x80);
		io_binary_encoding_append_byte (message,SSD1306_COLUMNADDR);
		
		io_binary_encoding_append_byte (message,0x80);
		io_binary_encoding_append_byte (message,0x00);
		
		io_binary_encoding_append_byte (message,0x80);
		io_binary_encoding_append_byte (message,this->width_in_pixels - 1);

		io_binary_encoding_append_byte (message,0x80);
		io_binary_encoding_append_byte (message,SSD1306_PAGEADDR);

		io_binary_encoding_append_byte (message,0x80);
		io_binary_encoding_append_byte (message,0x00);

		io_binary_encoding_append_byte (message,0x80);
		io_binary_encoding_append_byte (message,(this->height_in_pixels/8) - 1);

		io_binary_encoding_append_byte (message,0x40);// Co = 0, D/C = 1

		uint32_t pixel_offset = io_encoding_length (message);
		io_encoding_fill (message,0x00,bitmap_size);
		this->oled_pixels = io_encoding_get_byte_stream (message) + pixel_offset;
		io_twi_transfer_tx_length(cmd) = pixel_offset + bitmap_size;

		io_graphics_command_t **cursor = io_graphics_command_stack_begin(this->stack);
		while (cursor < io_graphics_command_stack_end (this->stack)) {
			run_io_graphics_command (*cursor++,(io_graphics_context_t*) this);
		}
		this->oled_pixels = NULL;
		this->end_of_command_action = ssd1306_io_graphics_end_of_pixels;
		io_socket_send_message (twi,message);
	}
}

io_graphics_context_t*
mk_ssd1306_io_graphics_context_twi (
	io_t *io,
	io_socket_t *ssd,
	uint32_t pixel_width,
	uint32_t pixel_height,
	uint32_t bus_address,
	uint32_t stack_length
) {
	ssd1306_io_graphics_context_twi_t *this = io_byte_memory_allocate (
		io_get_byte_memory (io),sizeof (ssd1306_io_graphics_context_twi_t)
	);
	
	if (this) {
		this->implementation = &ssd1306_graphics_context_twi_implementation;
		this->bus_address = bus_address;
		this->current_command = NULL;
		
		initialise_io_event (
			&this->command_complete,ssd1306_io_graphics_context_twi_command_complete,this
		);
		
		initialise_ssd1306_io_graphics_context (
			(ssd1306_io_graphics_context_t*) this,io,ssd,pixel_width,pixel_height,stack_length
		);
		
		io_socket_bind_inner (
			ssd,def_io_u8_address(bus_address),&this->command_complete,NULL
		);

		ssd1306_io_graphics_context_initialise_twi (this);
		
		return (io_graphics_context_t*) this;
	} else {
		return NULL;
	}
}

#undef cast_to_ssd1306_context
#endif /* IMPLEMENT_IO_COMPONENT_GRAPHICS_SSD1306 */
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


