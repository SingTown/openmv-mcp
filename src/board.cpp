#include "board.h"

namespace mcp {

// clang-format off
static std::vector<Board> createAllBoards() {
    return {
    {.vid = 0x1209, .pid = 0xABD1, .pidMask = 0xFFFF,
     .boardType = "", .name = "OpenMV Cam", .archString = "",
     .checkLicense = false},
    {.vid = 0x37C5, .pid = 0x1202, .pidMask = 0xFFFF,
     .boardType = "M4", .name = "OpenMV Cam M4", .archString = "OMV2 F4 256 JPEG",
     .checkLicense = false},
    {.vid = 0x37C5, .pid = 0x1203, .pidMask = 0xFFFF,
     .boardType = "M7", .name = "OpenMV Cam M7", .archString = "OMV3 F7 512",
     .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x1204, .pidMask = 0xFFFF,
     .boardType = "H7", .name = "OpenMV Cam H7", .archString = "OMV4 H7 1024",
     .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x124A, .pidMask = 0xFFFF,
     .boardType = "H7", .name = "OpenMV Cam H7 Plus", .archString = "OMV4P H7 32768 SDRAM",
     .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x1205, .pidMask = 0xFFFF,
     .boardType = "H7", .name = "OpenMV Pure Thermal", .archString = "OPENMVPT 65536 SDRAM",
     .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x1060, .pidMask = 0xFFFF,
     .boardType = "IMXRT1060", .name = "OpenMV Cam RT1062", .archString = "OMVRT1060 32768 SDRAM",
     .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x16E3, .pidMask = 0xFFFF,
     .boardType = "AE3", .name = "OpenMV Cam AE3", .archString = "OPENMV AE3",
     .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x1206, .pidMask = 0xFFFF,
     .boardType = "N6", .name = "OpenMV Cam N6", .archString = "OpenMV N6",
     .checkLicense = true},
    {.vid = 0x2341, .pid = 0x005B, .pidMask = 0x00FF,
     .boardType = "H7", .name = "Arduino Portenta", .archString = "PORTENTA H7 8192 SDRAM",
     .checkLicense = false},
    {.vid = 0x2341, .pid = 0x0066, .pidMask = 0x00FF,
     .boardType = "H7", .name = "Arduino Giga", .archString = "GIGA H7 8192 SDRAM",
     .checkLicense = false},
    {.vid = 0x2341, .pid = 0x005F, .pidMask = 0x00FF,
     .boardType = "NICLAV", .name = "Arduino Nicla Vision", .archString = "NICLAV H7 1024",
     .checkLicense = false},
    {.vid = 0x2341, .pid = 0x005A, .pidMask = 0x00FF,
     .boardType = "NANO33", .name = "Arduino Nano33 BLE", .archString = "NANO33 M4",
     .checkLicense = false},
    {.vid = 0x2341, .pid = 0x005E, .pidMask = 0x00FF,
     .boardType = "PICO", .name = "Arduino Nano RP2040 Connect", .archString = "PICO M0",
     .checkLicense = false},
    };
}
// clang-format on

std::vector<Board> allBoards() {
    return createAllBoards();
}

const std::map<uint32_t, std::string> ALL_SENSORS_MAP = {
    {0x26, "OV2640"},          {0x56, "OV5640"},        {0x76, "OV7690"},       {0x77, "OV7725"},
    {0x96, "OV9650"},          {0x1311, "MT9V0X2"},     {0x1312, "MT9V0X2"},    {0x1313, "MT9V0X2"},
    {0x1413, "MT9V0X2-C"},     {0x1324, "MT9V0X4"},     {0x1424, "MT9V0X4-C"},  {0x2481, "MT9M114"},
    {0xAE, "BOSON"},           {0xAE32, "BOSON-320"},   {0xAE64, "BOSON-640"},  {0xAE321, "BOSON-320+"},
    {0xAE641, "BOSON-640+"},   {0x54, "LEPTON"},        {0x5415, "LEPTON-1.5"}, {0x5416, "LEPTON-1.6"},
    {0x5420, "LEPTON-2.0"},    {0x5425, "LEPTON-2.5"},  {0x5430, "LEPTON-3.0"}, {0x5435, "LEPTON-3.5"},
    {0xB0, "HM01B0"},          {0x60, "HM0360"},        {0x21, "GC2145"},       {0x7920, "PAG7920"},
    {0x7936, "PAG7936"},       {0x6100, "PAJ6100"},     {0x5520, "PS5520"},     {0x2020, "FROGEYE2020"},
    {0x30501C01, "GENX320-S"}, {0xB0602003, "GENX320"},
};

}  // namespace mcp
