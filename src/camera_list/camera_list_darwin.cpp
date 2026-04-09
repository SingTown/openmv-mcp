#include "camera_list.h"

#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBLib.h>

#include <string>

#include "board.h"

namespace mcp {

static std::string cfstring_to_string(CFStringRef cf) {
    if (!cf) {
        return {};
    }
    char buf[256];
    if (CFStringGetCString(cf, buf, sizeof(buf), kCFStringEncodingUTF8)) {
        return buf;
    }
    return {};
}

static std::string cf_property_string(io_object_t obj, CFStringRef key) {
    CFTypeRef ref = IORegistryEntryCreateCFProperty(obj, key, kCFAllocatorDefault, 0);
    if (!ref) {
        return {};
    }
    std::string result;
    if (CFGetTypeID(ref) == CFStringGetTypeID()) {
        result = cfstring_to_string(static_cast<CFStringRef>(ref));
    }
    CFRelease(ref);
    return result;
}

static int cf_property_int(io_object_t obj, CFStringRef key) {
    CFTypeRef ref = IORegistryEntryCreateCFProperty(obj, key, kCFAllocatorDefault, 0);
    if (!ref) {
        return -1;
    }
    int result = -1;
    if (CFGetTypeID(ref) == CFNumberGetTypeID()) {
        CFNumberGetValue(static_cast<CFNumberRef>(ref), kCFNumberIntType, &result);
    }
    CFRelease(ref);
    return result;
}

static bool find_usb_parent(io_object_t child, uint16_t& vid, uint16_t& pid) {
    io_object_t parent;
    kern_return_t kr = IORegistryEntryGetParentEntry(child, kIOServicePlane, &parent);
    if (kr != KERN_SUCCESS) {
        return false;
    }

    for (int depth = 0; depth < 10; depth++) {
        int v = cf_property_int(parent, CFSTR("idVendor"));
        int p = cf_property_int(parent, CFSTR("idProduct"));
        if (v >= 0 && p >= 0) {
            vid = static_cast<uint16_t>(v);
            pid = static_cast<uint16_t>(p);
            IOObjectRelease(parent);
            return true;
        }

        io_object_t next;
        kr = IORegistryEntryGetParentEntry(parent, kIOServicePlane, &next);
        IOObjectRelease(parent);
        if (kr != KERN_SUCCESS) {
            return false;
        }
        parent = next;
    }

    IOObjectRelease(parent);
    return false;
}

std::vector<PortInfo> listCameras() {
    std::vector<PortInfo> result;

    CFMutableDictionaryRef match = IOServiceMatching(kIOSerialBSDServiceValue);
    if (!match) {
        return result;
    }

    CFDictionarySetValue(match, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDAllTypes));

    io_iterator_t iter;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, match, &iter);
    if (kr != KERN_SUCCESS) {
        return result;
    }

    io_object_t service;
    while ((service = IOIteratorNext(iter)) != 0) {
        std::string path = cf_property_string(service, CFSTR(kIOCalloutDeviceKey));
        if (path.empty()) {
            IOObjectRelease(service);
            continue;
        }

        uint16_t vid = 0;
        uint16_t pid = 0;
        find_usb_parent(service, vid, pid);
        IOObjectRelease(service);

        try {
            const auto& b = findBoardByVidPid(vid, pid);
            result.push_back({std::move(path), b.name});
        } catch (const std::runtime_error&) {
        }
    }

    IOObjectRelease(iter);
    return result;
}

}  // namespace mcp

#endif  // __APPLE__
