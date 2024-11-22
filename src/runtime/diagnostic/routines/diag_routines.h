#ifndef CANT_DIAG_ROUTINES_H
#define CANT_DIAG_ROUTINES_H

#include "../routine_manager.h"
#include <stdint.h>
#include <stdbool.h>

// Battery Test Routine
bool battery_test_start(const uint8_t* data, uint16_t length);
bool battery_test_stop(void);
bool battery_test_get_result(RoutineResult* result);

// Sensor Calibration Routine
bool sensor_calibration_start(const uint8_t* data, uint16_t length);
bool sensor_calibration_stop(void);
bool sensor_calibration_get_result(RoutineResult* result);

// Actuator Test Routine
bool actuator_test_start(const uint8_t* data, uint16_t length);
bool actuator_test_stop(void);
bool actuator_test_get_result(RoutineResult* result);

// Memory Check Routine
bool memory_check_start(const uint8_t* data, uint16_t length);
bool memory_check_stop(void);
bool memory_check_get_result(RoutineResult* result);

// Network Test Routine
bool network_test_start(const uint8_t* data, uint16_t length);
bool network_test_stop(void);
bool network_test_get_result(RoutineResult* result);

// Routine timeout handlers
void battery_test_timeout(void);
void sensor_calibration_timeout(void);
void actuator_test_timeout(void);
void memory_check_timeout(void);
void network_test_timeout(void);

#endif // CANT_DIAG_ROUTINES_H 