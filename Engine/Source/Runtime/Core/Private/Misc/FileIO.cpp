#include "Misc/FileIO.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <system_error>

namespace {

[[nodiscard]] std::filesystem::path MakeTempSibling(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    const auto stem = path.filename().string();
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::mt19937 rng{static_cast<std::mt19937::result_type>(now)};
    const std::uint32_t roll = rng();
    return parent / (stem + ".tmp." + std::to_string(now) + "." + std::to_string(roll));
}

} // namespace

bool WriteFileAtomic(const std::filesystem::path& path, const void* data, std::size_t size) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            std::cerr << "FileIO: cannot create directory for " << path.string() << ": "
                      << ec.message() << '\n';
            return false;
        }
    }

    const std::filesystem::path temp = MakeTempSibling(path);
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "FileIO: cannot open temp for write: " << temp.string() << '\n';
            return false;
        }
        if (size > 0 && data != nullptr) {
            out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        }
        out.flush();
        if (!out) {
            std::cerr << "FileIO: write failed: " << temp.string() << '\n';
            std::filesystem::remove(temp, ec);
            return false;
        }
    }

    std::filesystem::rename(temp, path, ec);
    if (ec) {
        // Windows: replace existing target when rename-over fails.
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(temp, path, ec);
        if (ec) {
            std::cerr << "FileIO: rename failed " << temp.string() << " -> " << path.string()
                      << ": " << ec.message() << '\n';
            std::filesystem::remove(temp, ec);
            return false;
        }
    }
    return true;
}

bool WriteFileAtomic(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    return WriteFileAtomic(path, bytes.data(), bytes.size());
}

bool WriteTextFileAtomic(const std::filesystem::path& path, std::string_view text) {
    return WriteFileAtomic(path, text.data(), text.size());
}

