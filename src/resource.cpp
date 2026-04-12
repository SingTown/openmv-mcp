#include "resource.h"

#include <cmrc/cmrc.hpp>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "miniz.h"
#include "openmv_resource_hash.h"

CMRC_DECLARE(openmv_embed);

namespace mcp {

namespace fs = std::filesystem;

static void extractAll(const void* data, size_t size, const fs::path& outDir) {
    mz_zip_archive zip{};
    if (mz_zip_reader_init_mem(&zip, data, size, 0) == 0) {
        throw std::runtime_error("mz_zip_reader_init_mem failed");
    }

    const mz_uint n = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < n; ++i) {
        mz_zip_archive_file_stat st{};
        if (mz_zip_reader_file_stat(&zip, i, &st) == 0) {
            mz_zip_reader_end(&zip);
            throw std::runtime_error("mz_zip_reader_file_stat failed");
        }

        fs::path target = outDir / st.m_filename;
        if (mz_zip_reader_is_file_a_directory(&zip, i) != 0) {
            fs::create_directories(target);
            continue;
        }
        if (target.has_parent_path()) {
            fs::create_directories(target.parent_path());
        }
        if (mz_zip_reader_extract_to_file(&zip, i, target.string().c_str(), 0) == 0) {
            mz_zip_reader_end(&zip);
            throw std::runtime_error("extract failed: " + std::string(st.m_filename));
        }

#ifndef _WIN32
        const uint32_t unixMode = (st.m_external_attr >> 16) & 0xFFFF;
        if (unixMode != 0) {
            std::error_code ec;
            fs::permissions(target, static_cast<fs::perms>(unixMode & 0777), ec);
        }
#endif
    }

    mz_zip_reader_end(&zip);
}

const fs::path& resourcePath() {
    static const fs::path p = fs::temp_directory_path() / ("openmv-mcp-" OPENMV_RESOURCE_HASH);
    return p;
}

void extractEmbeddedResource() {
    const fs::path& outDir = resourcePath();
    const fs::path marker = outDir / ".complete";

    if (fs::exists(marker)) {
        return;
    }

    if (fs::exists(outDir)) {
        std::error_code ec;
        fs::remove_all(outDir, ec);
    }
    fs::create_directories(outDir);

    auto rcfs = cmrc::openmv_embed::get_filesystem();
    auto file = rcfs.open("embedded_resource.zip");
    const void* data = file.begin();
    const size_t size = static_cast<size_t>(file.end() - file.begin());
    extractAll(data, size, outDir);

    std::FILE* fp = std::fopen(marker.string().c_str(), "wb");
    if (fp == nullptr) {
        throw std::runtime_error("failed to create marker file: " + marker.string());
    }
    std::fclose(fp);
}

}  // namespace mcp
