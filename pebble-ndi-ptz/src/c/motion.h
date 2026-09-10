/*
 * Motion control: turns the watch's orientation (measured with the
 * accelerometer as a gravity vector) into pan/tilt/zoom deflections.
 *
 * The Pebble SDK exposes the accelerometer, not a raw gyroscope stream.
 * For steering a camera this is actually what we want: the accelerometer
 * gives the *absolute* tilt of the wrist (gravity direction), so holding the
 * wrist at a fixed angle produces a constant camera speed, and levelling the
 * wrist stops the camera. A gyroscope would only report rotation *rate*.
 */
#pragma once

#include "ptz_app.h"

/* Deflections are -100..100. x = roll (wrist rotated left/right),
 * y = pitch (top edge of the watch tilted down/up). */
typedef void (*MotionUpdateHandler)(int8_t x, int8_t y);

void motion_init(void);
void motion_deinit(void);

/* Start/stop delivering accelerometer updates (~10 per second). */
void motion_set_active(bool active, MotionUpdateHandler handler);
bool motion_is_active(void);

/* Capture the current orientation as the neutral (zero) position. */
void motion_calibrate(void);
