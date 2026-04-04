#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "protocol.h"

namespace mcp {

class ProtocolV1 : public Protocol {
 public:
    ~ProtocolV1() override { disconnect(); }
    void connect(std::shared_ptr<SerialPort> port) override;
    void execScript(const std::string& script) override;
    void stopScript() override;

 private:
    void sendCommand(uint8_t opcode, uint32_t len);
    void readFwVersion();
    void readArchStr();
    void readSensorId();
    void poll() override;
    static void delay(int ms);
};

}  // namespace mcp
