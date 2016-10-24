#include <pebble.h>
#include <pebble-events/pebble-events.h>
#include "enamel.h"
#include "cut_up.h"

extern Settings settings;
static char current_text[2][3];
extern char update_text[2][3];

static Window* the_window;
static bool show_leading_zeroes_top;
static bool show_leading_zeroes_bottom;
static bool exiting = false;

static void load_settings() {
  settings.startup_animation = false; // this is not the animation setting but a way to control the dífferent startup scenario for Cut/up+
  settings.color_background_top = enamel_get_color_background_top();
  settings.color_background_bottom = enamel_get_color_background_bottom();
  settings.color_text_top = enamel_get_color_text_top();
  settings.color_text_bottom = enamel_get_color_text_bottom();
  settings.color_text_outline_top = enamel_get_color_text_outline_top();
  settings.color_text_outline_bottom = enamel_get_color_text_outline_bottom();
  settings.color_the_cut = connection_service_peek_pebble_app_connection() ? enamel_get_color_the_cut() : enamel_get_color_the_cut_disconnected();
  settings.color_the_cut_outline_top = enamel_get_color_the_cut_outline_top();
  settings.color_the_cut_outline_bottom = enamel_get_color_the_cut_outline_bottom();
  settings.animations = enamel_get_animations();
#ifndef PBL_PLATFORM_APLITE // there is no text positioning on Aplite to save RAM
  settings.offset_y_text_top = enamel_get_offset_y_text_top();
  settings.offset_y_text_bottom = enamel_get_offset_y_text_bottom();
#else
  settings.offset_y_text_top = 0;
  settings.offset_y_text_bottom = 0;
#endif

  uint8_t show_leading_zeroes = enamel_get_show_leading_zeroes();
  show_leading_zeroes_top = SHOW_LEADING_ZEROES_SHOW == show_leading_zeroes || SHOW_LEADING_ZEROES_HIDE_ON_BOTTOM == show_leading_zeroes;
  show_leading_zeroes_bottom = SHOW_LEADING_ZEROES_SHOW == show_leading_zeroes || SHOW_LEADING_ZEROES_HIDE_ON_TOP == show_leading_zeroes;
}

static void handle_leading_zeroes() {
  if(show_leading_zeroes_top) {
    if(' ' == update_text[CUT_UP_TOP][0]) {
      update_text[CUT_UP_TOP][0] = '0';
    }
  } else {
    if('0' == update_text[CUT_UP_TOP][0]) {
      update_text[CUT_UP_TOP][0] = ' ';
    }
  }
  if(show_leading_zeroes_bottom) {
    if(' ' == update_text[CUT_UP_BOTTOM][0]) {
      update_text[CUT_UP_BOTTOM][0] = '0';
    }
  } else {
    if('0' == update_text[CUT_UP_BOTTOM][0]) {
      update_text[CUT_UP_BOTTOM][0] = ' ';
    }
  }
}

static void set_value(char* text, uint8_t value, struct tm* tick_time) {
  switch(value) {
    case VALUE_TOP_DAY_OF_THE_MONTH: {
      strftime(text, 3, "%d", tick_time);
      break;
    }
    case VALUE_TOP_MONTH: {
      strftime(text, 3, "%m", tick_time);
      break;
    }
    case VALUE_TOP_YEAR: {
      strftime(text, 3, "%g", tick_time);
      break;
    }
    case VALUE_TOP_BATTERY_LEVEL: {
      BatteryChargeState battery_state =  battery_state_service_peek();
      uint8_t battery_level = battery_state.charge_percent;
      if(battery_level > 99) {
        battery_level = 99;
      }
      snprintf(text, 3, "%02d", battery_level);
      break;
    }
    case VALUE_TOP_CALENDAR_WEEK__ISO_8601_: {
      strftime(text, 3, "%V", tick_time);
      break;
    }
    case VALUE_TOP_CALENDAR_WEEK__US_: {
      strftime(text, 3, "%U", tick_time);
      break;
    }
    case VALUE_TOP_DAY_OF_THE_WEEK: {
      if(enamel_get_first_day_of_the_week()) {
        snprintf(text, 3, "%02d", tick_time->tm_wday+1);
      } else {
        strftime(text, 3, "0%u", tick_time);
      }
      break;
    }
  }
}

static void startup_handler(void* data) {
  // store current value for comparison
  memcpy(current_text[CUT_UP_TOP], update_text[CUT_UP_TOP], 2);
  memcpy(current_text[CUT_UP_BOTTOM], update_text[CUT_UP_BOTTOM], 2);

  time_t now = time(NULL);
  struct tm* tick_time = localtime(&now);
  set_value(update_text[CUT_UP_TOP], enamel_get_value_top(), tick_time);
  set_value(update_text[CUT_UP_BOTTOM], enamel_get_value_bottom(), tick_time);

  handle_leading_zeroes();
  cut_up_update(memcmp(current_text[CUT_UP_TOP], update_text[CUT_UP_TOP], 2), memcmp(current_text[CUT_UP_BOTTOM], update_text[CUT_UP_BOTTOM], 2), true);
}

static void enamel_settings_received_handler(void* context) {
  load_settings();
  startup_handler(context);
}

static void timer_exit(void* data) {
  window_stack_pop_all(false);
}

static void exit_app() {
  if(exiting) return;
  exiting = true;
  if(APP_LAUNCH_QUICK_LAUNCH == launch_reason()) {
    exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
  }
  if(ANIMATIONS_STARTUP_AND_TRANSITIONS == settings.animations) {
    cut_up_update(true, true, false);
    app_timer_register(2*PBL_DISPLAY_WIDTH+PBL_DISPLAY_WIDTH/3, timer_exit, NULL);
  } else {
    window_stack_pop_all(false);
  }
}

static void timer_handler(void* data) {
  exit_app();
}

void start() {
  time_t now = time(NULL);
  struct tm* tick_time = localtime(&now);

  if(APP_LAUNCH_QUICK_LAUNCH == launch_reason() && enamel_get_show_time_on_start()) {
    strftime(update_text[CUT_UP_TOP], 3, clock_is_24h_style() ? "%H" : "%I", tick_time);
    strftime(update_text[CUT_UP_BOTTOM], 3, "%M", tick_time);
    uint8_t time_zeroes = enamel_get_show_leading_zeroes_on_time();
    if(time_zeroes) {
      if('0' == update_text[CUT_UP_TOP][0]) {
        update_text[CUT_UP_TOP][0] = ' ';
      }
      if(SHOW_LEADING_ZEROES_ON_TIME_HIDE == time_zeroes) {
        if('0' == update_text[CUT_UP_BOTTOM][0]) {
          update_text[CUT_UP_BOTTOM][0] = ' ';
        }
      }
    }
    cut_up_update(false, false, true);
  }

  int32_t timeout = (enamel_get_quick_launch_timeout()*1000)/QUICK_LAUNCH_TIMEOUT_PRECISION;
  if(timeout && APP_LAUNCH_QUICK_LAUNCH == launch_reason()) {
    app_timer_register(timeout, timer_handler, NULL);
  }

  app_timer_register(10, startup_handler, NULL);
}

static void click_handler(ClickRecognizerRef recognizer, void *context) {
  exit_app();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, click_handler);
}


void handle_init(void) {
  enamel_init();
  enamel_settings_received_subscribe(enamel_settings_received_handler, NULL);
  events_app_message_open();
  load_settings();
  the_window = cut_up_init();
  window_set_click_config_provider(the_window, click_config_provider);
}

void handle_deinit(void) {
  cut_up_deinit();
  enamel_deinit();
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
