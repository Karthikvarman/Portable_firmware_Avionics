#include "stabilization.h"
#include <string.h>
static pid_controller_t pid_roll;
static pid_controller_t pid_pitch;
static pid_controller_t pid_yaw_rate;
static pid_controller_t pid_alt;
static stabilization_cmd_t current_cmd;
void stabilization_init(void) {
  pid_init(&pid_roll, 1.5f, 0.05f, 0.1f, -1.0f, 1.0f, 0.01f);
  pid_init(&pid_pitch, 1.5f, 0.05f, 0.1f, -1.0f, 1.0f, 0.01f);
  pid_init(&pid_yaw_rate, 2.0f, 0.1f, 0.0f, -1.0f, 1.0f, 0.01f);
  pid_init(&pid_alt, 1.2f, 0.2f, 0.1f, 0.0f, 1.0f, 0.01f);
  memset(&current_cmd, 0, sizeof(stabilization_cmd_t));
}
void stabilization_set_target(float roll, float pitch, float yaw_rate,
                              float alt) {
  current_cmd.roll_setpoint = roll;
  current_cmd.pitch_setpoint = pitch;
  current_cmd.yaw_rate_setpoint = yaw_rate;
  current_cmd.alt_setpoint = alt;
}
void stabilization_update(float roll, float pitch, float yaw_rate, float alt,
                          float dt) {
  pid_set_dt(&pid_roll, dt);
  pid_set_dt(&pid_pitch, dt);
  pid_set_dt(&pid_yaw_rate, dt);
  pid_set_dt(&pid_alt, dt);
  current_cmd.roll_out = pid_update(&pid_roll, current_cmd.roll_setpoint, roll);
  current_cmd.pitch_out =
      pid_update(&pid_pitch, current_cmd.pitch_setpoint, pitch);
  current_cmd.yaw_out =
      pid_update(&pid_yaw_rate, current_cmd.yaw_rate_setpoint, yaw_rate);
  current_cmd.throttle_out =
      pid_update(&pid_alt, current_cmd.alt_setpoint, alt);
}
void stabilization_get_commands(stabilization_cmd_t *cmd) {
  if (cmd) {
    memcpy(cmd, &current_cmd, sizeof(stabilization_cmd_t));
  }
}