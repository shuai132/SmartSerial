#include "SmartSerial.h"
#include "log.h"

int main(int argc, char **argv) {
    std::string portName = argc >= 2 ? argv[1] : "/dev/ttyUSB0";

    SmartSerial smartSerial(portName);

    // handle will on other thread
    smartSerial.setOnReadHandle([](const uint8_t* data, size_t size) {
        LOGI("on read handle: size:%zu, data:%s", size, data);
    });

    smartSerial.write("hello world");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}
