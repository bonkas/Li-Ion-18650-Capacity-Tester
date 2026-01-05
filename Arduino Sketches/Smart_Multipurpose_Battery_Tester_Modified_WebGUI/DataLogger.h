#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

// ========================================= DATA LOGGER ========================================
// Circular buffer for storing graph data (1 hour at 1 sample/second = 3600 samples)

#define MAX_DATA_POINTS 3600  // 1 hour of data at 1 sample per second
#define DATA_SAMPLE_INTERVAL 1000  // Sample every 1000ms (1 second)

// Data point structure - optimized for memory
struct DataPoint {
    uint32_t timestamp;  // Milliseconds since start of operation
    float voltage;       // Battery voltage
    int16_t current;     // Current in mA (signed for charge/discharge)
    float capacity;      // Accumulated capacity in mAh
};

class DataLogger {
private:
    DataPoint buffer[MAX_DATA_POINTS];
    uint16_t head;           // Next write position
    uint16_t count;          // Number of valid entries
    uint32_t startTime;      // Operation start time
    uint32_t lastSampleTime; // Last sample timestamp

public:
    DataLogger() : head(0), count(0), startTime(0), lastSampleTime(0) {}

    // Reset the logger for a new operation
    void reset() {
        head = 0;
        count = 0;
        startTime = millis();
        lastSampleTime = 0;
    }

    // Add a data point if enough time has passed
    bool addDataPoint(float voltage, int16_t current, float capacity) {
        uint32_t now = millis();

        // Check if enough time has passed since last sample
        if (now - lastSampleTime < DATA_SAMPLE_INTERVAL && lastSampleTime > 0) {
            return false;  // Not time to sample yet
        }

        lastSampleTime = now;

        // Store the data point
        buffer[head].timestamp = now - startTime;
        buffer[head].voltage = voltage;
        buffer[head].current = current;
        buffer[head].capacity = capacity;

        // Advance head pointer (circular)
        head = (head + 1) % MAX_DATA_POINTS;

        // Track count up to max
        if (count < MAX_DATA_POINTS) {
            count++;
        }

        return true;
    }

    // Get number of data points stored
    uint16_t getCount() const {
        return count;
    }

    // Get a data point by index (0 = oldest)
    bool getDataPoint(uint16_t index, DataPoint& point) const {
        if (index >= count) {
            return false;
        }

        // Calculate actual buffer index
        uint16_t bufferIndex;
        if (count < MAX_DATA_POINTS) {
            bufferIndex = index;
        } else {
            bufferIndex = (head + index) % MAX_DATA_POINTS;
        }

        point = buffer[bufferIndex];
        return true;
    }

    // Get the most recent data point
    bool getLatestDataPoint(DataPoint& point) const {
        if (count == 0) {
            return false;
        }

        uint16_t latestIndex = (head == 0) ? MAX_DATA_POINTS - 1 : head - 1;
        point = buffer[latestIndex];
        return true;
    }

    // Get elapsed time since start
    uint32_t getElapsedTime() const {
        return millis() - startTime;
    }

    // Check if logger has data
    bool hasData() const {
        return count > 0;
    }

    // Get data for JSON serialization (returns count of points to send)
    // Use with getDataPoint() to iterate
    uint16_t getDataForTransmit(uint16_t maxPoints = 360) const {
        // Limit points for initial transmission (every 10th point for history)
        if (count <= maxPoints) {
            return count;
        }
        return maxPoints;
    }

    // Get downsampled index for transmission
    uint16_t getDownsampledIndex(uint16_t transmitIndex, uint16_t maxPoints = 360) const {
        if (count <= maxPoints) {
            return transmitIndex;
        }
        // Downsample: pick evenly spaced points
        return (transmitIndex * count) / maxPoints;
    }
};

// Global data logger instance
DataLogger dataLogger;

#endif // DATA_LOGGER_H
