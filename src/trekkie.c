#include <pebble.h>

static Window *window;
static TextLayer *text_nice_date_layer;
static TextLayer *text_time_layer;
static TextLayer *text_ampm_layer;
static TextLayer *text_stardate_layer;
static GBitmap *background_image;
static BitmapLayer *background_image_layer;
static TextLayer *work_week_layer;
static TextLayer *battery_percent_layer;
static BatteryChargeState battery_charge_state;
static Layer *battery_status_layer;
static GBitmap *bluetooth_image;
static BitmapLayer *bluetooth_status_layer;
static GFont *LCARS17;
static GFont *LCARS36;
static GFont *LCARS60;

void update_battery_display(BatteryChargeState charge_state) {
  static char battery_percent_text[] = "000";
  snprintf(battery_percent_text, sizeof(battery_percent_text), "%02u", charge_state.charge_percent);
  if (charge_state.charge_percent < 100) {
    battery_percent_text[2] = '%';
  }
  text_layer_set_text(battery_percent_layer, battery_percent_text);
  battery_charge_state = charge_state; // Set for battery percentage bar.
  layer_mark_dirty(battery_status_layer);
}

void update_bluetooth_status(bool connected) {
  layer_set_hidden(bitmap_layer_get_layer(bluetooth_status_layer), !connected);
}

void battery_status_layer_update(Layer* layer, GContext* ctx) {
  if (battery_charge_state.is_charging) {
    graphics_context_set_fill_color(ctx, GColorYellow);
    graphics_fill_rect(ctx, GRect(1, 1, 35, 9), 0, 0);
    graphics_fill_rect(ctx, GRect(36, 3, 2, 5), 0, 0);
  }
  if (battery_charge_state.charge_percent > 40) {
    graphics_context_set_fill_color(ctx, GColorGreen);
  } else if (battery_charge_state.charge_percent > 20) {
    graphics_context_set_fill_color(ctx, GColorOrange);
  } else {
    graphics_context_set_fill_color(ctx, GColorRed);
  }
  graphics_fill_rect(ctx, GRect(2, 2, battery_charge_state.charge_percent / 3, 7), 0, 0);
}

void date_update(struct tm* tick_time, TimeUnits units_changed) {
  static char nice_date_text[] = "Xxx. Xxx. 00";
  static char stardate_text[] = "0000.00.00";
  static char work_week_text[] = "WW.00";

  // Date Layers
  strftime(nice_date_text, sizeof(nice_date_text), "%a.%n%b.%n%d", tick_time);
  strftime(stardate_text, sizeof(stardate_text), "%Y.%m.%d", tick_time); // Ok, not a stardate.
  strftime(work_week_text, sizeof(work_week_text), "WW%n%U", tick_time);
  // No way to tell strftime to use 1-based workweek...
  if (work_week_text[4] == '9') {
    work_week_text[4] = '0';
    ++work_week_text[3];
  } else {
    ++work_week_text[4];
  }
  text_layer_set_text(text_nice_date_layer, nice_date_text);
  text_layer_set_text(text_stardate_layer, stardate_text);
  text_layer_set_text(work_week_layer, work_week_text);
}

void time_update(struct tm* tick_time, TimeUnits units_changed) {
  if(units_changed >= DAY_UNIT) {
    date_update(tick_time, units_changed); 
  }
  static char time_text[6];
  if (clock_is_24h_style()) {
    static const char *time_format = "%H:%M";
    strftime(time_text, sizeof(time_text), time_format, tick_time);
  } else {
    static const char *time_format = "%I:%M";
    strftime(time_text, sizeof(time_text), time_format, tick_time);
    text_layer_set_text(text_ampm_layer, tick_time->tm_hour < 12 ? "am" : "pm");
  }

  // Time layer
  text_layer_set_text(text_time_layer, time_text);
}

