#pragma once

// Tile screen: up to four favourite entries plus "Diktieren" and "Liste".
void favorites_window_push(void);
void favorites_window_refresh(void);
void favorites_window_close(void);   // remove from the stack if present
