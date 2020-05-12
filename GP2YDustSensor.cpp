#include <Arduino.h>

#include "GP2YDustSensor.h"

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
                               uint16_t runningAverageCount,
                               float voltageReference)
{
    this->ledOutputPin = ledOutputPin;
    this->analogReadPin = analogReadPin;
    this->vRef = voltageReference;
    this->type = type;
    this->sensitivity = 0.5; // default sensitivity from datasheet
    this->nextRunningAverageCounter = 0;
    this->hasBaselineCandidate = false;
    this->readCount = 0;
    
    switch (type) {
        case GP2Y1010AU0F:
            // sensitivity: min/typ/max: 0.425 / 0.5 / 0.75 
            // output voltage at no dust: min/typ/max 0v / 0.9v / 1.5v
            
            this->minZeroDustVoltage = 0;
            this->typZeroDustVoltage = 0.9;
            this->maxZeroDustVoltage = 1.5;
            this->zeroDustVoltage = this->minDustVoltage = this->typZeroDustVoltage;
            break;
        case GP2Y1014AU0F:
            // sensitivity: min/typ/max: 0.35 / 0.5 / 0.65 
            // output voltage at no dust: min/typ/max: 0.1v / 0.6v / 1.1v

            this->minZeroDustVoltage = 0.1;
            this->typZeroDustVoltage = 0.6;
            this->maxZeroDustVoltage = 1.1;
            this->zeroDustVoltage = this->minDustVoltage = this->typZeroDustVoltage;
            break;
    }

    this->calibrationFactor = 1;
    this->currentBaselineCandidate = this->typZeroDustVoltage;

    this->runningAverageCount = runningAverageCount;
    if (this->runningAverageCount) {
        this->runningAverageBuffer = new int16_t[this->runningAverageCount];
        // init with -1
        for (uint16_t i = 0; i < this->runningAverageCount; i++) {
            this->runningAverageBuffer[i] = -1;
        }
    }
}

/**
 * Initialize sensor
 */
void GP2YDustSensor::begin()
{
    pinMode(this->ledOutputPin, OUTPUT);
}

/**
 * Sets the voltage at no dust. This baseline is set automatically to a typical value depending on the sensor type
 * But yoy have the option to tweak it
 * 
 * @param float zeroDustVoltage
 */
void GP2YDustSensor::setBaseline(float zeroDustVoltage)
{
    this->zeroDustVoltage = zeroDustVoltage;
}

float GP2YDustSensor::getBaseline()
{
    return this->zeroDustVoltage;
}

/**
* Returns the new baseline candidate, determined after reading enough samples
* (you need at least 1 minute worth of samples to be of any help) 
* 
* @return float baseline candidate scaled voltage
* @see GP2YDustSensor::setBaseline
*/
float GP2YDustSensor::getBaselineCandidate()
{
    if (!hasBaselineCandidate) {
        return this->currentBaselineCandidate;
    }

    float candidate = this->minDustVoltage;
    // reset min voltage to enable selection of new candidate
    this->minDustVoltage = this->maxZeroDustVoltage;
    this->currentBaselineCandidate = this->minDustVoltage;
    readCount = 0; // reset read sample count
    hasBaselineCandidate = false;

    return candidate;
}

/**
 * Set sensitivity in volts/100ug/m3
 * Typical sensitivity is 0.5V, set by default
 * GP2Y1010AU0F sensitivity: min/typ/max: 0.425 / 0.5 / 0.75
 * GP2Y1014AU0F sensitivity: min/typ/max: 0.35 / 0.5 / 0.65 
 * 
 * @param float sensitivity expressed in volts
 */
void GP2YDustSensor::setSensitivity(float sensitivity)
{
    this->sensitivity = sensitivity;
}

/**
 * Get already set sensor sensitivity in volts/100ug/m3
 */
float GP2YDustSensor::getSensitivity()
{
    return this->sensitivity;
}

/**
 * Raw sensor reading from ADC
 * 
 * @return uint16_t value between 0 - 1024
 */
