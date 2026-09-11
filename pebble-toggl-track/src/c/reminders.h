#pragma once

// Wakeup-based reminders: "timer still running?" and "no timer running".
void reminders_handle_launch(void);   // call in init, before the UI is shown
void reminders_schedule(void);        // call on exit
