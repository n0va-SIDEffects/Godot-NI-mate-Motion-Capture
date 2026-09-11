#pragma once

void list_window_push(void);                        // recent entries + projects
void list_window_push_for_text(const char *text);   // pick a project for a dictated text
void list_window_refresh(void);                     // reload rows after the model changed
