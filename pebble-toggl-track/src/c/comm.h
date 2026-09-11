#pragma once

#include <pebble.h>

// AppMessage link to the phone-side JavaScript (src/pkjs/index.js).

void comm_init(void);
void comm_deinit(void);
void comm_send_refresh(void);
void comm_send_start(int32_t project_id, const char *description);
void comm_send_stop(void);
void comm_send_previous(void);
