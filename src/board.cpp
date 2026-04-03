#include "board.h"

namespace mcp {

const std::vector<Board> ALL_BOARDS = {
    {0x1209, 0xABD1, 0xFFFF, "", "OpenMV Cam", ""},
    {0x37C5, 0x1202, 0xFFFF, "M4", "OpenMV Cam M4", "OMV2 F4 256 JPEG"},
    {0x37C5, 0x1203, 0xFFFF, "M7", "OpenMV Cam M7", "OMV3 F7 512"},
    {0x37C5, 0x1204, 0xFFFF, "H7", "OpenMV Cam H7", "OMV4 H7 1024"},
    {0x37C5, 0x124A, 0xFFFF, "H7", "OpenMV Cam H7 Plus", "OMV4P H7 32768 SDRAM"},
    {0x37C5, 0x1205, 0xFFFF, "H7", "OpenMV Pure Thermal", "OPENMVPT 65536 SDRAM"},
    {0x37C5, 0x1060, 0xFFFF, "IMXRT1060", "OpenMV Cam RT1062", "OMVRT1060 32768 SDRAM"},
    {0x37C5, 0x16E3, 0xFFFF, "AE3", "OpenMV AE3", "OPENMV AE3"},
    {0x37C5, 0x1206, 0xFFFF, "N6", "OpenMV N6", "OpenMV N6"},
    {0x2341, 0x005B, 0x00FF, "H7", "Arduino Portenta", "PORTENTA H7 8192 SDRAM"},
    {0x2341, 0x0066, 0x00FF, "H7", "Arduino Giga", "GIGA H7 8192 SDRAM"},
    {0x2341, 0x005F, 0x00FF, "NICLAV", "Arduino Nicla Vision", "NICLAV H7 1024"},
    {0x2341, 0x005A, 0x00FF, "NANO33", "Arduino Nano33 BLE", "NANO33 M4"},
    {0x2341, 0x005E, 0x00FF, "PICO", "Arduino Nano RP2040 Connect", "PICO M0"},
};

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
