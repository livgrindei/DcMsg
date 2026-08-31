"""A short tour of the DcMsg Python port: scalars, a nested message, and a
fixed array. Mirrors examples/quickstart.cpp.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))
from dcmsg import DcMsg


def main():
    sensor = DcMsg()
    sensor.AddUInt("Channel", 3)
    sensor.AddDouble("Value", 101.15)
    sensor.AddString("Label", "sensor-3")
    sensor.AddFloatArray("Samples", [1.1, 2.2, 3.3])

    frame = DcMsg()
    frame.AddMessage("Sensor", sensor)
    frame.AddULong("TimestampMs", 1732000000000)

    # Serialize, then decode as if it had just arrived over the wire.
    received = DcMsg(frame.GetData())

    values = received.GetDictionary()["Sensor"].GetDictionary()
    timestamp = received.GetDictionary()["TimestampMs"]

    print(f"Timestamp: {timestamp}")
    print(f"Channel {values['Channel']} ({values['Label']}): {values['Value']:.2f}")
    print("Samples:", list(values["Samples"]))


if __name__ == "__main__":
    main()
