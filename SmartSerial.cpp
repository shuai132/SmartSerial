#include "SmartSerial.h"
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
        } catch (const std::exception& e) {
            LOGE("open port exception: %s", e.what());
        }
    }

    // 读取阻塞的超时时间ms 也影响重连判断的速度 1s
    serial_->setTimeout(serial::Timeout::simpleTimeout(1000));

    auto thread = new std::thread([this]{
        while(running) {
            try {
                if (not serial_->isOpen()) {
                    LOGD("try open...");
                    serial_->open();
                    LOGD("open state:%d", serial_->isOpen());
                } else {
                    bool hasData = serial_->waitReadable();
                    if (hasData) {
                        size_t validSize = serial_->available();
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
            }
        }
        serial_->close();
    });
    monitorThread_ = std::unique_ptr<std::thread>(thread);
}

SmartSerial::~SmartSerial() {
    running = false;
    monitorThread_->join();
}

void SmartSerial::setOnReadHandle(const OnReadHandle& handle) {
    onReadHandle_ = handle;
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
