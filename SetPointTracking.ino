const int thermistorPins[4] = {A0, A1, A2, A3};
const int heaterPin = 9;
const int fanPin = 8;

// Thermistor calibration
const float ADC_26[4] = {501, 493, 500, 509};
const float ADC_31[4] = {409, 422, 438, 448};

// Cohen-Coon PID parameters
const float Kc = -3005.19;
const float Ti = 2.440729;
const float Td = 0.362319;

// Operating conditions
const float heaterPercent = 10.0;
const float fanMin = 0.0;
const float fanMax = 100.0;

const unsigned long sampleTime = 1000;
const unsigned long stepTime = 120000;

// PID variables
float setpoint = 26.5;
float previousTemperature = 26.5;
float integralError = 0;

unsigned long startTime;
unsigned long lastSample;

float readADC(int pin) {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return sum / 10.0;
}

float calibrateTemperature(float adc, int sensor) {
  return 26.0 +
    (ADC_26[sensor] - adc) * 5.0 /
    (ADC_26[sensor] - ADC_31[sensor]);
}

void setup() {
  Serial.begin(9600);

  pinMode(heaterPin, OUTPUT);
  pinMode(fanPin, OUTPUT);

  analogWrite(heaterPin, 26);  // 10% heater PWM
  analogWrite(fanPin, 0);

  startTime = millis();
  lastSample = startTime;

  Serial.println("Time,T1,T2,T3,T4,Average,Setpoint,Fan%,Heater%");
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastSample >= sampleTime) {
    float dt = (currentTime - lastSample) / 1000.0;
    lastSample = currentTime;

    float temperatures[4];
    float averageTemperature = 0;

    for (int i = 0; i < 4; i++) {
      float adc = readADC(thermistorPins[i]);
      temperatures[i] = calibrateTemperature(adc, i);
      averageTemperature += temperatures[i];
    }

    averageTemperature /= 4.0;

    // Setpoint step at 120 seconds
    if (currentTime - startTime >= stepTime) {
      setpoint = 25.5;
    } else {
      setpoint = 26.5;
    }

    // PID calculation
    float error = setpoint - averageTemperature;

    float dTdt =
      (averageTemperature - previousTemperature) / dt;

    float newIntegral = integralError + error * dt;

    float rawOutput = Kc *
      (error + newIntegral / Ti - Td * dTdt);

    float fanOutput = constrain(rawOutput, fanMin, fanMax);

    // Anti-windup
    if (rawOutput == fanOutput ||
        (rawOutput > fanMax && error > 0) ||
        (rawOutput < fanMin && error < 0)) {
      integralError = newIntegral;
    }

    analogWrite(fanPin, (int)(fanOutput * 255.0 / 100.0));

    // Heater remains constant at 10%
    analogWrite(heaterPin, 26);

    previousTemperature = averageTemperature;

    Serial.print((currentTime - startTime) / 1000.0);
    for (int i = 0; i < 4; i++) {
      Serial.print(",");
      Serial.print(temperatures[i], 2);
    }

    Serial.print(",");
    Serial.print(averageTemperature, 2);
    Serial.print(",");
    Serial.print(setpoint, 2);
    Serial.print(",");
    Serial.print(fanOutput, 2);
    Serial.println(",10");
  }
}