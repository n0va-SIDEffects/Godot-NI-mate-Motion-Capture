#include "ptz.h"

const char *ptz_axis_name(PtzAxis axis) {
  switch (axis) {
    case PTZ_AXIS_TILT:  return "TILT";
    case PTZ_AXIS_PAN:   return "PAN";
    case PTZ_AXIS_ZOOM:  return "ZOOM";
    case PTZ_AXIS_FOCUS: return "FOCUS";
    default:             return "?";
  }
}

const char *gyro_axes_name(GyroAxes mode) {
  switch (mode) {
    case GYRO_AXES_PAN_TILT:  return "PAN + TILT";
    case GYRO_AXES_PAN_ZOOM:  return "PAN + ZOOM";
    case GYRO_AXES_TILT_ONLY: return "NUR TILT";
    default:                  return "?";
  }
}
