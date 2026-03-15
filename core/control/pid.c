#include "pid.h"
#include <math.h>
void pid_init(pid_controller_t *pid, float kp, float ki, float kd, float min,
              float max, float dt) {
  pid->Kp = kp;
  pid->Ki = ki;
  pid->Kd = kd;
  pid->output_min = min;
  pid->output_max = max;
  pid->integral_max = max > 0 ? max : 100.0f; 
  pid->dt = (dt > 0) ? dt : 0.01f;
  pid_reset(pid);
}
void pid_reset(pid_controller_t *pid) {
  pid->integral = 0.0f;
  pid->prev_error = 0.0f;
  pid->prev_input = 0.0f;
  pid->first_run = true;
}
float pid_update(pid_controller_t *pid, float setpoint, float measurement) {
  float error = setpoint - measurement;
  float p_term = pid->Kp * error;
  pid->integral += (error * pid->Ki * pid->dt);
  if (pid->integral > pid->integral_max)
    pid->integral = pid->integral_max;
  if (pid->integral < -pid->integral_max)
    pid->integral = -pid->integral_max;
  float d_term = 0.0f;
  if (!pid->first_run) {
    d_term = -pid->Kd * (measurement - pid->prev_input) / pid->dt;
  }
  pid->prev_input = measurement;
  pid->prev_error = error;
  pid->first_run = false;
  float output = p_term + pid->integral + d_term;
  if (output > pid->output_max)
    output = pid->output_max;
  if (output < pid->output_min)
    output = pid->output_min;
  return output;
}
void pid_set_dt(pid_controller_t *pid, float dt) {
  if (dt > 0)
    pid->dt = dt;
}