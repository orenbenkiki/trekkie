#include <pebble.h>
  
// Not a good practice, but it makes it so much easier to
// initialize the data arrays below.
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  
//// Overall window.
  
// The overall application window.
static Window *window;

static void init_window() {
  window = window_create();
  window_stack_push(window, true /* Animated */);
}

static void deinit_window() {
  window_destroy(window);
}

//// Images.

// Used image data.
typedef struct {
  // The automatically generated font resource index.
  int resource_id;
  
  // The rectangle to render the image into.
  GRect g_rect;
  
  // The actual image bitmap, obtained at init.
  GBitmap *g_bitmap;
  
  // The layer to render the image into, obtained at init.
  BitmapLayer *bitmap_layer;
} Image;

// The indices of the images we use.
typedef enum {
  BACKGROUND_IMAGE, // Used for the overall background.
  BLUETOOTH_IMAGE, // Used to indicate active bluetooth connection.
  IMAGES_COUNT
} WhichImage;

// The data of the images we use.
static Image images[IMAGES_COUNT] = {
  { RESOURCE_ID_IMAGE_BACKGROUND /* Whole window */ },
  { RESOURCE_ID_BLUETOOTH_IMAGE, { .origin = { .x = 12, .y = 6 }, { .w = 20, .h = 26 } } }
};

static void init_images() {
  for (WhichImage which_image = 0; which_image < IMAGES_COUNT; ++which_image) {
    if (which_image == BACKGROUND_IMAGE) {
      images[which_image].g_rect = layer_get_frame(window_get_root_layer(window));
    }
    images[which_image].g_bitmap = gbitmap_create_with_resource(images[which_image].resource_id);
    images[which_image].bitmap_layer = bitmap_layer_create(images[which_image].g_rect);
    bitmap_layer_set_bitmap(images[which_image].bitmap_layer, images[which_image].g_bitmap);
    layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(images[which_image].bitmap_layer));
  }
}

static void set_image_visibility(WhichImage which_image, bool is_visible) {
  layer_set_hidden(bitmap_layer_get_layer(images[which_image].bitmap_layer), !is_visible);
}

static void deinit_images() {
  for (WhichImage which_image = 0; which_image < IMAGES_COUNT; ++which_image) {
    bitmap_layer_destroy(images[which_image].bitmap_layer);
    gbitmap_destroy(images[which_image].g_bitmap);
  }
}

//// Image updates.

static void update_bluetooth_status(bool is_connected) {
  set_image_visibility(BLUETOOTH_IMAGE, is_connected);
}

//// Battery graphics.

// There's only one graphic layer so don't bother with indices etc.

// The current battery charge state.
static BatteryChargeState battery_charge_state;

// The graphic layer to render the battery state into.
static Layer *battery_graphics_layer;

// The top-left location of the battery charge indicator.
#define BATTERY_CHARGE_LEFT 35
#define BATTERY_CHARGE_TOP 148

// The width and height of the rectangle representing the battery charge, including the border.
#define BATTERY_CHARGE_OUTER_WIDTH 99
#define BATTERY_CHARGE_OUTER_HEIGHT 14
  
// The width of the border around the charge indicator.
#define BATTERY_CHARGE_BORDER 2

// The width and height of the extra "bump" at the right end.
#define BATTERY_EXTRA_WIDTH 2
#define BATTERY_EXTRA_HEIGHT 8
  
