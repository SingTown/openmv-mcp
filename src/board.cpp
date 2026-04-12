#include "board.h"

#include <filesystem>

#include "resource.h"

namespace mcp {

namespace fs = std::filesystem;

// clang-format off
static std::vector<Board> createAllBoards() {
    const auto& RES = resourcePath();

#if defined(_WIN32)
    const auto DFU_UTIL = (RES / "dfu-util/windows/dfu-util.exe").string();
    const auto SDPHOST  = (RES / "sdphost/win/sdphost.exe").string();
    const auto BLHOST   = (RES / "blhost/win/blhost.exe").string();
#elif defined(__APPLE__)
    const auto DFU_UTIL = (RES / "dfu-util/osx/dfu-util").string();
    const auto SDPHOST  = (RES / "sdphost/mac/sdphost").string();
    const auto BLHOST   = (RES / "blhost/mac/blhost").string();
#elif defined(__aarch64__)
    const auto DFU_UTIL = (RES / "dfu-util/aarch64/dfu-util").string();
    const std::string SDPHOST;  // TODO: no aarch64 build available
    const std::string BLHOST;   // TODO: no aarch64 build available
#else
    const auto DFU_UTIL = (RES / "dfu-util/linux64/dfu-util").string();
    const auto SDPHOST  = (RES / "sdphost/linux/amd64/sdphost").string();
    const auto BLHOST   = (RES / "blhost/linux/amd64/blhost").string();
#endif

#if defined(_WIN32)
    const auto STM32_PROG = (RES / "stcubeprogrammer/windows/STM32_Programmer_CLI.exe").string();
#elif defined(__APPLE__)
    const auto STM32_PROG = (RES / "stcubeprogrammer/mac/bin/STM32_Programmer_CLI").string();
#else
    const auto STM32_PROG = (RES / "stcubeprogrammer/linux64/bin/STM32_Programmer_CLI").string();
#endif

    auto dfuListMatch = [&](const std::string& vidPid) -> std::string {
#if defined(_WIN32)
        return DFU_UTIL + " -l | findstr /I /C:\"[" + vidPid + "]\" >nul";
#else
        return DFU_UTIL + " -l | grep -qi \"\\[" + vidPid + "\\]\"";
#endif
    };

    const std::string DFU_FACTORY_MSG =
        "Disconnect your OpenMV Cam from your computer, add a jumper wire between the BOOT and RST pins, "
        "and then reconnect your OpenMV Cam to your computer.";
    const std::string DFU_BOOTLOADER_MSG =
        "Disconnect your OpenMV Cam from your computer, remove the jumper wire between the BOOT and RST pins, "
        "and then reconnect your OpenMV Cam to your computer.";
    const std::string SBL_FACTORY_MSG =
        "Disconnect your OpenMV Cam from your computer, add a jumper wire between the SBL and 3.3V pins, "
        "and then reconnect your OpenMV Cam to your computer.";
    const std::string SBL_BOOTLOADER_MSG =
        "Disconnect your OpenMV Cam from your computer, remove the jumper wire between the SBL and 3.3V pins, "
        "and then reconnect your OpenMV Cam to your computer.";
    const std::string NANO33_MSG =
        "Please update the bootloader to the latest version and install the SoftDevice to flash the OpenMV firmware.";
    const std::string RP2040_MSG = "Please short REC to GND and reset your board.";

    return {
    {.vid = 0x1209, .pid = 0xABD1, .pidMask = 0xFFFF,
     .boardType = "", .name = "OpenMV Cam", .archString = "",
     .factoryDetectCommand = dfuListMatch("0483:df11"), .factoryDetectMessage = DFU_FACTORY_MSG,
     .bootloaderDetectCommand = "", .bootloaderDetectMessage = "",
     .bootloaderCommands = {},
     .firmwareCommands = {},
     .firmwareDir = "", .checkLicense = false},
    {.vid = 0x37C5, .pid = 0x1202, .pidMask = 0xFFFF,
     .boardType = "M4", .name = "OpenMV Cam M4", .archString = "OMV2 F4 256 JPEG",
     .factoryDetectCommand = dfuListMatch("0483:df11"), .factoryDetectMessage = DFU_FACTORY_MSG,
     .bootloaderDetectCommand = dfuListMatch("37C5:9202"), .bootloaderDetectMessage = DFU_BOOTLOADER_MSG,
     .bootloaderCommands = {DFU_UTIL + " -w -d 37C5:9202 -a 0 -s 0x08000000 -D bootloader.bin"},
     .firmwareCommands = {DFU_UTIL + " -w -d 37C5:9202 -a 2 -D firmware.bin",
                          DFU_UTIL + " -d 37C5:9202 -a 2 -e"},
     .firmwareDir = "firmware/OPENMV2/", .checkLicense = false},
    {.vid = 0x37C5, .pid = 0x1203, .pidMask = 0xFFFF,
     .boardType = "M7", .name = "OpenMV Cam M7", .archString = "OMV3 F7 512",
     .factoryDetectCommand = dfuListMatch("0483:df11"), .factoryDetectMessage = DFU_FACTORY_MSG,
     .bootloaderDetectCommand = dfuListMatch("37C5:9203"), .bootloaderDetectMessage = DFU_BOOTLOADER_MSG,
     .bootloaderCommands = {DFU_UTIL + " -w -d 37C5:9203 -a 0 -s 0x08000000 -D bootloader.bin"},
     .firmwareCommands = {DFU_UTIL + " -w -d 37C5:9203 -a 2 -D firmware.bin",
                          DFU_UTIL + " -d 37C5:9203 -a 2 -e"},
     .firmwareDir = "firmware/OPENMV3/", .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x1204, .pidMask = 0xFFFF,
     .boardType = "H7", .name = "OpenMV Cam H7", .archString = "OMV4 H7 1024",
     .factoryDetectCommand = dfuListMatch("0483:df11"), .factoryDetectMessage = DFU_FACTORY_MSG,
     .bootloaderDetectCommand = dfuListMatch("37C5:9204"), .bootloaderDetectMessage = DFU_BOOTLOADER_MSG,
     .bootloaderCommands = {DFU_UTIL + " -w -d 37C5:9204 -a 0 -s 0x08000000 -D bootloader.bin"},
     .firmwareCommands = {DFU_UTIL + " -w -d 37C5:9204 -a 2 -D firmware.bin",
                          DFU_UTIL + " -d 37C5:9204 -a 2 -e"},
     .firmwareDir = "firmware/OPENMV4/", .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x124A, .pidMask = 0xFFFF,
     .boardType = "H7", .name = "OpenMV Cam H7 Plus", .archString = "OMV4P H7 32768 SDRAM",
     .factoryDetectCommand = dfuListMatch("0483:df11"), .factoryDetectMessage = DFU_FACTORY_MSG,
     .bootloaderDetectCommand = dfuListMatch("37C5:924A"), .bootloaderDetectMessage = DFU_BOOTLOADER_MSG,
     .bootloaderCommands = {DFU_UTIL + " -w -d 37C5:924A -a 0 -s 0x08000000 -D bootloader.bin"},
     .firmwareCommands = {DFU_UTIL + " -w -d 37C5:924A -a 2 -D firmware.bin",
                          DFU_UTIL + " -d 37C5:924A -a 2 -e"},
     .firmwareDir = "firmware/OPENMV4P/", .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x1205, .pidMask = 0xFFFF,
     .boardType = "H7", .name = "OpenMV Pure Thermal", .archString = "OPENMVPT 65536 SDRAM",
     .factoryDetectCommand = dfuListMatch("0483:df11"), .factoryDetectMessage = DFU_FACTORY_MSG,
     .bootloaderDetectCommand = dfuListMatch("37C5:9205"), .bootloaderDetectMessage = DFU_BOOTLOADER_MSG,
     .bootloaderCommands = {DFU_UTIL + " -w -d 37C5:9205 -a 0 -s 0x08000000 -D bootloader.bin"},
     .firmwareCommands = {DFU_UTIL + " -w -d 37C5:9205 -a 2 -D firmware.bin",
                          DFU_UTIL + " -d 37C5:9205 -a 2 -e"},
     .firmwareDir = "firmware/OPENMVPT/", .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x1060, .pidMask = 0xFFFF,
     .boardType = "IMXRT1060", .name = "OpenMV Cam RT1062", .archString = "OMVRT1060 32768 SDRAM",
     .factoryDetectCommand = SDPHOST + " -u 0x1FC9,0x0135 -- error-status", .factoryDetectMessage = SBL_FACTORY_MSG,
     .bootloaderDetectCommand = BLHOST + " -u 0x15A2,0x0073 -- get-property 1", .bootloaderDetectMessage = SBL_BOOTLOADER_MSG,
     .bootloaderCommands = {SDPHOST + " -u 0x1FC9,0x0135 -- write-file 0x20001C00 sdphost_flash_loader.bin",
                            SDPHOST + " -u 0x1FC9,0x0135 -- jump-address 0x20001C00",
                            BLHOST + " -u 0x15A2,0x0073 -- get-property 1",
                            BLHOST + " -u 0x15A2,0x0073 -- fill-memory 0x2000 4 0xC0000008 word",
                            BLHOST + " -u 0x15A2,0x0073 -- configure-memory 9 0x2000",
                            BLHOST + " -u 0x15A2,0x0073 -t 120000 -- flash-erase-region 0x60000000 0x1000",
                            BLHOST + " -u 0x15A2,0x0073 -t 120000 -- flash-erase-region 0x60001000 0x3F000",
                            BLHOST + " -u 0x15A2,0x0073 -- write-memory 0x60001000 blhost_flash_loader.bin"},
     .firmwareCommands = {SDPHOST + " -u 0x1FC9,0x0135 -- write-file 0x20001C00 sdphost_flash_loader.bin",
                          SDPHOST + " -u 0x1FC9,0x0135 -- jump-address 0x20001C00",
                          BLHOST + " -u 0x15A2,0x0073 -- get-property 1",
                          BLHOST + " -u 0x15A2,0x0073 -- fill-memory 0x2000 4 0xC0000008 word",
                          BLHOST + " -u 0x15A2,0x0073 -- configure-memory 9 0x2000",
                          BLHOST + " -u 0x15A2,0x0073 -t 120000 -- flash-erase-region 0x60040000 0x3C0000",
                          BLHOST + " -u 0x15A2,0x0073 -- write-memory 0x60040000 firmware.bin",
                          BLHOST + " -u 0x15A2,0x0073 -- reset"},
     .firmwareDir = "firmware/OPENMV_RT1060/", .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x16E3, .pidMask = 0xFFFF,
     .boardType = "AE3", .name = "OpenMV Cam AE3", .archString = "OPENMV AE3",
     .factoryDetectCommand = "", .factoryDetectMessage = "",
     .bootloaderDetectCommand = dfuListMatch("37C5:96E3"), .bootloaderDetectMessage = DFU_BOOTLOADER_MSG,
     .bootloaderCommands = {DFU_UTIL + " -w -d 37C5:96E3 -a 0 -s 0x08000000 -D bootloader.bin"},
     .firmwareCommands = {DFU_UTIL + " -w -d 37C5:96E3 -a 1 -D firmware_M55_HP.bin",
                          DFU_UTIL + " -w -d 37C5:96E3 -a 2 -D firmware_M55_HE.bin",
                          DFU_UTIL + " -d 37C5:96E3 -a 2 -e"},
     .firmwareDir = "firmware/OPENMV_AE3/", .checkLicense = true},
    {.vid = 0x37C5, .pid = 0x1206, .pidMask = 0xFFFF,
     .boardType = "N6", .name = "OpenMV Cam N6", .archString = "OpenMV N6",
     .factoryDetectCommand = STM32_PROG + " -c port=USB1", .factoryDetectMessage = DFU_FACTORY_MSG,
     .bootloaderDetectCommand = dfuListMatch("37C5:9206"), .bootloaderDetectMessage = DFU_BOOTLOADER_MSG,
     .bootloaderCommands = {STM32_PROG + " -c port=USB1 -d FlashLayout.tsv"},
     .firmwareCommands = {DFU_UTIL + " -w -d 37C5:9206 -a 1 -D firmware.bin",
                          DFU_UTIL + " -d 37C5:9206 -a 1 -e"},
     .firmwareDir = "firmware/OPENMV_N6/", .checkLicense = true},
    {.vid = 0x2341, .pid = 0x005B, .pidMask = 0x00FF,
     .boardType = "H7", .name = "Arduino Portenta", .archString = "PORTENTA H7 8192 SDRAM",
     .factoryDetectCommand = "", .factoryDetectMessage = "",
     .bootloaderDetectCommand = dfuListMatch("2341:035b"), .bootloaderDetectMessage = DFU_BOOTLOADER_MSG,
     .bootloaderCommands = {DFU_UTIL + " -w -d 2341:035b -a 0 -s 0x08000000 -D bootloader.bin"},
     .firmwareCommands = {DFU_UTIL + " -w -d 2341:035b -a 0 -s 0x08040000 -D firmware.bin",
                          DFU_UTIL + " -d 2341:035b -a 0 -e"},
     .firmwareDir = "firmware/ARDUINO_PORTENTA_H7/", .checkLicense = false},
    {.vid = 0x2341, .pid = 0x0066, .pidMask = 0x00FF,
     .boardType = "H7", .name = "Arduino Giga", .archString = "GIGA H7 8192 SDRAM",
     .factoryDetectCommand = "", .factoryDetectMessage = "",
     .bootloaderDetectCommand = dfuListMatch("2341:0366"), .bootloaderDetectMessage = DFU_BOOTLOADER_MSG,
     .bootloaderCommands = {DFU_UTIL + " -w -d 2341:0366 -a 0 -s 0x08000000 -D bootloader.bin"},
     .firmwareCommands = {DFU_UTIL + " -w -d 2341:0366 -a 0 -s 0x08040000 -D firmware.bin",
                          DFU_UTIL + " -d 2341:0366 -a 0 -e"},
     .firmwareDir = "firmware/ARDUINO_GIGA/", .checkLicense = false},
    {.vid = 0x2341, .pid = 0x005F, .pidMask = 0x00FF,
     .boardType = "NICLAV", .name = "Arduino Nicla Vision", .archString = "NICLAV H7 1024",
     .factoryDetectCommand = "", .factoryDetectMessage = "",
     .bootloaderDetectCommand = dfuListMatch("2341:035f"), .bootloaderDetectMessage = DFU_BOOTLOADER_MSG,
     .bootloaderCommands = {DFU_UTIL + " -w -d 2341:035f -a 0 -s 0x08000000 -D bootloader.bin"},
     .firmwareCommands = {DFU_UTIL + " -w -d 2341:035f -a 0 -s 0x08040000 -D firmware.bin",
                          DFU_UTIL + " -d 2341:035f -a 0 -e"},
     .firmwareDir = "firmware/ARDUINO_NICLA_VISION/", .checkLicense = false},
    {.vid = 0x2341, .pid = 0x005A, .pidMask = 0x00FF,
     .boardType = "NANO33", .name = "Arduino Nano33 BLE", .archString = "NANO33 M4",
     .factoryDetectCommand = "", .factoryDetectMessage = NANO33_MSG,
     .bootloaderDetectCommand = "", .bootloaderDetectMessage = "",
     .bootloaderCommands = {},
     .firmwareCommands = {},
     .firmwareDir = "", .checkLicense = false},
    {.vid = 0x2341, .pid = 0x005E, .pidMask = 0x00FF,
     .boardType = "PICO", .name = "Arduino Nano RP2040 Connect", .archString = "PICO M0",
     .factoryDetectCommand = "", .factoryDetectMessage = RP2040_MSG,
     .bootloaderDetectCommand = "picotool info", .bootloaderDetectMessage = "",
     .bootloaderCommands = {},
     .firmwareCommands = {},
     .firmwareDir = "", .checkLicense = false},
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
