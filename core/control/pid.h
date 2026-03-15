#ifndef PID_H
#define PID_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
  float Kp; 
  float Ki; 
  float Kd; 
  float setpoint;   
  float integral;   
  float prev_error; 
  float prev_input; 
  float output_min;   
  float output_max;   
  float integral_max; 
  float dt;       
  bool first_run; 
} pid_controller_t;
void pid_init(pid_controller_t *pid, float kp, float ki, float kd, float min,
              float max, float dt);
void pid_reset(pid_controller_t *pid);
float pid_update(pid_controller_t *pid, float setpoint, float measurement);
void pid_set_dt(pid_controller_t *pid, float dt);
#endif 