static void update_battery_graphics(Layer* layer, GContext* ctx) {
  if (battery_charge_state.is_charging) {
    graphics_context_set_fill_color(ctx, GColorYellow);
    graphics_fill_rect(ctx, GRect(0, 0,
                                  BATTERY_CHARGE_OUTER_WIDTH, BATTERY_CHARGE_OUTER_HEIGHT),
                       0, 0);
    int extra_height_top = (BATTERY_CHARGE_OUTER_HEIGHT - BATTERY_EXTRA_HEIGHT) / 2;
    graphics_fill_rect(ctx, GRect(BATTERY_CHARGE_OUTER_WIDTH, extra_height_top,
                                  BATTERY_EXTRA_WIDTH, BATTERY_EXTRA_HEIGHT),
                       0, 0);
  }
  if (battery_charge_state.charge_percent > 40) {
    graphics_context_set_fill_color(ctx, GColorGreen);
  } else if (battery_charge_state.charge_percent > 20) {
    graphics_context_set_fill_color(ctx, GColorOrange);
  } else {
    graphics_context_set_fill_color(ctx, GColorRed);
  }
  int charge_inner_height = BATTERY_CHARGE_OUTER_HEIGHT - 2 * BATTERY_CHARGE_BORDER;
  int charge_inner_width = BATTERY_CHARGE_OUTER_WIDTH - 2 * BATTERY_CHARGE_BORDER;
  charge_inner_width *= battery_charge_state.charge_percent;
  charge_inner_width /= 100;
  graphics_fill_rect(ctx, GRect(BATTERY_CHARGE_BORDER, BATTERY_CHARGE_BORDER,
                                charge_inner_width, charge_inner_height),
                     0, 0);
}

static void init_battery_graphics() {
  battery_graphics_layer = layer_create(GRect(BATTERY_CHARGE_LEFT, BATTERY_CHARGE_TOP,
                                              BATTERY_CHARGE_OUTER_WIDTH + BATTERY_EXTRA_WIDTH,
                                              BATTERY_CHARGE_OUTER_HEIGHT));
  layer_add_child(window_get_root_layer(window), battery_graphics_layer);
  layer_set_update_proc(battery_graphics_layer, update_battery_graphics);
}

static void deinit_battery_graphics() {
  layer_destroy(battery_graphics_layer);
}

//// Update battery graphics.

static void update_time_left(time_t tick_time);

static void battery_update(BatteryChargeState charge_state) {
  battery_charge_state = charge_state; // Set for battery percentage bar.
  layer_mark_dirty(battery_graphics_layer);
  update_time_left(time(NULL));
}

/// Fonts.

// Used font data.
typedef struct {
  // The automatically generated font resource index.
  int resource_id;
  
  // The actual usable font object, obtained at init.
  GFont *g_font;
} Font;

// The indices of the fonts we use.
typedef enum {
  TIME_FONT, // Used for the time (hours and minutes).
  DATE_FONT, // Used for the date (stardate-ish YYYY.MM.DD).
  TEXT_FONT, // Used for all the texts on the left panels.
  TIME_LEFT_FONT, // Used for the dis/charge time left prediction.
  FONTS_COUNT
} WhichFont;

// The data of the fonts we use.
static Font fonts[FONTS_COUNT] = {
  { RESOURCE_ID_FONT_LCARS_60 }, // TIME_FONT
  { RESOURCE_ID_FONT_LCARS_36 }, // DATE_FONT
  { RESOURCE_ID_FONT_LUCIDA_17 }, // TEXT_FONT
  { RESOURCE_ID_FONT_MOONHOUSE_14 } // TIME_LEFT_FONT
};

static void init_fonts() {
  for (WhichFont which_font = 0; which_font < FONTS_COUNT; ++which_font) {
    fonts[which_font].g_font = fonts_load_custom_font(resource_get_handle(fonts[which_font].resource_id));
  }
}

static GFont *font(WhichFont which_font) {
  return fonts[which_font].g_font;
}

static void deinit_fonts() {
  for (WhichFont which_font = 0; which_font < FONTS_COUNT; ++which_font) {
    fonts_unload_custom_font(fonts[which_font].g_font);
  }
}

//// Texts.

// Dynamic text data.
typedef struct {
  // The index of the font to use.
  WhichFont which_font;
  
  // The color of the text.
  // The background is always clear.
  uint8_t text_color;
  
  // The top-left position of the text rectangle.
  // We allow the text to span all the way to the end of the frame.
  GPoint origin;
  
  // The layer to render the text into, obtained at init.
  TextLayer *text_layer;
} Text;

