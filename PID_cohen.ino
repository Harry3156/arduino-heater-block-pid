
const int thermistorPins[4] = {A0, A1, A2, A3};
const int heaterPin = 9;
const int fanPin = 8;  // PWM pin (Uno/Nano)

// Calibration values
const float ADC_26[4] = {501, 493, 500, 509};
const float ADC_31[4] = {409, 422, 438, 448};

// PID parameters: Cohen-Coon
const float Kc = -3005.19;
const float Ti = 2.440729;
const float Td = 0.362319;

// Temperature setpoint (change if required)
const float setpoint = 26.5;

// Heater PWM values
const int heater10 = 26;   // approximately 10%
const int heater50 = 128;  // approximately 50%

// Fan limits
const float fanMin = 0.0;
const float fanMax = 100.0;

const unsigned long sampleInterval = 1000;

unsigned long startTime;
unsigned long lastSample = 0;

float temperatures[4];
float averageTemperature = 0;

float integralError = 0;
float previousTemperature = 0;
float fanOutput = 15.0;

bool firstReading = true;

float readADC(int pin) {
  long total = 0;

  for (int i = 0; i < 10; i++) {
    total += analogRead(pin);
    delay(2);
  }

  return total / 10.0;
}

float calibrateTemperature(float adc, int sensor) {
  return 26.0 +
    (ADC_26[sensor] - adc) * 5.0 /
    (ADC_26[sensor] - ADC_31[sensor]);
}

int percentToPWM(float percentage) {
  return constrain(
    round(percentage * 255.0 / 100.0),
    0, 255
  );
}

void setup() {
  Serial.begin(9600);

  pinMode(heaterPin, OUTPUT);
  pinMode(fanPin, OUTPUT);

  // Initial conditions
  analogWrite(heaterPin, heater10);
  analogWrite(fanPin, percentToPWM(15));

  startTime = millis();

  Serial.println(
    "Time\tT1\tT2\tT3\tT4\tAverage\tSP\tFan%\tHeater%\tStatus"
  );
}

void loop() {

  unsigned long elapsed = millis() - startTime;

  // HEATER DISTURBANCE
  if (elapsed >= 180000 && elapsed < 240000) {
    analogWrite(heaterPin, heater50);
  }
  else {
    analogWrite(heaterPin, heater10);
  }

  // TEMPERATURE SAMPLING AND PID
  if (millis() - lastSample >= sampleInterval) {

    unsigned long currentTime = millis();
    float dt = (currentTime - lastSample) / 1000.0;
    lastSample = currentTime;

    float sum = 0;

    for (int i = 0; i < 4; i++) {
      float adc = readADC(thermistorPins[i]);
      temperatures[i] = calibrateTemperature(adc, i);
      sum += temperatures[i];
    }

    averageTemperature = sum / 4.0;

    // First measurement initialization
    if (firstReading) {
      previousTemperature = averageTemperature;
      firstReading = false;
    }

    // PID calculations
    float error = setpoint - averageTemperature;

    // Derivative on measurement
    float dTdt =
      (averageTemperature - previousTemperature) / dt;

    // Candidate integral
    float candidateIntegral =
      integralError + error * dt;

    // PID output
    float rawOutput =
      Kc * (
        error
        + candidateIntegral / Ti
        - Td * dTdt
      );

    // Anti-windup
    if ((rawOutput >= fanMin && rawOutput <= fanMax) ||
        (rawOutput > fanMax && Kc * error < 0) ||
        (rawOutput < fanMin && Kc * error > 0)) {

      integralError = candidateIntegral;
    }

    // Recalculate PID output
    rawOutput =
      Kc * (
        error
        + integralError / Ti
        - Td * dTdt
      );

    fanOutput = constrain(rawOutput, fanMin, fanMax);

    // Apply fan PWM
    analogWrite(fanPin, percentToPWM(fanOutput));

    previousTemperature = averageTemperature;

    // Serial output
    Serial.print(elapsed / 1000.0, 1);
    Serial.print("\t");

    for (int i = 0; i < 4; i++) {
      Serial.print(temperatures[i], 2);
      Serial.print("\t");
    }

    Serial.print(averageTemperature, 2);
    Serial.print("\t");
    Serial.print(setpoint, 2);
    Serial.print("\t");
    Serial.print(fanOutput, 2);
    Serial.print("\t");

    if (elapsed >= 180000 && elapsed < 240000)
      Serial.print("50%");
    else
      Serial.print("10%");

    Serial.print("\t");

    if (elapsed < 180000)
      Serial.println("NORMAL");
    else if (elapsed < 240000)
      Serial.println("HEATER_DISTURBANCE");
    else
      Serial.println("DISTURBANCE_REMOVED");
  }
}

