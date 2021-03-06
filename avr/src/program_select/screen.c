#include "screen.h"
#include "program_select.h"
#include "script.h"
#include "str2img.h"
#include "filebrowser.h"
#include "fpga.h"
#include <unistd.h>
#include "sram.h"
#include "button.h"
#include "timer.h"
#include "compiler.h"


// SCREEN STRINGS
#define SCREEN_LINE_SELECT_PROGRAM 		"|~~~~~~~~~~[ SELECT PROGRAM ]~~~~~~~~~~|"
#define SCREEN_LINE_SELECT_DATA			"|~~~~~~~~~~~[ SELECT DATA ]~~~~~~~~~~~~|"
#define SCREEN_LINE_ERROR				"|~~~~~~~~~~~~~~[ ERROR! ]~~~~~~~~~~~~~~|"
#define SCREEN_LINE_BOTTOM				"|~~~~~~~~~~256 SHADES OF GRAY~~~~~~~~~~|"
#define SCREEN_LINE_MORE_DATA			"                               more...  "
#define SCREEN_LINE_EMPTY				"                                        "
#define SCREEN_PREFIX_ITEM				' '
#define SCREEN_PREFIX_SELECTED_ITEM 	'*'

struct set {
	int first;
	int last;
} visible_items;

extern int menu_item_selected;
extern enum data_type current_type;
extern int menu_size;

volatile bool halt;
void wait_for_click(U8 b) {
	LED_Toggle(b);
	halt = FALSE;
}

void screen_display_error_message(char *message) {
#define ERROR_MSG_SLEEP 3000
	str2img_clear();
	str2img_set_cursor(0,0);
	str2img_writeline(SCREEN_LINE_ERROR);
	str2img_writeline(SCREEN_LINE_EMPTY);
	str2img_write(message);
	str2img_write("\n\n");
	//str2img_write("Push any button to continue...");
	str2img_set_cursor(SCREEN_HEIGHT-1, 0);
	str2img_write(SCREEN_LINE_BOTTOM);
	screen_draw_bitmap_on_screen();

	/*button_set_tmp_listener(&wait_for_click);
	halt = TRUE;
	while (halt);	// Wait for click
	LED_On(LED7);
	button_remove_tmp_listener();*/
	timer_sleep(ERROR_MSG_SLEEP);

	screen_load_data_to_bitmap(current_type);
	screen_draw_bitmap_on_screen();
}

//#define BUFFER_ADRESS (SRAM + STR2IMG_BUFFER_SIZE)
void screen_draw_bitmap_on_screen(void) {
	U8 *buffer = SCREEN_BITMAP;
	str2img_read_block(buffer);
	fpga_send_data_from_memory(buffer, PICTURE_SIZE);
}

/**
 * Moves the cursor by manipulating the current bitmap
 */
void screen_move_cursor(S8 direction) {
	int prev_sel = menu_item_selected;
	menu_item_selected += direction;

	if (menu_item_selected >= visible_items.first && menu_item_selected <= visible_items.last) {
		str2img_set_cursor(SCREEN_ITEMS_OFFSET + prev_sel-visible_items.first, 0);
		str2img_putc(SCREEN_PREFIX_ITEM);

		str2img_set_cursor(SCREEN_ITEMS_OFFSET + menu_item_selected-visible_items.first, 0);
		str2img_putc(SCREEN_PREFIX_SELECTED_ITEM);
	} else {
		screen_load_data_to_bitmap(current_type);
	}

	screen_draw_bitmap_on_screen();
}

void screen_load_data_to_bitmap(enum data_type type) {
	int i,j,rc;

	// Clear screen
	str2img_clear();
	str2img_set_cursor(0,0);

	// Set top line of screen
	if (type == DATA) {
		str2img_write(SCREEN_LINE_SELECT_DATA);
	} else if (type == PROGRAM) {
		str2img_write(SCREEN_LINE_SELECT_PROGRAM);
	}

	// Determine scroll
	if (menu_size > SCREEN_MAX_ITEMS) {
		int remaining_items = menu_size - menu_item_selected-1;
		int remaining_spots = Min(remaining_items, (SCREEN_MAX_ITEMS-1));
		int preceeding_spots = (SCREEN_MAX_ITEMS-1) - remaining_spots;
		visible_items.first = menu_item_selected - preceeding_spots;

		if (visible_items.first > 0) {	// More data above
			str2img_write(SCREEN_LINE_MORE_DATA);
		} else {						// No data above
			str2img_write(SCREEN_LINE_EMPTY);
		}
		visible_items.last = visible_items.first + remaining_spots;

	} else {
		visible_items.first = 0;
		visible_items.last = menu_size -1;
		str2img_write(SCREEN_LINE_EMPTY);
	}

	// Seek to the first file
	if (menu_size > 0) {
		rc = fb_iterator_seek(visible_items.first);
		if (rc) {
			screen_display_error_messagef(" Error, could not seek to file %d\n Return code is %d\n",visible_items.first, rc);
		}
		// Gets file names and write them to bitmap
		for (i=0, j=visible_items.first; i < SCREEN_MAX_ITEMS; ++i, ++j) {
			if (fb_iterator_has_next()) {
				if (j == menu_item_selected) {				// Adds prefix
					str2img_putc(SCREEN_PREFIX_SELECTED_ITEM);
				} else {
					str2img_putc(SCREEN_PREFIX_ITEM);
				}
				str2img_putc(' ');
				char *line_name = fb_iterator_next();
				str2img_writeline(line_name);

			} else {		// No more files
				str2img_write(SCREEN_LINE_EMPTY);
			}
		}
	} else {
		str2img_writeline("  no files");
		str2img_set_cursor(SCREEN_HEIGHT-2,0);
	}

	// Sets bottom of screen
	if (visible_items.last < menu_size-3){
		str2img_write(SCREEN_LINE_MORE_DATA);
	} else {
		str2img_write(SCREEN_LINE_EMPTY);
	}
	str2img_write(SCREEN_LINE_BOTTOM);
	fb_iterator_terminate();
}
