#include "SmartSerial.h"
#include "log.h"

int main(int argc, char **argv) {
    std::string portName = argc >= 2 ? argv[1] : "/dev/tty.usbserial-1460";

    SmartSerial smartSerial(portName);

    // handle will on other thread
    smartSerial.setOnOpenHandle([](bool isOpen) {
        LOGI("on open handle: %d", isOpen);
    });

    // handle will on other thread
    smartSerial.setOnReadHandle([&](const uint8_t* data, size_t size) {
        static int count = 0;
        auto str = std::string((char*)data, size);
        LOGI("on read handle: size:%zu, data:%s, count=%d", size, str.c_str(), count++);
    });

    for(;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        smartSerial.write("hello world");
    }

    return 0;
}
