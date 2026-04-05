#include "protocol/protocol_detect.h"

#include <cstdint>
#include <stdexcept>

#include "utils/crc.h"

namespace mcp {

int detectProtocol(SerialPort& port) {
    // V1 probe: CMD + SENSOR_ID opcode
    uint8_t v1_cmd[6] = {0x30, 0x87, 0x04, 0x00, 0x00, 0x00};
    port.write_bytes({v1_cmd, v1_cmd + 6});

    // V2 probe: PROTO_SYNC packet with raw bytes (seq=0, chan=0, flags=0, opcode=0, len=4)
    uint8_t v2_hdr[8] = {0xAA, 0xD5, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00};
    uint8_t v2_payload[4] = {0, 0, 0, 0};
    port.write_bytes({v2_hdr, v2_hdr + 8});
    port.write_le16(crc16(v2_hdr, 8));
    port.write_bytes({v2_payload, v2_payload + 4});
    port.write_le32(crc32(v2_payload, 4));

    port.send();

    try {
        auto response = port.read_bytes(4);
        uint16_t first_word = static_cast<uint16_t>(response[0] | (response[1] << 8));
        return (first_word != 0xD5AA) ? 1 : 2;
    } catch (const std::runtime_error&) {
        throw std::runtime_error("Protocol detection failed: no response from device");
    }
}

}  // namespace mcp