uint16_t GP2YDustSensor::readDustRawOnce()
{
    // Turn on the dust sensor LED by setting digital pin LOW.
    digitalWrite(this->ledOutputPin, LOW);

    // Wait 0.28ms before taking a reading of the output voltage as per spec.
    delayMicroseconds(280);

    // Record the output voltage. This operation takes around 100 microseconds.
    uint16_t VoRaw = analogRead(this->analogReadPin);

    // Turn the dust sensor LED off by setting digital pin HIGH.
    digitalWrite(this->ledOutputPin, HIGH);

    return VoRaw;
}

/**
 * Get average dust density between numSamples in ug/m3
 * With the default value of numSamples (20) the reading should take 200ms
 * 
 * @return uint16_t dust density between 0 and 600 ug/m3
 */
uint16_t GP2YDustSensor::getDustDensity(uint16_t numSamples)
{
    uint32_t total = 0;
    uint16_t avgRaw;
  
    for (uint8_t i = 0; i < numSamples; i++) {
        total += this->readDustRawOnce();
        // Wait for remainder of the 10ms cycle = 10000 - 280 - 100 microseconds.
        delayMicroseconds(9620);
    }

    avgRaw = total / numSamples;

    // we scale up the read ADC voltage to the vRef input range
    // so we can interpret the results based on voltage
    // we assume a 10 bit ADC resolution currently given by analogRead()
    float scaledVoltage = avgRaw * (this->vRef / 1024) * calibrationFactor;

    // determine new baseline candidate
    if (scaledVoltage < this->minDustVoltage && scaledVoltage >= minZeroDustVoltage && scaledVoltage <= maxZeroDustVoltage) {
        this->minDustVoltage = scaledVoltage;
    }

    uint16_t dustDensity;

    if (scaledVoltage < zeroDustVoltage) {
        dustDensity = 0;
    } else {
        // taken from the graph, at 0.4mg dust density we should have 3.05 volts
        // 3.05v ................ 0.4
        // scaledVoltage ........ ?
        // ? = scaledVoltage * 0.4 / 3.05
        // We will try to be smarter and adjust the graph
        // according to the zero dust voltage offset and sensitivity
        // typical zero dust is 0.6V but I observed 0.4V on my sensor
        // sensor sensitivy is 0.5V according to the datasheet
        // dustDensity is expressed in ug/m3
        dustDensity = (scaledVoltage - zeroDustVoltage) / this->sensitivity * 100;
    }

    if (this->runningAverageCount) {
        this->updateRunningAverage(dustDensity);
    }

    if (!hasBaselineCandidate) {
        readCount++;
        if (readCount > BASELINE_CANDIDATE_MIN_READINGS) {
            hasBaselineCandidate = true;
        }
    }

    return dustDensity;
}

/**
 * Get the running average value of dust density using runningAverageCount number of samples
 * Example: If you read the density with getDustDensity() each second and runningAverageCount is 60 (default)
 * you will get a running average for 1 minute
 * 
 * @return uint16_t average dust density value between 0 and 600 ug/m3
 * 
 */
uint16_t GP2YDustSensor::getRunningAverage()
{
    if (!this->runningAverageCount) {
        return -1;
        //throw std::runtime_error("Running average was disabled from constructor. Use runningAverageCount to specify the size.");
    }

  float runningAverage = 0;
  uint16_t sampleCount = 0;

    for (uint16_t i = 0; i < this->runningAverageCount; i++) {
        if (this->runningAverageBuffer[i] != -1) {
            runningAverage += this->runningAverageBuffer[i];
            sampleCount++;
        }
    }

    if (sampleCount == 0) {
        return 0;
    }
    
    runningAverage /= sampleCount;

    return round(runningAverage);
} 

/**
 * Set a calibration factor to improve accuracy
 * Calibrate against known source / precision instrument
 * 
 * @oaram float slope
 */
void GP2YDustSensor::setCalibrationFactor(float slope)
{
    this->calibrationFactor = slope;
}

GP2YDustSensor::~GP2YDustSensor()
{
    if (this->runningAverageBuffer) {
        delete this->runningAverageBuffer;
    }
}

void GP2YDustSensor::updateRunningAverage(uint16_t value)
{
    this->runningAverageBuffer[this->nextRunningAverageCounter] = value;

    this->nextRunningAverageCounter++;
    if (this->nextRunningAverageCounter >= this->runningAverageCount) {
        this->nextRunningAverageCounter = 0; 
    }
}
