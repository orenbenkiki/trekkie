#include <pebble.h>

static Window *window;
static TextLayer *text_compass_two_letter_layer;
static TextLayer *text_compass_one_letter_layer;
static TextLayer *text_nice_date_layer;
static TextLayer *text_time_layer;
static TextLayer *text_stardate_layer;
static TextLayer *text_long_time_left_layer;
static TextLayer *text_short_time_left_layer;
static TextLayer *text_charge_time_left_layer;
static GBitmap *background_image;
static BitmapLayer *background_image_layer;
static TextLayer *work_week_layer;
static BatteryChargeState battery_charge_state;
static Layer *battery_status_layer;
static GBitmap *bluetooth_image;
static BitmapLayer *bluetooth_status_layer;
static GFont *LCARS17;
static GFont *LUCIDA17;
static GFont *LCARS20;
static GFont *LCARS36;
static GFont *LCARS60;
static GFont *MOON14;

typedef struct {
  double hours_per_percent;
  time_t previous_tick_time;
  int previous_tick_percent;
} Tracker;

static Tracker discharge_tracker;
static Tracker charge_tracker;

bool was_previous_battery_charging;
time_t previous_tick_time;

void switched_tracker(Tracker* tracker) {
  tracker->previous_tick_time = 0; 
}

void update_tracker(Tracker* tracker, int current_tick_percent, time_t current_tick_time) {
  int percents_delta = current_tick_percent - tracker->previous_tick_percent;
  if (!percents_delta) {
    return;
  }
  if (percents_delta < 0) {
    percents_delta = -percents_delta;
  }
  if (tracker->previous_tick_time) {
    time_t time_delta = current_tick_time - tracker->previous_tick_time;
    double hours_delta = time_delta / 3600.0;
    double step_hours_per_percent = hours_delta / percents_delta;
    if (tracker->hours_per_percent) {
      tracker->hours_per_percent = 0.9 * tracker->hours_per_percent + 0.1 * step_hours_per_percent;
    } else {
      tracker->hours_per_percent = step_hours_per_percent;
    }
  }
  tracker->previous_tick_time = current_tick_time;
  tracker->previous_tick_percent = current_tick_percent;
}

