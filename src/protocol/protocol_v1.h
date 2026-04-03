#pragma once

#include <cstdint>
#include <vector>

#include "protocol.h"

namespace mcp {

class ProtocolV1 : public Protocol {
 public:
    void connect(std::shared_ptr<SerialPort> port) override;

 private:
    void sendCommand(uint8_t opcode, uint32_t len);
};

}  // namespace mcp
