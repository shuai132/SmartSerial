/***
 * This example expects the serial port has a loopback on it.
 *
 * Alternatively, you could use an Arduino:
 *
 * <pre>
 *  void setup() {
 *    Serial.begin(<insert your baudrate here>);
 *  }
 *
 *  void loop() {
 *    if (Serial.available()) {
 *      Serial.write(Serial.read());
 *    }
 *  }
 * </pre>
 */

#include "SmartSerial.h"
#include "log.h"

int main(int argc, char **argv) {
    std::string portName("/dev/tty.usbserial-1440");
    if(argc >= 2) {
        portName = argv[1];
    }

    SmartSerial smartSerial(portName);

    smartSerial.setOnReadHandle([&](const uint8_t* data, size_t size) {
        LOGI("on read handle: size:%zu, data:%s", size, data);
    });

    smartSerial.write("hello world");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}