// The indices of the text we use.
typedef enum {
  TIME_TEXT, // The current time (HH:MM).
  DATE_TEXT, // The current date (stardate-ish YYYY.MM.DD).
  DATE_NAMES_TEXT, // The current date in text (3-letter month, newline, 3-letter week day).
  COMPASS_ONE_LETTER_TEXT, // The compass heading if it is one letter (N, S, E, W).
  COMPASS_TWO_LETTER_TEXT, // The compass heading if it is two letter (NE, NW, SE, SW).
  WORK_WEEK_TEXT, // The work week number in the year.
  LONG_TIME_LEFT_TEXT, // Remaining discharge time if battery is >=50, always green background.
  SHORT_TIME_LEFT_TEXT, // Remaining discharge time if battery is <50, always black background.
  CHARGE_TIME_LEFT_TEXT, // Remaining charge time, always light (yellow or green) background.
  TEXTS_COUNT
} WhichText;

// The data of the texts we use.
static Text texts[TEXTS_COUNT] = {
  { TIME_FONT, GColorWhiteARGB8, { .x = 45, .y = 5 } }, // TIME_TEXT
  { DATE_FONT, GColorWhiteARGB8, { .x = 34, .y = 94 } }, // DATE_TEXT
  { TEXT_FONT, GColorBlackARGB8, { .x = 6, .y = 33 } }, // DATE_NAMES_TEXT
  { TEXT_FONT, GColorBlackARGB8, { .x = 12, .y = 95 } }, // COMPASS_ONE_LETTER_TEXT
  { TEXT_FONT, GColorBlackARGB8, { .x = 7, .y = 95 } }, // COMPASS_TWO_LETTER_TEXT
  { TEXT_FONT, GColorBlackARGB8, { .x = 7, .y = 112 } }, // WORK_WEEK_TEXT
  { TIME_LEFT_FONT, GColorBlackARGB8, { .x = 38, .y = 145 } }, // LONG_TIME_LEFT_TEXT
  { TIME_LEFT_FONT, GColorWhiteARGB8, { .x = 87, .y = 145 } }, // SHORT_TIME_LEFT_TEXT
  { TIME_LEFT_FONT, GColorBlackARGB8, { .x = 87, .y = 145 } }, // CHARGE_TIME_LEFT_TEXT
};

static void init_texts() {
  const GRect frame = layer_get_frame(window_get_root_layer(window));
  for (WhichText which_text = 0; which_text < TEXTS_COUNT; ++which_text) {
    GRect rect = frame;
    rect.origin = texts[which_text].origin;
    grect_clip(&rect, &frame);
    texts[which_text].text_layer = text_layer_create(rect);
    text_layer_set_text_color(texts[which_text].text_layer, (GColor){.argb = texts[which_text].text_color});
    text_layer_set_background_color(texts[which_text].text_layer, GColorClear);
    text_layer_set_font(texts[which_text].text_layer, font(texts[which_text].which_font));
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(texts[which_text].text_layer));
  }
}

static void set_text(WhichText which_text, const char* data) {
  text_layer_set_text(texts[which_text].text_layer, data);
}

static void deinit_texts() {
  for (WhichText which_text = 0; which_text < TEXTS_COUNT; ++which_text) {
    text_layer_destroy(texts[which_text].text_layer);
  }
}

//// Predict time left.

// Alas, we need to maintain global state to update the predictors.
static bool was_previous_battery_charging; // Was the battery charging last time we got an update?

// The keys we need for the global state.
typedef enum {
  PERSIST_WAS_PREVIOUS_BATTERY_CHARGING, // A boolean.
  PERSIST_GLOBAL_FIELDS_COUNT,
} PredictorGlobalField;

// The state of a time left predictor.
typedef struct {
  // How many hours does it take to modify the state by one percent.
  double hours_per_percent;
  
  // The last time we saw an interesting measurement, or 0.
  time_t previous_tick_time;
  
  // The value of the last interesting measurement.
  int previous_tick_percent;
} Predictor;

