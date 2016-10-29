#ifndef __3DS_EVENT_H__
#define __3DS_EVENT_H__

#include "../../src/event.h"

enum bottom_screen_mode
{
  BOTTOM_SCREEN_MODE_PREVIEW,
  BOTTOM_SCREEN_MODE_KEYBOARD,
  BOTTOM_SCREEN_MODE_MAX
};

enum bottom_screen_mode get_bottom_screen_mode(void);
bool get_allow_focus_changes(void);

void do_unicode_key_event(struct buffered_status *status, bool down, enum keycode code, int unicode);
void do_key_event(struct buffered_status *status, bool down, enum keycode code);
void do_joybutton_event(struct buffered_status *status, bool down, int button);

#endif /* __3DS_EVENT_H__ */
