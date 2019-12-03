#include "SmartSerial.h"

#include <utility>
#include <regex>
#include "log.h"

template<typename T, typename... Args>
static std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

SmartSerial::SmartSerial(const std::string& port, uint32_t baudrate, OnOpenHandle handle)
        : serial_(make_unique<Serial>("", baudrate)), onOpenHandle_(std::move(handle)) {
    setPortName(port);

    if (!port.empty()) {
        try {
            serial_->open();
            updateOpenState();
        } catch (const std::exception& e) {
            LOGE("open failed: %s", e.what());
        }
    }

    // 读取阻塞的超时时间ms 也影响重连判断的速度 1s
    serial_->setTimeout(serial::Timeout::simpleTimeout(1000));

    monitorThread_ = make_unique<std::thread>([this]{
        while(running_) {
            try {
                if (not serial_->isOpen()) {
                    auto portName = guessPortName();
                    if (portName.empty()) {
                        LOGD("wait device...");
                        std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL_SEC));
                        continue;
                    }
                    setPortName(portName);
                    LOGD("try open: %s", portName.c_str());
                    serial_->open();
                    updateOpenState();
                } else {
//                    static int count;
//                    LOGD("check count:%d", count++);
#if defined(_WIN32)
                    // windows不支持waitReadable 采用轮询方式读取数据
                    // 实际上在大数据量传输时更有效率
                    // 100ms是为了相对及时取数据又不占用过多CPU
                    if (not serial_->available()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
#else
                    // unix系统使用阻塞方式读取 更具功耗优势
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
}

SmartSerial::SmartSerial(const std::string& vid, const std::string& pid, SmartSerial::OnOpenHandle handle)
    : SmartSerial("", 115200, std::move(handle)) {
    setVidPid(vid, pid);
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
    if (not serial_->isOpen()) {
        LOGD("serial not open, abort write!");
        return false;
    }
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

void SmartSerial::setPortName(const std::string& portName) {
    LOGD("setPortName: %s", portName.c_str());
    auto oldName = serial_->getPort();
    if (portName == oldName) return;
    serial_->setPort(portName);
    if (serial_->isOpen()) {
        serial_->close();
        updateOpenState();
    }
}

std::string SmartSerial::getPortName() {
    return serial_->getPort();
}

void SmartSerial::setVidPid(std::string vid, std::string pid) {
    vid_ = std::move(vid);
    pid_ = std::move(pid);

    std::transform(vid_.begin(), vid_.end(), vid_.begin(), tolower);
    std::transform(pid_.begin(), pid_.end(), pid_.begin(), tolower);
}

bool SmartSerial::isOpen() {
    return isOpen_;
}

void SmartSerial::updateOpenState() {
    bool isOpen = serial_->isOpen();
    if (isOpen_ == isOpen) return;
    LOGD("open state: %d", isOpen);
    isOpen_ = isOpen;
    if (onOpenHandle_) {
        onOpenHandle_(isOpen);
    }
}

std::string SmartSerial::guessPortName() {
    auto portName = serial_->getPort();
    if (!portName.empty())
        return portName;

    if (vid_.empty() and pid_.empty()) {
        LOGD("not set VID and PID");
        return portName;
    }

    auto ports = serial::list_ports();
    for (const auto& info : ports) {
        const auto& hardwareId = info.hardware_id;
        // mac: USB VID:PID=1234:5740 SNR=8D8842A64955
        // lin: USB VID:PID=1234:5740 SNR=8D8842A64955
        // win: USB\VID_1234&PID_5740&REV_0200

        if (hardwareId == "n/a") continue;

#if defined(_WIN32)
        std::regex re("VID_(.*)&PID_(.*)&");
#else
        std::regex re("VID:PID=(.*):(.*) ");
#endif
        std::smatch results;
        auto r = std::regex_search(hardwareId, results, re);
        if (r) {
            auto vid = results[1].str();
            auto pid = results[2].str();
            std::transform(vid.begin(), vid.end(), vid.begin(), tolower);
            std::transform(pid.begin(), pid.end(), pid.begin(), tolower);

            if (vid == vid_ and pid == pid_) {
                LOGD("match device: vid:%s, pid:%s", vid_.c_str(), pid_.c_str());
                return info.port;
            }
        }
    }
    return portName;
}
