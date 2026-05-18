/**
 * @file PwmControl.h
 * @brief Header file for PWM (Pulse Width Modulation) control functionality
 * 
 * This file defines the PwmControl class which provides an interface for controlling
 * PWM signals on Arduino pins. It allows control of motor speeds, LED brightness, or
 * other PWM-capable devices by managing duty cycle and on/off states.
 * 
 * @class PwmControl
 * @brief Manages PWM signal generation on a specified Arduino pin
 * 
 * This class encapsulates PWM control operations, allowing users to:
 * - Initialize PWM on a specific pin
 * - Set variable speed/brightness (0-255)
 * - Enable full speed operation
 * - Disable (stop) PWM output
 * 
 * Typical usage for controlling a motor or device:
 * @code
 * PwmControl pump(9);  // Create PWM controller on pin 9
 * pump.begin();        // Initialize PWM
 * pump.setSpeed(50);   // Set to 50% speed
 * pump.on();           // Full speed
 * pump.off();          // Stop
 * @endcode
 */
#ifndef PWMCONTROL_H
#define PWMCONTROL_H

#include <Arduino.h>

class PwmControl {
public:
    PwmControl(int pin, bool digitalOutput = false);
    void begin();
    void setSpeed(int speed);  // speed: 0-100% mapped to PWM 0-255, or digital HIGH/LOW
    void on();                 // full speed / HIGH
    void off();                // stop / LOW

private:
    int _pin;
    bool _digitalOutput;
};

#endif