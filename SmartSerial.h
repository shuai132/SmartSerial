#pragma once

#include <string>
#include <thread>
#include <functional>
#include <atomic>

#include "serial/serial.h"

class SmartSerial {
    using Serial = serial::Serial;
    using OnReadHandle = std::function<void(const uint8_t* data, size_t size)>;
    using OnOpenHandle = std::function<void(bool isOpen)>;

public:
    explicit SmartSerial(const std::string& port = "", uint32_t baudrate = 115200, OnOpenHandle handle = nullptr);

    ~SmartSerial();

    // noncopyable
    SmartSerial(const SmartSerial&) = delete;
    void operator=(const SmartSerial&) = delete;

public:
    /**
     * 设置接收的handle 注意回调将发生在读取线程
     * @param handle
     */
    void setOnReadHandle(const OnReadHandle& handle);

    /**
     * 设置打开/关闭handle 注意回调很可能发生在读取线程
     * @param handle
     */
    void setOnOpenHandle(const OnOpenHandle& handle);

    /**
     * 写入数据 直到写入所有再返回
     * @param data
     * @param size
     * @return 写入成功/失败
     */
    bool write(const uint8_t* data, size_t size);

    bool write(const std::string& data);

    bool write(const std::vector<uint8_t>& data);

    // 获取serial 其大部分方法都是线程安全的
    Serial* getSerial();

    void setPortName(std::string portName);

    // Hex string, eg: setVidPid("1234", "abCD")
    void setVidPid(std::string vid, std::string pid);

private:
    void updateOpenState();

    std::string guessPortName();

private:
    std::unique_ptr<Serial> serial_;
    std::unique_ptr<std::thread> monitorThread_;
    const uint32_t CHECK_INTERVAL_SEC = 2;

    OnReadHandle onReadHandle_;
    OnOpenHandle onOpenHandle_;
    static const size_t BUFFER_SIZE = 1024;
    uint8_t buffer_[BUFFER_SIZE]{};

    std::atomic_bool running_{true};

    std::string portName_;
    std::string VID_;
    std::string PID_;
    bool isOpen_ = false;
};