void format_tracker(Tracker* tracker, int current_tick_time, int target_percent, char *text) {
  if (tracker->previous_tick_time && tracker->hours_per_percent) {
    int time_since_previous_tick = current_tick_time - tracker->previous_tick_time;
    double hours_since_previous_tick = time_since_previous_tick / 3600.0;
    int difference_percent = target_percent - tracker->previous_tick_percent;
    if (difference_percent < 0) {
      difference_percent = -difference_percent;
    }
    if (difference_percent) {
      double difference_hours = difference_percent * tracker->hours_per_percent - hours_since_previous_tick;
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

void time_left_update() {
  static char time_left_text[] = "0+00";
  if (battery_charge_state.is_charging) {
    update_tracker(&charge_tracker, battery_charge_state.charge_percent, previous_tick_time);
    format_tracker(&charge_tracker, previous_tick_time, 100, time_left_text);
  } else {
    update_tracker(&discharge_tracker, battery_charge_state.charge_percent, previous_tick_time);
    format_tracker(&discharge_tracker, previous_tick_time, 0, time_left_text);
  }
  if (battery_charge_state.is_charging) {
    text_layer_set_text(text_long_time_left_layer, "");
    text_layer_set_text(text_short_time_left_layer, "");
    text_layer_set_text(text_charge_time_left_layer, time_left_text);
  } else if(battery_charge_state.charge_percent < 50) {
    text_layer_set_text(text_long_time_left_layer, "");
    text_layer_set_text(text_short_time_left_layer, time_left_text);
    text_layer_set_text(text_charge_time_left_layer, "");
  } else {
    text_layer_set_text(text_long_time_left_layer, time_left_text);
    text_layer_set_text(text_short_time_left_layer, "");
    text_layer_set_text(text_charge_time_left_layer, "");
  }
}

void compass_heading_handler(CompassHeadingData heading_data) {
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
      text_layer_set_text(text_compass_one_letter_layer, "");
      text_layer_set_text(text_compass_two_letter_layer, current_heading_text);
    } else {
      text_layer_set_text(text_compass_one_letter_layer, current_heading_text);
      text_layer_set_text(text_compass_two_letter_layer, "");
    }
    last_heading_text = current_heading_text;
  }
}

void update_battery_display(BatteryChargeState charge_state) {
  battery_charge_state = charge_state; // Set for battery percentage bar.
  layer_mark_dirty(battery_status_layer);
  if (battery_charge_state.is_charging != was_previous_battery_charging) {
    switched_tracker(&charge_tracker);
    switched_tracker(&discharge_tracker);
    was_previous_battery_charging = battery_charge_state.is_charging;
  }
  time_left_update();
}

void update_bluetooth_status(bool connected) {
  layer_set_hidden(bitmap_layer_get_layer(bluetooth_status_layer), !connected);
}

void battery_status_layer_update(Layer* layer, GContext* ctx) {
  if (battery_charge_state.is_charging) {
    graphics_context_set_fill_color(ctx, GColorYellow);
    graphics_fill_rect(ctx, GRect(0, 0, 99, 14), 0, 0);
    graphics_fill_rect(ctx, GRect(99, 3, 2, 8), 0, 0);
  }
  if (battery_charge_state.charge_percent > 40) {
    graphics_context_set_fill_color(ctx, GColorGreen);
  } else if (battery_charge_state.charge_percent > 20) {
    graphics_context_set_fill_color(ctx, GColorOrange);
  } else {
    graphics_context_set_fill_color(ctx, GColorRed);
  }
  graphics_fill_rect(ctx, GRect(2, 2, (battery_charge_state.charge_percent * 95) / 100, 10), 0, 0);
}

void date_update(struct tm* tick_time, TimeUnits units_changed) {
  static char nice_date_text[] = "Xxx.Xxx";
  static char stardate_text[] = "0000.00.00";
  static char work_week_text[] = "00";

  // Date Layers
  strftime(nice_date_text, sizeof(nice_date_text), "%a%n%b", tick_time);
  for (char *c = nice_date_text; *c; ++c) {
    if ('a' <= *c && *c <= 'z') {
      *c -= 'a';
      *c += 'A';
    }
  }
  strftime(stardate_text, sizeof(stardate_text), "%Y.%m.%d", tick_time); // Ok, not a stardate.
  strftime(work_week_text, sizeof(work_week_text), "%U", tick_time);
  // No way to tell strftime to use 1-based workweek...
  if (work_week_text[1] == '9') {
    work_week_text[1] = '0';
    ++work_week_text[0];
  } else {
    ++work_week_text[1];
  }
  // Abuse the 12/24 setting to force an additional 1w offset.
  if (clock_is_24h_style()) {
    if (work_week_text[1] == '9') {
      work_week_text[1] = '0';
      ++work_week_text[0];
    } else {
      ++work_week_text[1];
    }
  }
  text_layer_set_text(text_nice_date_layer, nice_date_text);
  text_layer_set_text(text_stardate_layer, stardate_text);
  text_layer_set_text(work_week_layer, work_week_text);
}

void time_update(struct tm* tick_time, TimeUnits units_changed) {
  if(units_changed >= DAY_UNIT) {
    date_update(tick_time, units_changed); 
  }
  
  // Time layer
  static char time_text[6];
  static const char *time_format = "%H:%M";
  strftime(time_text, sizeof(time_text), time_format, tick_time);
  text_layer_set_text(text_time_layer, time_text);
  
  previous_tick_time = mktime(tick_time);
  time_left_update();
}

static void init(void) {
  window = window_create();
  window_stack_push(window, true /* Animated */);

  // Fonts
  LUCIDA17 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LUCIDA_17));
  LCARS17 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCARS_BOLD_17));
  LCARS20 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCARS_BOLD_20));
  LCARS36 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCARS_36));
  LCARS60 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCARS_60));
  MOON14 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MOONHOUSE_14));

  // Background Layer
  background_image_layer = bitmap_layer_create(layer_get_frame(window_get_root_layer(window)));
  background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  bitmap_layer_set_bitmap(background_image_layer, background_image);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(background_image_layer));

  // Compass heading layer
  text_compass_two_letter_layer = text_layer_create(GRect(7, 95, 27, 168 - 95));
  text_layer_set_background_color(text_compass_two_letter_layer, GColorClear);
  text_layer_set_font(text_compass_two_letter_layer, LUCIDA17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_compass_two_letter_layer));
  
  text_compass_one_letter_layer = text_layer_create(GRect(12, 95, 27, 168 - 95));
  text_layer_set_background_color(text_compass_one_letter_layer, GColorClear);
  text_layer_set_font(text_compass_one_letter_layer, LUCIDA17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_compass_one_letter_layer));
  
  // Nice Date Layer
  text_nice_date_layer = text_layer_create(GRect(6, 33, 144 - 6, 168 - 33));
  text_layer_set_background_color(text_nice_date_layer, GColorClear);
  text_layer_set_font(text_nice_date_layer, LUCIDA17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_nice_date_layer));

  // Time Layer
  text_time_layer = text_layer_create(GRect(45, 5, 144 - 45, 168 - 5));
  text_layer_set_text_color(text_time_layer, GColorWhite);
  text_layer_set_background_color(text_time_layer, GColorClear);
  text_layer_set_font(text_time_layer, LCARS60);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_time_layer));

  // Stardate Layer
  text_stardate_layer = text_layer_create(GRect(34, 94, 144 - 36, 168 - 94));
  text_layer_set_text_color(text_stardate_layer, GColorWhite);
  text_layer_set_background_color(text_stardate_layer, GColorClear);
  text_layer_set_font(text_stardate_layer, LCARS36);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_stardate_layer));
  
  // Work week layer.
  work_week_layer = text_layer_create(GRect(7, 112, 27, 168 - 112));
  text_layer_set_background_color(work_week_layer, GColorClear);
  text_layer_set_font(work_week_layer, LUCIDA17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(work_week_layer));
  
  // Battery Status Layer
  battery_status_layer = layer_create(GRect(35, 148, 101, 14));
  layer_add_child(window_get_root_layer(window), battery_status_layer);
  layer_set_update_proc(battery_status_layer, battery_status_layer_update);

  // Bluetooth Status Layer
  bluetooth_status_layer = bitmap_layer_create(GRect(12, 6, 20, 26));
  bluetooth_image = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_IMAGE);
  bitmap_layer_set_bitmap(bluetooth_status_layer, bluetooth_image);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(bluetooth_status_layer));
  
  // Long time left Layer
  text_long_time_left_layer = text_layer_create(GRect(38, 145, 144 - 38, 168 - 145));
  text_layer_set_text_color(text_long_time_left_layer, GColorBlack);
  text_layer_set_background_color(text_long_time_left_layer, GColorClear);
  text_layer_set_font(text_long_time_left_layer, MOON14);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_long_time_left_layer));
  
  // Short time left Layer
  text_short_time_left_layer = text_layer_create(GRect(87, 145, 144 - 87, 168 - 145));
  text_layer_set_text_color(text_short_time_left_layer, GColorWhite);
  text_layer_set_background_color(text_short_time_left_layer, GColorClear);
  text_layer_set_font(text_short_time_left_layer, MOON14);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_short_time_left_layer));

  // Charge time left Layer
  text_charge_time_left_layer = text_layer_create(GRect(87, 145, 144 - 87, 168 - 145));
  text_layer_set_text_color(text_charge_time_left_layer, GColorBlack);
  text_layer_set_background_color(text_charge_time_left_layer, GColorClear);
  text_layer_set_font(text_charge_time_left_layer, MOON14);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_charge_time_left_layer));
  
  // Prevent blank screen on init.
  time_t now = time(NULL);
  struct tm* tick_time;
  tick_time = localtime(&now);
  time_update(tick_time, DAY_UNIT);
  update_battery_display(battery_state_service_peek());
  update_bluetooth_status(bluetooth_connection_service_peek());

  // Subscribe to bluetooth, battery, time and compass updates.
  bluetooth_connection_service_subscribe(update_bluetooth_status);
  battery_state_service_subscribe(update_battery_display);
  tick_timer_service_subscribe(MINUTE_UNIT, &time_update);
  compass_service_set_heading_filter(10 * (TRIG_MAX_ANGLE / 360));
  compass_service_subscribe(&compass_heading_handler);
}

static void deinit(void) {
  // Deinit, in reverse order from creation.
  compass_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();

  // Destroy layers.
  bitmap_layer_destroy(bluetooth_status_layer);
  layer_destroy(battery_status_layer);
  text_layer_destroy(work_week_layer);
  text_layer_destroy(text_stardate_layer);
  text_layer_destroy(text_time_layer);
  text_layer_destroy(text_long_time_left_layer);
  text_layer_destroy(text_short_time_left_layer);
  text_layer_destroy(text_charge_time_left_layer);
  text_layer_destroy(text_nice_date_layer);
  text_layer_destroy(text_compass_one_letter_layer);
  text_layer_destroy(text_compass_two_letter_layer);
  bitmap_layer_destroy(background_image_layer);

  // Destroy bitmaps.
  gbitmap_destroy(bluetooth_image);
  gbitmap_destroy(background_image);

  // Destroy fonts.
  fonts_unload_custom_font(LCARS17);
  fonts_unload_custom_font(LUCIDA17);
  fonts_unload_custom_font(LCARS20);
  fonts_unload_custom_font(LCARS36);
  fonts_unload_custom_font(LCARS60);
  fonts_unload_custom_font(MOON14);
  
  // Destroy window.
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