// The indices of the predictors we use.
// The keys we need to persist the fields of the predictor.
typedef enum {
  PERSIST_HOURS_TO_PERCENT, // A double.
  PERSIST_PREVIOUS_TICK_TIME, // A 64-bit integer.
  PERSIST_PREVIOUS_TICK_PERCENT, // An 8-bit integer.
  PERSIST_INSTANCE_FIELDS_COUNT,
} PredictorInstanceField;

typedef enum {
  CHARGE_PREDICTOR, // Predict how long it takes to charge the battery to full.
  DISCHARGE_PREDICTOR, // Predict how long before the battery runs out.
  PREDICTORS_COUNT
} WhichPredictor;

// The used predictors data.
static Predictor predictors[PREDICTORS_COUNT];

static int predictor_instance_field_key(WhichPredictor which_predictor, PredictorInstanceField predictor_instance_field) {
  return PERSIST_GLOBAL_FIELDS_COUNT + which_predictor * PERSIST_INSTANCE_FIELDS_COUNT + predictor_instance_field;
}

static void init_predictors() {
  if (persist_exists(PERSIST_WAS_PREVIOUS_BATTERY_CHARGING)) {
    was_previous_battery_charging = persist_read_bool(PERSIST_WAS_PREVIOUS_BATTERY_CHARGING);
  }
  time_t now = time(NULL);
  for (WhichPredictor which_predictor = 0; which_predictor < PREDICTORS_COUNT; ++which_predictor) {
    int field_key = predictor_instance_field_key(which_predictor, PERSIST_HOURS_TO_PERCENT);
    if (persist_exists(field_key)) {
      persist_read_data(field_key, &predictors[which_predictor].hours_per_percent,
                        sizeof(predictors[which_predictor].hours_per_percent));
    }
    field_key = predictor_instance_field_key(which_predictor, PERSIST_PREVIOUS_TICK_TIME);
    if (persist_exists(field_key)) {
      persist_read_data(field_key, &predictors[which_predictor].previous_tick_time,
                        sizeof(predictors[which_predictor].previous_tick_time));
      // One hour won't throw the algorithm off too much.
      if (now - predictors[which_predictor].previous_tick_time > 3600) {
        predictors[which_predictor].previous_tick_time = 0;
      }
    }
    if (predictors[which_predictor].previous_tick_time) {
      field_key = predictor_instance_field_key(which_predictor, PERSIST_PREVIOUS_TICK_PERCENT);
      if (persist_exists(field_key)) {
        predictors[which_predictor].previous_tick_percent = persist_read_int(field_key);
      }
    }
  }
}

static void deinit_predictors() {
  persist_write_bool(PERSIST_WAS_PREVIOUS_BATTERY_CHARGING, was_previous_battery_charging);
  for (WhichPredictor which_predictor = 0; which_predictor < PREDICTORS_COUNT; ++which_predictor) {
    int field_key = predictor_instance_field_key(which_predictor, PERSIST_HOURS_TO_PERCENT);
    persist_write_data(field_key, &predictors[which_predictor].hours_per_percent,
                       sizeof(predictors[which_predictor].hours_per_percent));
    field_key = predictor_instance_field_key(which_predictor, PERSIST_PREVIOUS_TICK_TIME);
    persist_write_data(field_key, &predictors[which_predictor].previous_tick_time,
                       sizeof(predictors[which_predictor].previous_tick_time));
    field_key = predictor_instance_field_key(which_predictor, PERSIST_PREVIOUS_TICK_PERCENT);
    persist_write_int(field_key, predictors[which_predictor].previous_tick_percent);
  }
}

