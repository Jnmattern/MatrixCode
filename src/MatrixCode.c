#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0x8B, 0x2A, 0x20, 0x41, 0x56, 0xC8, 0x4B, 0x9D, 0xAF, 0x7B, 0xE8, 0x89, 0x48, 0x96, 0xD6, 0x73 }
PBL_APP_INFO(MY_UUID,
             "Matrix Code", "Jnm",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_WATCH_FACE);

#define SCREENW 144
#define SCREENH 168
#define CX 72
#define CY 84
#define NUM_COLS 9
#define NUM_ROWS 7

#define NUM_BMP 7
const int bmpId[NUM_BMP] = {
	RESOURCE_ID_IMAGE_B1, RESOURCE_ID_IMAGE_B2, RESOURCE_ID_IMAGE_B3,
	RESOURCE_ID_IMAGE_B4, RESOURCE_ID_IMAGE_B5, RESOURCE_ID_IMAGE_B6,
	RESOURCE_ID_IMAGE_B7
};

#define MAX_NUM_NEW_CELLS 5

Window window;
bool clock12;

typedef struct {
	char text[2];
	TextLayer textLayer;
	BitmapLayer bitmapLayer;
	int step;
} MatrixCell;

static MatrixCell cells[NUM_ROWS][NUM_COLS];
static int dx, dy, cellWidth, cellHeight;
static GFont customFont;
static int32_t rand_seed = 3;
static PblTm now, last;
static HeapBitmap bmp[NUM_BMP];
static int M_ROWS = NUM_ROWS/2;
static int M_COLS = NUM_COLS/2;
static GRect bmpRect;

static inline void set_random_seed(int32_t seed) {
	rand_seed = (seed & 32767);
}

// Returns pseudo-random integer in the range [0,max[
static int random(int max) {
	rand_seed = (((rand_seed * 214013L + 2531011L) >> 16) & 32767);
	return (rand_seed%max);
}

static inline void setCellText(MatrixCell *cell, char t) {
	cell->text[0] = t;
	text_layer_set_text(&cell->textLayer, cell->text);
}

static void setHour() {
	setCellText(&cells[M_ROWS][M_COLS-2], '0' + now.tm_hour/10);
	setCellText(&cells[M_ROWS][M_COLS-1], '0' + now.tm_hour%10);
	setCellText(&cells[M_ROWS][M_COLS+1], '0' + now.tm_min/10);
	setCellText(&cells[M_ROWS][M_COLS+2], '0' + now.tm_min%10);
}

void handle_tick(AppContextRef ctx, PebbleTickEvent *e) {
	int r, c, i, max;
	MatrixCell *cell;
	
	now = *(e->tick_time);
	
	if (now.tm_min != last.tm_min) {
		setHour();
	}
	
	for (r=0; r<NUM_ROWS; r++) {
		for (c=0; c<NUM_COLS; c++) {
			cell = &cells[r][c];
			if (cell->step > 0) {
				cell->step--;
				if (cell->step == 0) {
					bitmap_layer_set_bitmap(&cell->bitmapLayer, NULL);
					setCellText(cell, ' ');
				} else {
					bitmap_layer_set_bitmap(&cell->bitmapLayer, &bmp[cell->step-1].bmp);
				}
			}
		}
	}
	
	max = 1+random(MAX_NUM_NEW_CELLS);
	for (i=0; i<max; i++) {
		r = random(NUM_ROWS);
		c = random(NUM_COLS);
		while (cells[r][c].step < 0) {
			r = random(NUM_ROWS);
			c = random(NUM_COLS);
		}
		cells[r][c].step = NUM_BMP+1;
		setCellText(&cells[r][c], 'a' + random(26));
		bitmap_layer_set_bitmap(&cells[r][c].bitmapLayer, NULL);
	}
	last = now;
}

void initCell(int col, int row) {
	GRect rect = GRect(dx + col * cellWidth, dy + row * cellHeight, cellWidth, cellHeight);
	MatrixCell *cell = &cells[row][col];
	TextLayer *t = &cell->textLayer;
	BitmapLayer *b = &cell->bitmapLayer;
	
	if ((row == M_ROWS) && (col >= M_COLS-2) && (col <= M_COLS+2)) {
		cell->step = -1;
	} else {
		cell->step = 0;
	}
	
	text_layer_init(t, rect);
	text_layer_set_font(t, customFont);
	text_layer_set_background_color(t, GColorBlack);
	text_layer_set_text_color(t, GColorWhite);
	text_layer_set_text_alignment(t, GTextAlignmentCenter);
	layer_add_child(&window.layer, &t->layer);
	setCellText(cell, ' ');
	
	bitmap_layer_init(b, rect);
	bitmap_layer_set_compositing_mode(b, GCompOpAnd);
	layer_add_child(&window.layer, &b->layer);
}

void handle_init(AppContextRef ctx) {
	int x, y;
	
	window_init(&window, "Matrix Code");
	window_stack_push(&window, true /* Animated */);
    window_set_background_color(&window, GColorBlack);
    
    // Init resources
    resource_init_current_app(&APP_RESOURCES);

	customFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MATRIX_23));
	
	cellWidth = SCREENW / NUM_COLS;
	cellHeight = SCREENH / NUM_ROWS;
	dx = (SCREENW - NUM_COLS*cellWidth)/2;
	dy = (SCREENH - NUM_ROWS*cellHeight)/2;
	bmpRect = GRect(0, 0, cellWidth, cellHeight);
	
	get_time(&now);
	set_random_seed(now.tm_yday*86400+now.tm_hour*3600+now.tm_min*60+now.tm_sec);
	
	for (x=0; x<NUM_BMP; x++) {
		heap_bitmap_init(&bmp[x], bmpId[x]);
	}

	for (y=0; y<NUM_ROWS; y++) {
		for (x=0; x<NUM_COLS; x++) {
			initCell(x, y);
		}
	}
	
	last.tm_min = -1;
	setHour();
}

void handle_deinit(AppContextRef ctx) {
	int i;
	fonts_unload_custom_font(customFont);
	for (i=0; i<NUM_BMP; i++) {
		heap_bitmap_deinit(&bmp[i]);
	}

}

void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
		.init_handler = &handle_init,
		.deinit_handler = &handle_deinit,
		
		.tick_info = {
			.tick_handler = &handle_tick,
			.tick_units   = SECOND_UNIT
		}
	};
	app_event_loop(params, &handlers);
}
