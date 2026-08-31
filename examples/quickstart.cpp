// A short tour of the DcMsg API: scalars, a nested message, a fixed array,
// and basic error handling.

#include <dcmsg/dcmsg.h>

#include <cstdio>
#include <string>
#include <vector>

int main()
{
    DS::DcMsg sensor;
    sensor.AddUInt("Channel", 3);
    sensor.AddDouble("Value", 101.15);
    sensor.AddString("Label", "sensor-3");

    std::vector<float> samples = {1.1f, 2.2f, 3.3f};
    sensor.AddFloatArray("Samples", samples);

    DS::DcMsg frame;
    frame.AddMessage("Sensor", sensor);
    frame.AddULong("TimestampMs", 1732000000000ULL);

    // Serialize, then decode as if it had just arrived over the wire.
    uint64_t size = 0;
    void* buffer = frame.GetData(size);
    DS::DcMsg received(buffer, size);
    if (!received.IsValid())
    {
        printf("Failed to parse the message!\n");
        return 1;
    }

    DS::DcMsg receivedSensor;
    received.GetMessage("Sensor", receivedSensor);

    uint32_t channel = 0;
    double value = 0.0;
    std::string label;
    std::vector<float> receivedSamples;
    receivedSensor.GetUInt("Channel", channel);
    receivedSensor.GetDouble("Value", value);
    receivedSensor.GetString("Label", label);
    receivedSensor.GetFloatArray("Samples", receivedSamples);

    uint64_t timestamp = 0;
    received.GetULong("TimestampMs", timestamp);

    printf("Timestamp: %llu\n", static_cast<unsigned long long>(timestamp));
    printf("Channel %u (%s): %.2f\n", channel, label.c_str(), value);
    printf("Samples:");
    for (float s : receivedSamples)
    {
        printf(" %.2f", s);
    }
    printf("\n");

    // Asking for a field with the wrong type fails cleanly, and
    // GetLastError() explains why.
    int32_t wrongType = 0;
    if (!receivedSensor.GetInt("Value", wrongType))
    {
        printf("Expected failure: field 'Value' is not an INT (error code %d)\n",
               static_cast<int>(receivedSensor.GetLastError()));
    }

    return 0;
}