static WhichPredictor current_predictor(time_t tick_time) {
  if (battery_charge_state.is_charging != was_previous_battery_charging) {
    if (was_previous_battery_charging && battery_charge_state.charge_percent == 100) {
      // Assume that if we were charging and at 100%, then we still are at 100%.
      for (WhichPredictor which_predictor = 0; which_predictor < PREDICTORS_COUNT; ++which_predictor) {
        predictors[which_predictor].previous_tick_time = tick_time; 
        predictors[which_predictor].previous_tick_percent = 100; 
      }
    } else {
      // Otherwise, we need to reset the time, since we don't know the sufficiently-accurate charge percent.
      for (WhichPredictor which_predictor = 0; which_predictor < PREDICTORS_COUNT; ++which_predictor) {
        predictors[which_predictor].previous_tick_time = 0; 
      }
    }
    was_previous_battery_charging = battery_charge_state.is_charging;
  }
  return battery_charge_state.is_charging ? CHARGE_PREDICTOR : DISCHARGE_PREDICTOR;
}

static void update_predictor(WhichPredictor which_predictor, int current_tick_percent, time_t current_tick_time) {
  Predictor *predictor = &predictors[which_predictor];
  int percents_delta = current_tick_percent - predictor->previous_tick_percent;
  if (!percents_delta) {
    return;
  }
  if (percents_delta < 0) {
    percents_delta = -percents_delta;
  }
  if (predictor->previous_tick_time) {
    time_t time_delta = current_tick_time - predictor->previous_tick_time;
    double hours_delta = time_delta / 3600.0;
    double step_hours_per_percent = hours_delta / percents_delta;
    if (predictor->hours_per_percent) {
      predictor->hours_per_percent = 0.9 * predictor->hours_per_percent + 0.1 * step_hours_per_percent;
    } else {
      predictor->hours_per_percent = step_hours_per_percent;
    }
  }
  predictor->previous_tick_time = current_tick_time;
  predictor->previous_tick_percent = current_tick_percent;
}

static void format_predictor(WhichPredictor which_predictor, int current_tick_time, char *text) {
  const Predictor *predictor = &predictors[which_predictor];
  if (predictor->previous_tick_time && predictor->hours_per_percent) {
    int time_since_previous_tick = current_tick_time - predictor->previous_tick_time;
    double hours_since_previous_tick = time_since_previous_tick / 3600.0;
    int target_percent = which_predictor == DISCHARGE_PREDICTOR ? 0 : 100;
    int difference_percent = target_percent - predictor->previous_tick_percent;
    if (difference_percent < 0) {
      difference_percent = -difference_percent;
    }
    if (difference_percent) {
      double difference_hours = difference_percent * predictor->hours_per_percent - hours_since_previous_tick;
      if (difference_hours >= 1) {
        int difference_days = (int)(difference_hours / 24.0);
        int rounded_difference_hours = (int)(difference_hours - difference_days * 24 + 0.5);
        snprintf(text, 5, "%1d+%02d", difference_days, rounded_difference_hours);
        return;
      }
    }
  }
  *text = '\0';
}

//// Text updates.

static void update_time_left(time_t tick_time) {
  static char time_left_text[] = "0+00";
  WhichPredictor which_predictor = current_predictor(tick_time);
  update_predictor(which_predictor, battery_charge_state.charge_percent, tick_time);
  format_predictor(which_predictor, tick_time, time_left_text);
  if (battery_charge_state.is_charging) {
    set_text(LONG_TIME_LEFT_TEXT, "");
    set_text(SHORT_TIME_LEFT_TEXT, "");
    set_text(CHARGE_TIME_LEFT_TEXT, time_left_text);
  } else if(battery_charge_state.charge_percent < 50) {
    set_text(LONG_TIME_LEFT_TEXT, "");
    set_text(SHORT_TIME_LEFT_TEXT, time_left_text);
    set_text(CHARGE_TIME_LEFT_TEXT, "");
  } else {
    set_text(LONG_TIME_LEFT_TEXT, time_left_text);
    set_text(SHORT_TIME_LEFT_TEXT, "");
    set_text(CHARGE_TIME_LEFT_TEXT, "");
  }
}

static void upcase(char *text) {
  for (char *c = text; *c; ++c) {
    if ('a' <= *c && *c <= 'z') {
      *c -= 'a';
      *c += 'A';
    }
  }
}

