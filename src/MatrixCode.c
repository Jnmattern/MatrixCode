#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0x8B, 0x2A, 0x20, 0x41, 0x56, 0xC8, 0x4B, 0x9D, 0xAF, 0x7B, 0xE8, 0x89, 0x48, 0x96, 0xD6, 0x73 }
PBL_APP_INFO(MY_UUID,
             "Matrix Code", "Jnm",
             1, 5, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_WATCH_FACE);

#define STRAIGHT_DIGITS false
#define BLINKING_SEMICOLON false

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

#define MAX_NEW_CELLS 3

Window window;
bool clock12;

typedef struct {
	TextLayer textLayer;
	BitmapLayer bitmapLayer;
	int step;
} MatrixCell;

MatrixCell cells[NUM_ROWS*NUM_COLS];
int dx, dy, cellWidth, cellHeight;
GFont matrixFont, digitFont;
int32_t rand_seed = 3;
PblTm now, last;
HeapBitmap bmp[NUM_BMP];
int M_ROWS = NUM_ROWS/2;
int M_COLS = NUM_COLS/2;
GRect bmpRect;
char *glyphs[] = {
	"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
	"n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"
};
char *uppers[] = {
	"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
	"N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"
};
char *digits[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
char space[] = " ";
char semicolon[] = ":";

static inline void set_random_seed(int32_t seed) {
	rand_seed = (seed & 32767);
}

// Returns pseudo-random integer in the range [0,max[
static int random(int max) {
	rand_seed = (((rand_seed * 214013L + 2531011L) >> 16) & 32767);
	return (rand_seed%max);
}

static inline void setCellEmpty(int r, int c) {
	MatrixCell *cell = &(cells[r*NUM_COLS+c]);
	text_layer_set_text(&(cell->textLayer), space);
}

static inline void setCellSemiColon(int r, int c) {
	MatrixCell *cell = &(cells[r*NUM_COLS+c]);
	text_layer_set_text(&(cell->textLayer), semicolon);
}

static inline void setCellDigit(int r, int c, int n) {
	MatrixCell *cell = &(cells[r*NUM_COLS+c]);
	text_layer_set_text(&(cell->textLayer), digits[n]);
}

static inline void setCellGlyph(int r, int c, int n) {
	MatrixCell *cell = &(cells[r*NUM_COLS+c]);
	text_layer_set_text(&(cell->textLayer), glyphs[n]);
}

static inline void setCellChar(int r, int c, char t) {
	MatrixCell *cell = &(cells[r*NUM_COLS+c]);
	text_layer_set_text(&(cell->textLayer), uppers[t - 'A']);
	cell->step = NUM_BMP+1;
}

static inline void setCellBitmap(int r, int c, GBitmap *bmp) {
	bitmap_layer_set_bitmap(&(cells[r*NUM_COLS+c].bitmapLayer), bmp);
}

static void setHour() {
	int h = now.tm_hour;

	if (clock12) {
		h = h%12;
		if (h == 0) h = 12;
	}
	setCellDigit(M_ROWS, M_COLS-2, h/10);
	setCellDigit(M_ROWS, M_COLS-1, h%10);
	setCellDigit(M_ROWS, M_COLS+1, now.tm_min/10);
	setCellDigit(M_ROWS, M_COLS+2, now.tm_min%10);
}

void handle_tick(AppContextRef ctx, PebbleTickEvent *e) {
	int r, c, i, max;
	MatrixCell *cell;
	
	now = *(e->tick_time);
	
	for (r=0; r<NUM_ROWS; r++) {
		for (c=0; c<NUM_COLS; c++) {
			cell = &(cells[r*NUM_COLS+c]);
			if (cell->step > 0) {
				cell->step = cell->step - 1;
				if (cell->step == 0) {
					setCellBitmap(r, c, NULL);
					setCellEmpty(r, c);
				} else {
					setCellBitmap(r, c, &(bmp[cell->step-1].bmp));
				}
			}
		}
	}
	
	max = 1+random(MAX_NEW_CELLS);
	for (i=0; i<max; i++) {
		r = random(NUM_ROWS);
		c = random(NUM_COLS);
		
		if (cells[r*NUM_COLS+c].step == 0) {
			cells[r*NUM_COLS+c].step = NUM_BMP+1;
			setCellGlyph(r, c, random(26));
			setCellBitmap(r, c, NULL);
		}
	}

	if (now.tm_min != last.tm_min) {
		setHour();
	}
	
#if BLINKING_SEMICOLON
	if (now.tm_sec%2) {
		setCellEmpty(M_ROWS, M_COLS);
	} else {
		setCellSemiColon(M_ROWS, M_COLS);
	}
#endif
	last = now;
}

void initCell(int col, int row) {
	GRect rect = GRect(dx + col * cellWidth, dy + row * cellHeight, cellWidth, cellHeight);
	MatrixCell *cell = &(cells[row*NUM_COLS+col]);
	TextLayer *t = &(cell->textLayer);
	BitmapLayer *b = &(cell->bitmapLayer);
	
	cell->step = 0;
	
	text_layer_init(t, rect);
	text_layer_set_font(t, matrixFont);
	text_layer_set_background_color(t, GColorBlack);
	text_layer_set_text_color(t, GColorWhite);
	text_layer_set_text_alignment(t, GTextAlignmentCenter);
	layer_add_child(&window.layer, &t->layer);
	setCellEmpty(row, col);
	
	bitmap_layer_init(b, rect);
	bitmap_layer_set_compositing_mode(b, GCompOpClear);
	layer_add_child(&window.layer, &b->layer);
}

void handle_init(AppContextRef ctx) {
	int x, y;
	MatrixCell *cell;
	
	window_init(&window, "Matrix Code");
	window_stack_push(&window, true /* Animated */);
    window_set_background_color(&window, GColorBlack);
    
    // Init resources
    resource_init_current_app(&APP_RESOURCES);

	matrixFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MATRIX_23));
	digitFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SOURCECODE_23));
	
	cellWidth = SCREENW / NUM_COLS;
	cellHeight = SCREENH / NUM_ROWS;
	dx = (SCREENW - NUM_COLS*cellWidth)/2;
	dy = (SCREENH - NUM_ROWS*cellHeight)/2;
	bmpRect = GRect(0, 0, cellWidth, cellHeight);
	
	get_time(&now);
	set_random_seed(now.tm_yday*86400+now.tm_hour*3600+now.tm_min*60+now.tm_sec);
	
	clock12 = !clock_is_24h_style();

	for (x=0; x<NUM_BMP; x++) {
		heap_bitmap_init(&bmp[x], bmpId[x]);
	}

	for (y=0; y<NUM_ROWS; y++) {
		for (x=0; x<NUM_COLS; x++) {
			initCell(x, y);
		}
	}
	
	// Reserve cells for displaying hour
	for (x=M_COLS-2; x<=M_COLS+2; x++) {
		cell = &(cells[M_ROWS*NUM_COLS+x]);
		cell->step = -1;
#if STRAIGHT_DIGITS
		text_layer_set_font(&cell->textLayer, digitFont);
#endif
	}
	// Set Font for the semicolon as the MatrixFont doesn't have it
	text_layer_set_font(&(cells[M_ROWS*NUM_COLS+M_COLS].textLayer), digitFont);
	
	// Start with a nice Splash Screen
	setCellChar(0, 0, 'M');
	setCellChar(0, 1, 'A');
	setCellChar(0, 2, 'T');
	setCellChar(0, 3, 'R');
	setCellChar(0, 4, 'I');
	setCellChar(0, 5, 'X');
	
	setCellChar(1, 0, 'C');
	setCellChar(1, 1, 'O');
	setCellChar(1, 2, 'D');
	setCellChar(1, 3, 'E');

	setCellChar(NUM_ROWS-2, NUM_COLS-2, 'B');
	setCellChar(NUM_ROWS-2, NUM_COLS-1, 'Y');
	setCellChar(NUM_ROWS-1, NUM_COLS-3, 'J');
	setCellChar(NUM_ROWS-1, NUM_COLS-2, 'N');
	setCellChar(NUM_ROWS-1, NUM_COLS-1, 'M');

	last.tm_min = -1;
	setHour();
}

void handle_deinit(AppContextRef ctx) {
	int i;
	fonts_unload_custom_font(matrixFont);
	fonts_unload_custom_font(digitFont);
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
