#include "PwmControl.h"

PwmControl::PwmControl(int pin, bool digitalOutput)
  : _pin(pin), _digitalOutput(digitalOutput) {}

void PwmControl::begin() {
    pinMode(_pin, OUTPUT);
    off(); // Start with the device off
}

void PwmControl::setSpeed(int speed) {
    speed = constrain(speed, 0, 100);
    int pwmValue = 0;
    if (speed > 0) {
        const int MIN_USABLE_PWM = 190;  // Tune this
        const int MAX_PWM = 255;
        pwmValue = map(speed, 1, 100, MIN_USABLE_PWM, MAX_PWM);
    }
    
    if (_digitalOutput) {
        digitalWrite(_pin, pwmValue > 0 ? HIGH : LOW);
    } else {
#if defined(digitalPinToTimer) && defined(NOT_ON_TIMER)
        if (digitalPinToTimer(_pin) == NOT_ON_TIMER) {
            digitalWrite(_pin, pwmValue > 0 ? HIGH : LOW);
        } else {
            analogWrite(_pin, pwmValue);
        }
#else
        analogWrite(_pin, pwmValue);
#endif
    }
}

void PwmControl::on() {
    if (_digitalOutput) {
        digitalWrite(_pin, HIGH);
    } else {
        setSpeed(255); // Set to full speed
    }
}

void PwmControl::off() {
    if (_digitalOutput) {
        digitalWrite(_pin, LOW);
    } else {
        analogWrite(_pin, 0); // Turn off PWM output
    }
}