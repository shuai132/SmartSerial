#pragma once

#include <string>
#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>

#include "serial/serial.h"

class SmartSerial {
    using Serial = serial::Serial;
    using OnReadHandle = std::function<void(const uint8_t* data, size_t size)>;
    using OnOpenHandle = std::function<void(bool isOpen)>;

public:
    explicit SmartSerial(const std::string& port = "", uint32_t baudrate = 115200, OnOpenHandle handle = nullptr);
    explicit SmartSerial(const std::string& vid, const std::string& pid, OnOpenHandle handle = nullptr);

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

    void setPortName(const std::string& portName);

    std::string getPortName();

    // Hex string, eg: setVidPid("1234", "abCD")
    void setVidPid(std::string vid, std::string pid);

    bool isOpen();

    // see OpenHandle
    void open();

    // see OpenHandle
    void close();

private:
    void updateOpenState();

    std::string guessPortName();

private:
    std::unique_ptr<Serial> serial_;
    std::unique_ptr<std::thread> monitorThread_;
    const uint32_t CHECK_INTERVAL_SEC = 2;

    std::recursive_mutex serialMutex_;
    std::recursive_mutex settingMutex_;

    OnReadHandle onReadHandle_;
    OnOpenHandle onOpenHandle_;
    static const size_t BUFFER_SIZE = 1024;
    uint8_t buffer_[BUFFER_SIZE]{};

    std::atomic_bool running_{true};

    std::string vid_;
    std::string pid_;
    std::atomic_bool isOpen_ {false};

    std::atomic_bool autoOpen_ {true};

    std::chrono::steady_clock::time_point lastReadTime_;
};
