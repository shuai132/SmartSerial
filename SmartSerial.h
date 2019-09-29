#pragma once

#include <string>
#include <thread>
#include <functional>
#include <atomic>

#include "serial/serial.h"

class SmartSerial {
    using Serial = serial::Serial;
    using OnReadHandle = std::function<void(const uint8_t* data, size_t size)>;

public:
    explicit SmartSerial(const std::string& port = "", uint32_t baudrate = 115200);

    ~SmartSerial();

public:
    /**
     * 设置接收的handle 注意回调将发生在其他线程
     * @param handle
     */
    void setOnReadHandle(const OnReadHandle& handle);

    bool write(const uint8_t* data, size_t size);

    bool write(const std::string& data);

    bool write(const std::vector<uint8_t>& data);

    Serial* getSerial();

private:
    std::unique_ptr<Serial> serial_;
    std::unique_ptr<std::thread> monitorThread_;
    const uint32_t CHECK_INTERVAL_SEC = 2;

    OnReadHandle onReadHandle_;
    static const size_t BUFFER_SIZE = 1024;
    uint8_t buffer_[BUFFER_SIZE];

    std::atomic_bool running{true};
};