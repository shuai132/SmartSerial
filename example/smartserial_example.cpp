#include "SmartSerial.h"
#include "log.h"

static void listPort() {
    auto ports = serial::list_ports();
    if (ports.empty()) {
        LOGI("No Any Port!");
    }
    for (const auto& info : ports) {
        static int count;
        LOG("list port [%d]:", count++);

        // Port
        // macOS: /dev/cu.usbmodem8D8842A649551
        // Linux: /dev/ttyACM0
        // Windows: COM6
        LOG("port:%s", info.port.c_str());

        // Description
        // macOS: Scope STM32 Virtual ComPort
        // Linux: Scope STM32 Virtual ComPort 8D8842A64955
        // Windows: USB 串行设备 (COM6)
        LOG("description:%s", info.description.c_str());

        // ID
        // mac: USB VID:PID=1234:5740 SNR=8D8842A64955
        // lin: USB VID:PID=1234:5740 SNR=8D8842A64955
        // win: USB\VID_1234&PID_5740&REV_0200
        LOG("hardware_id:%s", info.hardware_id.c_str());
    }
}

int main(int argc, char **argv) {
    std::string portName = argc >= 2 ? argv[1] : "/dev/tty.usbserial-1460";

    listPort();

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
