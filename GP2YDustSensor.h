#include <stdint.h>

enum GP2YDustSensorType
{
    GP2Y1010AU0F,
    GP2Y1014AU0F
};

class GP2YDustSensor
{
    private:
        GP2YDustSensorType type;
        uint32_t maxAdc;
        uint8_t ledOutputPin;
        uint8_t analogReadPin;
        float vRef;
        float zeroDustVoltage;
        float minDustVoltage;
        float minZeroDustVoltage;
        float maxZeroDustVoltage;
        float typZeroDustVoltage;
        float currentBaselineCandidate;
        bool hasBaselineCandidate;
        uint16_t readCount = 0;
        float calibrationFactor;
        float sensitivity;
        int16_t *runningAverageBuffer;
        int runningAverageCount;
        int nextRunningAverageCounter;
        int runningAverageCounter;
        const uint8_t BASELINE_CANDIDATE_MIN_READINGS = 10;

    protected:
        uint16_t readDustRawOnce();
        void updateRunningAverage(uint16_t dustDensity);

    public:
        GP2YDustSensor(GP2YDustSensorType type,
                uint8_t ledOutputPin,
                uint8_t analogReadPin,
                uint16_t runningAverageCount = 60,
                float voltageReference = 5.0);
        ~GP2YDustSensor();
        void begin();
        uint16_t getDustDensity(uint16_t numSamples = 20);
        uint16_t getRunningAverage();
        float getBaseline();
        void setBaseline(float zeroDustVoltage);
        float getBaselineCandidate();
        void setSensitivity(float sensitivity);
        float getSensitivity();
        void setCalibrationFactor(float slope);
};