static void increment_two_digits(char *digits) {
  if (digits[1] == '9') {
    digits[1] = '0';
    ++digits[0];
  } else {
    ++digits[1];
  }
}

static void update_date(struct tm* tick_time) {
  static char date_text[] = "0000.00.00";
  strftime(date_text, sizeof(date_text), "%Y.%m.%d", tick_time);
  set_text(DATE_TEXT, date_text);
  
  static char date_names_text[] = "Xxx.Xxx";
  strftime(date_names_text, sizeof(date_names_text), "%a%n%b", tick_time);
  upcase(date_names_text);
  set_text(DATE_NAMES_TEXT, date_names_text);
  
  static char work_week_text[] = "00";
  strftime(work_week_text, sizeof(work_week_text), "%U", tick_time);
  // No way to tell strftime to use 1-based workweek...
  increment_two_digits(work_week_text);
  // Abuse the 12/24 setting to force an additional 1w offset.
  if (clock_is_24h_style()) {
    increment_two_digits(work_week_text);
  }
  set_text(WORK_WEEK_TEXT, work_week_text);
}

static void update_time(struct tm* tick_time, TimeUnits units_changed) {
  if(units_changed >= DAY_UNIT) {
    update_date(tick_time); 
  }
  
  static char time_text[6];
  static const char *time_format = "%H:%M";
  strftime(time_text, sizeof(time_text), time_format, tick_time);
  set_text(TIME_TEXT, time_text);
  
  update_time_left(mktime(tick_time));
}

static void update_compass(CompassHeadingData heading_data) {
  static char *direction_text[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW", "N" };
  static char *last_heading_text = NULL;
  char *current_heading_text = NULL;
  bool is_two_letter = false;
  switch (heading_data.compass_status) {
    case CompassStatusDataInvalid:
      current_heading_text = "!";
      break;
    case CompassStatusCalibrating:
      current_heading_text = "?";
      break;
    case CompassStatusCalibrated:
      {
        int angle = 360 - TRIGANGLE_TO_DEG(heading_data.true_heading);
        int direction = (angle * 8 + 180) / 360;
        is_two_letter = direction % 2;
        current_heading_text = direction_text[direction];
      }
      break;
  }
  if (current_heading_text != last_heading_text) {
    if (is_two_letter) {
      set_text(COMPASS_ONE_LETTER_TEXT, "");
      set_text(COMPASS_TWO_LETTER_TEXT, current_heading_text);
    } else {
      set_text(COMPASS_ONE_LETTER_TEXT, current_heading_text);
      set_text(COMPASS_TWO_LETTER_TEXT, "");
    }
    last_heading_text = current_heading_text;
  }
}

//// Subscriptions.

static void init_subscriptions() {
  tick_timer_service_subscribe(MINUTE_UNIT, &update_time);
  bluetooth_connection_service_subscribe(update_bluetooth_status);
  compass_service_set_heading_filter(10 * (TRIG_MAX_ANGLE / 360));
  compass_service_subscribe(&update_compass);
  battery_state_service_subscribe(battery_update);
}

// Prevent blank data until the 1st event arrives, which could be long.
// Ideally, subscribe should have immediately invoked the callback...
static void trigger_updates_before_subscriptions() {
  time_t now = time(NULL);
  struct tm* tick_time;
  tick_time = localtime(&now);
  update_time(tick_time, DAY_UNIT);
  battery_update(battery_state_service_peek());
  update_bluetooth_status(bluetooth_connection_service_peek());
}

static void deinit_subscriptions() {
  battery_state_service_unsubscribe();
  compass_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  tick_timer_service_unsubscribe();
}

/// Main.

static void init(void) {
  init_window();
  init_images();
  init_fonts();
  init_battery_graphics();
  init_texts();
  init_predictors();
  trigger_updates_before_subscriptions();
  init_subscriptions();
}

static void deinit(void) {
  deinit_subscriptions();
  deinit_predictors();
  deinit_texts();
  deinit_battery_graphics();
  deinit_fonts();
  deinit_images();
  deinit_window();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}