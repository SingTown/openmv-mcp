#pragma once

#include "serial_port/serial_port.h"

namespace mcp {

int detectProtocol(SerialPort& port);

}  // namespace mcp
