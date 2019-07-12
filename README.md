# Sharp GP2Y* Dust Sensor library

Arduino compatible (ESP8266, ESP32, etc) library for the Sharp GP2Y dust sensors:
- GP2Y1010AU0F
- GP2Y1014AU0F

### Features
- multi-reading average
- running average
- runtime calibration
- sensitivity setting
- offset adjustment (zero dust value) with drift correction 

### Basic usage

The library provides an OOP interface for using the dust Sharp sensors.
Create an object of type `GP2YDustSensor` specifying the type, the GPIO pin driving the led and the analog input pin reading the sensor;s output.

```c++
/**
 * @param GP2YDustSensorType type use one of the two supported types
 * @param uint8_t ledOutputPin - the GPIO pin powering up the Sharp IR LED
 * @param uint8_t analogReadPin - the analog input pin connected from the Sharp analog output (Vo).
 * On ESP8266 there is a single A0 pin
 * @param uint16_t runningAverageCount - number of samples taken for the running average.
 * use 0 to disable running average
 */
GP2YDustSensor::GP2YDustSensor(GP2YDustSensorType type,
                               uint8_t ledOutputPin,
                               uint8_t analogReadPin,
                               uint16_t runningAverageCount)
```

The forth parameter can be used to specify how many samples are used for calculating the running average.

Call `dustSensor.init()` somewhere in the `setup()` method to initialize the sensor.
Then use `dustSensor.getDustDensity()` to obtain an average dust average for the number of samples specified (default 20):

```c++
/**
 * Get average dust density between numSamples in ug/m3
 * With the default value of numSamples (20) the reading should take 200ms
 * 
 * @return uint16_t dust density between 0 and 600 ug/m3
 */
uint16_t GP2YDustSensor::getDustDensity(uint16_t numSamples)
```

```c++
#include <GP2YDustSensor.h>

const uint8_t SHARP_LED_PIN = 14;   // Sharp Dust/particle sensor Led Pin
const uint8_t SHARP_VO_PIN = A0;    // Sharp Dust/particle analog out pin used for reading 

GP2YDustSensor dustSensor(GP2YDustSensorType::GP2Y1014AU0F, SHARP_LED_PIN, SHARP_VO_PIN);

void setup() {
  Serial.begin(9600);
  //dustSensor.setBaseline(0.4); // set no dust voltage according to your own experiments
  //dustSensor.setCalibrationFactor(1.1); // calibrate against precision instrument
  dustSensor.begin();
}

void loop() {
  Serial.print("Dust density: ");
  Serial.print(dustSensor.getDustDensity());
  Serial.print(" ug/m3; Running average: ");
  Serial.print(dustSensor.getRunningAverage());
  Serial.println(" ug/m3");
  delay(1000);
}
```

To read a single sample 10ms are needed. By default, `getDustDensity()` reads 20 samples and returns an average. This means a reading will take about 200ms.
Tweak the number of samples either by reducing the number of samples to read faster or increase them to reduce noise by increasing the window of time for averaging. 

### Baseline adjustment (Zero dust value)

The Sharp sensors don't normally output 0 when no dust is present but they offer something like 0.6V , sometimes less, sometimes more. This number is not fixed.
This is accordint to the specs.
After this value, the graph is mostly liniar.  
A typical baseline is set automatically by the library for your sensor, but you have the option to tweak this value.

```c++
/**
 * Sets the voltage at no dust. This baseline is set automatically to a typical value depending on the sensor type
 * But yoy have the option to tweak it
 * 
 * @param float zeroDustVoltage
 */
void GP2YDustSensor::setBaseline(float zeroDustVoltage)
```


### Drift correction

These sensor have an observable random drift for the zero-dust voltage. Usually the zero-dust value gradually increases over time, so in order to improve accuracy an automatic basline calculation is available in the library.

```c++
/**
* Returns the new baseline candidate, determined after reading enough samples
* (you need at least 1 minute worth of samples to be of any help) 
* 
* @return float baseline candidate scaled voltage
* @see GP2YDustSensor::setBaseline
*/
float GP2YDustSensor::getBaselineCandidate()
```

**Pseudocode:**

```
loop:
    Read n samples (could be 1 minute worth of samples)
    Show running average for that interval
    Set new baseline determined after reading n samples to fix the baseline drift
```

```c++
void loop() {
  for (int i; i <= 100; i++) {
      sprintf(logBuffer, "Density: %d ug/m3", dustSensor.getDustDensity());
      Serial.println(logBuffer);
      delay(580);
  }

  dustAverage = dustSensor.getRunningAverage();
  float newBaseline = dustSensor.getBaselineCandidate();
  
  sprintf(logBuffer, "1m Avg Dust Density: %d ug/m3; New baseline: %.4f", dustAverage, newBaseline);
  Serial.println(logBuffer);

  // compensates sensor drift after 1m
  dustSensor.setBaseline(newBaseline);

  delay(1000);
}
```

### Calibration

Using the baseline and the calibration factor you can calibrate the sensor against a precision instrument.
The dust density with correcterd baseline is multiplied by this value to obtain a more accurate dust density reading.
Since these Sharp sensors have a good liniarity, they can be sucessfully calibrated against more precise instruments.

```c++
/**
 * Set a calibration factor to improve accuracy
 * Calibrate against known source / precision instrument
 * 
 * @oaram float slope
 */
void GP2YDustSensor::setCalibrationFactor(float slope)

```

### Sensitivity setting

The Sharp sensors have a typical sensitivity (normally 0.5V / 100ug/m3, which is set by default in this library.
The sensitivity is used by this library to calculate the dust density based on the output voltage given by the sensor.
You can adjust this sensitiity,

```c++
/**
 * Set sensitivity in volts/100ug/m3
 * Typical sensitivity is 0.5V, set by default
 * GP2Y1010AU0F sensitivity: min/typ/max: 0.425 / 0.5 / 0.75
 * GP2Y1014AU0F sensitivity: min/typ/max: 0.35 / 0.5 / 0.65 
 * 
 * @param float sensitivity expressed in volts
 */
void GP2YDustSensor::setSensitivity(float sensitivity)
```

### Reducing noise using the running average

The library maintains a running average of n samples. The number of samples is specified in the constuctor.
```c++
/**
 * Get the running average value of dust density using runningAverageCount number of samples
 * Example: If you read the density with getDustDensity() each second and runningAverageCount is 60 (default)
 * you will get a running average for 1 minute
 * 
 * @return uint16_t average dust density value between 0 and 600 ug/m3
 * 
 */
uint16_t GP2YDustSensor::getRunningAverage()
``` 


