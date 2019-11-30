#include "SmartSerial.h"

#include <utility>
#include <regex>
#include "log.h"

template<typename T, typename... Args>
static std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

SmartSerial::SmartSerial(const std::string& port, uint32_t baudrate)
        : serial_(make_unique<Serial>("", baudrate)) {
    serial_->setPort(port);

    if (!port.empty()) {
        try {
            serial_->open();
            updateOpenState();
        } catch (const std::exception& e) {
            LOGE("open port exception: %s", e.what());
        }
    }

    // 读取阻塞的超时时间ms 也影响重连判断的速度 1s
    serial_->setTimeout(serial::Timeout::simpleTimeout(1000));

    auto thread = new std::thread([this]{
        while(running_) {
            try {
                if (not serial_->isOpen()) {
                    serial_->setPort(guessPortName());
                    LOGD("try open: %s", serial_->getPort().c_str());
                    serial_->open();
                    updateOpenState();
                } else {
//                    static int count;
//                    LOGD("check count:%d", count++);
#if defined(_WIN32)
                    if (not serial_->available()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
#else
                    serial_->waitReadable();
#endif
                    size_t validSize = serial_->available();
                    if (validSize > 0) {
                        size_t size = serial_->read(buffer_, validSize <= BUFFER_SIZE ? validSize : BUFFER_SIZE);
                        if(size > 0 && onReadHandle_) {
                            onReadHandle_(buffer_, size);
                        }
                    }
                }
            } catch (const std::exception& e) {
                LOGD("monitorThread_ exception: %s", e.what());
                std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL_SEC));
                serial_->close();
                updateOpenState();
            }
        }
        serial_->close();
    });
    monitorThread_ = std::unique_ptr<std::thread>(thread);
}

SmartSerial::~SmartSerial() {
    running_ = false;
    monitorThread_->join();
}

void SmartSerial::setOnReadHandle(const OnReadHandle& handle) {
    onReadHandle_ = handle;
}

void SmartSerial::setOnOpenHandle(const OnOpenHandle& handle) {
    onOpenHandle_ = handle;
}

bool SmartSerial::write(const uint8_t* data, size_t size) {
    try {
        // 防止一次不能发送完 测试发现实际上即使在极端情况下也都能一次发送完的
        size_t hasWriteSize = 0;
        while (hasWriteSize < size) {
            size_t writeSize = serial_->write(data + hasWriteSize, size - hasWriteSize);
            if (writeSize == 0) {
                LOGE("serial send payload error");
                return false;
            }
            hasWriteSize += writeSize;
        }
        return true;
    } catch (const std::exception& e) {
        LOGE("write error: %s", e.what());
    }
    return false;
}

bool SmartSerial::write(const std::string& data) {
    return write((uint8_t*)data.data(), data.length());
}

bool SmartSerial::write(const std::vector<uint8_t>& data) {
    return write(data.data(), data.size());
}

SmartSerial::Serial* SmartSerial::getSerial() {
    return serial_.get();
}

void SmartSerial::updateOpenState() {
    bool isOpen = serial_->isOpen();
    LOGD("open state:%d", isOpen);
    if (onOpenHandle_) {
        onOpenHandle_(isOpen);
    }
}

void SmartSerial::setPortName(std::string portName) {
    LOGD("setPortName: %s", portName.c_str());
    portName_ = std::move(portName);
}

std::string SmartSerial::guessPortName() {
    if (!portName_.empty())
        return portName_;

    auto ports = serial::list_ports();
    for (const auto& info : ports) {
        const auto& hardwareId = info.hardware_id;
        // mac: USB VID:PID=1234:5740 SNR=8D8842A64955
        // lin: USB VID:PID=1234:5740 SNR=8D8842A64955
        // win: USB\VID_1234&PID_5740&REV_0200

        if (hardwareId == "n/a") continue;

        if (VID_.empty() and PID_.empty()) {
            LOGD("auto select a port: %s", info.port.c_str());
            return info.port;
        }

#if defined(_WIN32)
        std::regex re("VID_(.*)&PID_(.*)&");
#else
        std::regex re("VID:PID=(.*):(.*) ");
#endif
        std::smatch results;
        auto r = std::regex_search(hardwareId, results, re);
        if (r and results[1] == VID_ and results[2] == PID_) {
            LOGD("match device: vid:%s, pid:%s", VID_.c_str(), PID_.c_str());
            return info.port;
        }
    }
    return portName_;
}

void SmartSerial::setVidPid(std::string vid, std::string pid) {
    VID_ = std::move(vid);
    PID_ = std::move(pid);
}
