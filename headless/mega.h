#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace Eden::Mega {

inline constexpr const char* kApiUrl = "https://g.api.mega.co.nz/cs?id=0";

struct Link {
    std::string handle;
    std::array<std::uint8_t, 32> key{};
};

bool ParseLink(std::string_view text, Link& link);
std::string Format(const Link& link);
std::string FileRequest(const Link& link);
bool ReadFileAnswer(std::string_view text, std::string& url, std::uint64_t& size, std::string& error);
bool DecryptFile(const std::filesystem::path& file, const Link& link, std::uint64_t size, std::string& error);
bool EncryptFile(const std::filesystem::path& file, const std::array<std::uint8_t, 16>& file_key,
                 const std::array<std::uint8_t, 8>& nonce, Link& link);

}