static void init(void) {
  window = window_create();
  window_stack_push(window, true /* Animated */);

  // Fonts
  LCARS17=fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCARS_BOLD_17));
  LCARS36=fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCARS_36));
  LCARS60=fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCARS_60));

  // Background Layer
  background_image_layer = bitmap_layer_create(layer_get_frame(window_get_root_layer(window)));
  background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  bitmap_layer_set_bitmap(background_image_layer, background_image);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(background_image_layer));

  // Nice Date Layer
  text_nice_date_layer = text_layer_create(GRect(6, 26, 144 - 6, 168 - 26));
  text_layer_set_background_color(text_nice_date_layer, GColorClear);
  text_layer_set_font(text_nice_date_layer, LCARS17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_nice_date_layer));

  // Time Layer
  text_time_layer = text_layer_create(GRect(42, 17, 144 - 42, 168 - 17));
  text_layer_set_text_color(text_time_layer, GColorYellow);
  text_layer_set_background_color(text_time_layer, GColorClear);
  text_layer_set_font(text_time_layer, LCARS60);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_time_layer));

  // AM/PM Layer
  text_ampm_layer = text_layer_create(GRect(36, 69, 144 - 36, 168 - 69));
  text_layer_set_text_color(text_ampm_layer, GColorYellow);
  text_layer_set_background_color(text_ampm_layer, GColorClear);
  text_layer_set_font(text_ampm_layer, LCARS17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_ampm_layer));

  // Stardate Layer
  text_stardate_layer = text_layer_create(GRect(36, 95, 144 - 36, 168 - 95));
  text_layer_set_text_color(text_stardate_layer, GColorYellow);
  text_layer_set_background_color(text_stardate_layer, GColorClear);
  text_layer_set_font(text_stardate_layer, LCARS36);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_stardate_layer));
  
  // Work week layer.
  work_week_layer = text_layer_create(GRect(8, 93, 27, 115));
  text_layer_set_background_color(work_week_layer, GColorClear);
  text_layer_set_font(work_week_layer, LCARS17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(work_week_layer));
  
  // Battery Percent Layer
  battery_percent_layer = text_layer_create(GRect(65, 4, 27, 115));
  text_layer_set_background_color(battery_percent_layer, GColorClear);
  text_layer_set_font(battery_percent_layer, LCARS17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(battery_percent_layer));
  
  // Battery Status Layer
  battery_status_layer = layer_create(GRect(96, 9, 39, 11));
  layer_add_child(window_get_root_layer(window), battery_status_layer);
  layer_set_update_proc(battery_status_layer, battery_status_layer_update);

  // Bluetooth Status Layer
  bluetooth_status_layer = bitmap_layer_create(GRect(44, 8, 14, 13));
  bluetooth_image = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_IMAGE);
  bitmap_layer_set_bitmap(bluetooth_status_layer, bluetooth_image);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(bluetooth_status_layer));

  // Prevent blank screen on init.
  time_t now = time(NULL);
  struct tm* tick_time;
  tick_time = localtime(&now);
  time_update(tick_time, DAY_UNIT);
  update_battery_display(battery_state_service_peek());
  update_bluetooth_status(bluetooth_connection_service_peek());

  // Subscribe to bluetooth, battery, and time updates.
  bluetooth_connection_service_subscribe(update_bluetooth_status);
  battery_state_service_subscribe(update_battery_display);
  tick_timer_service_subscribe(MINUTE_UNIT, &time_update);
}

static void deinit(void) {
  // Deinit, in reverse order from creation.

  // Destroy layers.
  bitmap_layer_destroy(bluetooth_status_layer);
  layer_destroy(battery_status_layer);
  text_layer_destroy(battery_percent_layer);
  text_layer_destroy(work_week_layer);
  text_layer_destroy(text_stardate_layer);
  text_layer_destroy(text_ampm_layer);
  text_layer_destroy(text_time_layer);
  text_layer_destroy(text_nice_date_layer);
  bitmap_layer_destroy(background_image_layer);

  // Destroy bitmaps.
  gbitmap_destroy(bluetooth_image);
  gbitmap_destroy(background_image);

  //Destroy fonts.
  fonts_unload_custom_font(LCARS17);
  fonts_unload_custom_font(LCARS36);
  fonts_unload_custom_font(LCARS60);
  
  // Destroy window.
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
