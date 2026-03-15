#ifndef STABILIZATION_H
#define STABILIZATION_H
#include "pid.h"
typedef struct {
  float roll_setpoint;     
  float pitch_setpoint;    
  float yaw_rate_setpoint; 
  float alt_setpoint;      
  float roll_out;
  float pitch_out;
  float yaw_out;
  float throttle_out;
} stabilization_cmd_t;
void stabilization_init(void);
void stabilization_update(float roll, float pitch, float yaw_rate, float alt,
                          float dt);
void stabilization_get_commands(stabilization_cmd_t *cmd);
void stabilization_set_target(float roll, float pitch, float yaw_rate,
                              float alt);
#endif 