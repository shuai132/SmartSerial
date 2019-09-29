# SmartSerial

简化串口使用

基于[wjwwood/serial](https://github.com/wjwwood/serial)的一个Fork: [shuai132/serial](https://github.com/shuai132/serial/tree/cmake)(cmake分支)

优化了cmake和部分代码。

## Requirements

* C++11
* CMake 3.1

## Features

* 数据自动回调
* 异常自动重连

## Usage

* clone repository
```bash
git clone --recursive https://github.com/shuai132/SmartSerial.git
```
* how to use
```cpp

#include "SmartSerial.h"

int main() {
    SmartSerial smartSerial("/dev/ttyUSB0");
    smartSerial.setOnReadHandle([](const uint8_t* data, size_t size) {
        // on read data(other thread)
    });
    smartSerial.write("hello world");
    return 0;
}
```
* example  
[example/smartserial_example.cpp](example/smartserial_example.cpp)

## Hardware

任选一种方式

* 短接Rx与Tx
* arduino
```cpp
void setup() {
    Serial.begin(115200);
}

void loop() {
    if (Serial.available()) {
        Serial.write(Serial.read());
    }
}                                         
```

## Other

如果需要对数据打包和解包，可配合下面的库使用：

[https://github.com/shuai132/PacketProcessor](https://github.com/shuai132/PacketProcessor)
