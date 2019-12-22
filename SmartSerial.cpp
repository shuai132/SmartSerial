#include <utility>
#include <regex>

#include "SmartSerial.h"

//#define LOG_SHOW_VERBOSE
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
        } catch (const serial::SerialException& e) {
            LOGE("open failed: %s", e.what());
        }
    }

    monitorThread_ = make_unique<std::thread>([this]{
        while(running_) {
            try {
                std::lock_guard<std::recursive_mutex> lockGuard(serialMutex_);
                if (not serial_->isOpen()) {
                    serialMutex_.unlock();
                    auto portName = guessPortName();
                    if (portName.empty() || not autoOpen_) {
                        LOGV("wait device...");
                        std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL_SEC));
                        continue;
                    }
                    setPortName(portName);
                    LOGD("try open: %s", portName.c_str());
                    {
                        std::lock_guard<std::recursive_mutex> lockGuard2(serialMutex_);
                        serial_->open();
                    }
                    updateOpenState();
                } else {
                    /// still locked here!
                    // 为了在大数据量传输时更有效率和兼容Windows 采用轮询方式读取数据
                    // 为了相对及时取数据又不占用过多CPU 将进行动态调整读取等待的延迟
                    // 也即: 默认等待延时100ms 距离上次接收时间小于1s时等待时间为1ms

                    size_t validSize = serial_->available();
                    auto now = std::chrono::steady_clock::now();

                    if (validSize == 0) {
                        serialMutex_.unlock();
                        bool fastMode = now - lastReadTime_ < std::chrono::seconds(1);
                        std::this_thread::sleep_for(std::chrono::milliseconds(fastMode ? 1 : 100));
                        continue;
                    }

                    lastReadTime_ = now;

                    size_t size = serial_->read(buffer_, validSize <= BUFFER_SIZE ? validSize : BUFFER_SIZE);
                    serialMutex_.unlock();
                    std::lock_guard<std::recursive_mutex> lockGuard2(settingMutex_);
                    if(size > 0 && onReadHandle_) {
                        onReadHandle_(buffer_, size);
                    }
                }
            } catch (const serial::SerialException& e) {
                LOGD("monitorThread_ exception: %s", e.what());
                std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL_SEC));
                {
                    std::lock_guard<std::recursive_mutex> lockGuard(serialMutex_);
                    serial_->close();
                }
                updateOpenState();
            }
        }
        std::lock_guard<std::recursive_mutex> lockGuard(serialMutex_);
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
    std::lock_guard<std::recursive_mutex> lockGuard(settingMutex_);
    onReadHandle_ = handle;
}

void SmartSerial::setOnOpenHandle(const OnOpenHandle& handle) {
    std::lock_guard<std::recursive_mutex> lockGuard(settingMutex_);
    onOpenHandle_ = handle;
}

bool SmartSerial::write(const uint8_t* data, size_t size) {
    std::lock_guard<std::recursive_mutex> lockGuard(serialMutex_);

    if (not isOpen_) {
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
    } catch (const serial::SerialException& e) {
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
    std::lock(settingMutex_, serialMutex_);
    std::lock_guard<std::recursive_mutex> lockGuard(settingMutex_, std::adopt_lock);
    std::lock_guard<std::recursive_mutex> lockGuard2(serialMutex_, std::adopt_lock);

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
    std::lock_guard<std::recursive_mutex> lockGuard(serialMutex_);
    return serial_->getPort();
}

void SmartSerial::setVidPid(std::string vid, std::string pid) {
    std::lock_guard<std::recursive_mutex> lockGuard(settingMutex_);
    vid_ = std::move(vid);
    pid_ = std::move(pid);

    std::transform(vid_.begin(), vid_.end(), vid_.begin(), tolower);
    std::transform(pid_.begin(), pid_.end(), pid_.begin(), tolower);
}

bool SmartSerial::isOpen() {
    std::lock_guard<std::recursive_mutex> lockGuard(settingMutex_);
    return isOpen_;
}

void SmartSerial::open() {
    autoOpen_ = true;
}

void SmartSerial::close() {
    std::lock(settingMutex_, serialMutex_);
    std::lock_guard<std::recursive_mutex> lockGuard(settingMutex_, std::adopt_lock);
    std::lock_guard<std::recursive_mutex> lockGuard2(serialMutex_, std::adopt_lock);

    autoOpen_ = false;
    if (serial_->isOpen()) {
        serial_->close();
        updateOpenState();
    }
}

void SmartSerial::updateOpenState() {
    std::lock(settingMutex_, serialMutex_);
    std::lock_guard<std::recursive_mutex> lockGuard(settingMutex_, std::adopt_lock);
    std::lock_guard<std::recursive_mutex> lockGuard2(serialMutex_, std::adopt_lock);

    bool isOpen = serial_->isOpen();
    serialMutex_.unlock();
    if (isOpen_ == isOpen) return;
    LOGD("open state: %d", isOpen);
    isOpen_ = isOpen;
    if (onOpenHandle_) {
        onOpenHandle_(isOpen);
    }
}

std::string SmartSerial::guessPortName() {
    std::lock(settingMutex_, serialMutex_);
    std::lock_guard<std::recursive_mutex> lockGuard(settingMutex_, std::adopt_lock);
    std::lock_guard<std::recursive_mutex> lockGuard2(serialMutex_, std::adopt_lock);

    auto portName = serial_->getPort();
    serialMutex_.unlock();

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
