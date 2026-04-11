#include "camera_list.h"

#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBLib.h>
#include <sys/mount.h>

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

static io_object_t find_usb_device_ancestor(io_object_t service) {
    io_object_t current = service;
    IOObjectRetain(current);
    for (int depth = 0; depth < 10; depth++) {
        io_name_t className;
        if (IOObjectGetClass(current, className) == KERN_SUCCESS && std::string(className) == "IOUSBHostDevice") {
            return current;
        }
        io_object_t parent = 0;
        if (IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent) != KERN_SUCCESS) {
            IOObjectRelease(current);
            return 0;
        }
        IOObjectRelease(current);
        current = parent;
    }
    IOObjectRelease(current);
    return 0;
}

// Recursively find an IOMedia BSD Name under the given entry.
// Prefers partition (Whole=false) over whole-disk; returns empty string if nothing found.
static std::string find_bsd_name(io_object_t entry, int depth = 0) {
    if (depth > 20) {
        return {};
    }
    io_iterator_t iter = 0;
    if (IORegistryEntryGetChildIterator(entry, kIOServicePlane, &iter) != KERN_SUCCESS) {
        return {};
    }
    std::string fallback;
    io_object_t child = 0;
    while ((child = IOIteratorNext(iter)) != 0) {
        io_name_t cls;
        if (IOObjectGetClass(child, cls) == KERN_SUCCESS && std::string(cls) == "IOMedia") {
            CFTypeRef bsdRef = IORegistryEntryCreateCFProperty(child, CFSTR("BSD Name"), kCFAllocatorDefault, 0);
            if (bsdRef) {
                char buf[64];
                if (CFStringGetCString(static_cast<CFStringRef>(bsdRef), buf, sizeof(buf), kCFStringEncodingUTF8)) {
                    CFTypeRef wholeRef = IORegistryEntryCreateCFProperty(child, CFSTR("Whole"), kCFAllocatorDefault, 0);
                    bool whole = (wholeRef != nullptr) && CFBooleanGetValue(static_cast<CFBooleanRef>(wholeRef));
                    if (wholeRef) {
                        CFRelease(wholeRef);
                    }
                    if (!whole) {
                        CFRelease(bsdRef);
                        IOObjectRelease(child);
                        IOObjectRelease(iter);
                        return buf;
                    }
                    if (fallback.empty()) {
                        fallback = buf;
                    }
                }
                CFRelease(bsdRef);
            }
        }
        std::string found = find_bsd_name(child, depth + 1);
        IOObjectRelease(child);
        if (!found.empty()) {
            IOObjectRelease(iter);
            return found;
        }
    }
    IOObjectRelease(iter);
    return fallback;
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

static std::string drive_path_from_service(io_object_t serialService) {
    io_object_t usbDevice = find_usb_device_ancestor(serialService);
    if (!usbDevice) return {};

    std::string bsdName = find_bsd_name(usbDevice);
    IOObjectRelease(usbDevice);
    if (bsdName.empty()) return {};

    std::string devPath = "/dev/" + bsdName;
    struct statfs* mounts = nullptr;
    int count = getmntinfo(&mounts, MNT_NOWAIT);
    for (int i = 0; i < count; i++) {
        if (devPath == mounts[i].f_mntfromname) {
            return mounts[i].f_mntonname;
        }
    }
    return {};
}

std::filesystem::path findDrivePath(const std::string& serialPath) {
    CFMutableDictionaryRef match = IOServiceMatching(kIOSerialBSDServiceValue);
    if (!match) return {};

    io_iterator_t iter;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, match, &iter);
    if (kr != KERN_SUCCESS) return {};

    io_object_t service;
    std::string result;
    while ((service = IOIteratorNext(iter))) {
        std::string path = cf_property_string(service, CFSTR(kIOCalloutDeviceKey));
        if (path == serialPath) {
            result = drive_path_from_service(service);
            IOObjectRelease(service);
            break;
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iter);
    return result;
}

}  // namespace mcp

#endif  // __APPLE__
