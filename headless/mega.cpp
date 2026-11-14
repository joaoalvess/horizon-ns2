#include "mega.h"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <algorithm>
#include <fcntl.h>
#include <memory>
#include <unistd.h>
#include <vector>

namespace Eden::Mega {
namespace {

using Json = nlohmann::json;
using Block = std::array<std::uint8_t, 16>;
using Context = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

constexpr std::string_view kPrefix = "https://mega.nz/file/";
constexpr std::string_view kStorage = ".userstorage.mega.co.nz";
constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
constexpr std::size_t kHandleSize = 8;
constexpr std::size_t kKeyText = 43;
constexpr std::size_t kSegment = 0x20000;
constexpr std::size_t kLargestChunk = 8 * kSegment;

struct Keys {
    Block key{};
    Block counter{};
    Block mac_iv{};
};

int Digit(char c) {
    const auto found = kAlphabet.find(c);
    return found == kAlphabet.npos ? -1 : static_cast<int>(found);
}

bool Decode(std::string_view text, std::uint8_t* out, std::size_t size) {
    std::uint32_t buffer = 0;
    unsigned bits = 0;
    std::size_t filled = 0;
    for (const char c : text) {
        const int value = Digit(c);
        if (value < 0) return false;
        buffer = (buffer << 6) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (filled == size) return false;
            out[filled++] = static_cast<std::uint8_t>(buffer >> bits);
        }
    }
    return filled == size;
}

std::string Encode(const std::uint8_t* data, std::size_t size) {
    std::string out;
    std::uint32_t buffer = 0;
    unsigned bits = 0;
    for (std::size_t i = 0; i < size; ++i) {
        buffer = (buffer << 8) | data[i];
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            out += kAlphabet[(buffer >> bits) & 63];
        }
    }
    if (bits > 0) out += kAlphabet[(buffer << (6 - bits)) & 63];
    return out;
}

Keys MakeKeys(const Block& key, const std::uint8_t* nonce) {
    Keys keys;
    keys.key = key;
    std::copy_n(nonce, 8, keys.counter.begin());
    std::copy_n(nonce, 8, keys.mac_iv.begin());
    std::copy_n(nonce, 8, keys.mac_iv.begin() + 8);
    return keys;
}

Keys LinkKeys(const Link& link) {
    Block key{};
    for (std::size_t i = 0; i < key.size(); ++i) key[i] = link.key[i] ^ link.key[i + 16];
    return MakeKeys(key, link.key.data() + 16);
}

std::array<std::uint8_t, 8> Condense(const Block& mac) {
    std::array<std::uint8_t, 8> out{};
    for (std::size_t i = 0; i < 4; ++i) {
        out[i] = mac[i] ^ mac[i + 4];
        out[i + 4] = mac[i + 8] ^ mac[i + 12];
    }
    return out;
}

bool Start(const Context& context, const EVP_CIPHER* cipher, const Block& key, const Block& iv) {
    return context && EVP_EncryptInit_ex(context.get(), cipher, nullptr, key.data(), iv.data()) == 1 &&
           EVP_CIPHER_CTX_set_padding(context.get(), 0) == 1;
}

bool Apply(const Context& context, const std::uint8_t* in, std::uint8_t* out, std::size_t size) {
    int written = 0;
    return EVP_EncryptUpdate(context.get(), out, &written, in, static_cast<int>(size)) == 1 &&
           static_cast<std::size_t>(written) == size;
}

bool ReadAll(int descriptor, std::uint8_t* data, std::size_t size, std::uint64_t offset) {
    while (size > 0) {
        const ssize_t got = pread(descriptor, data, size, static_cast<off_t>(offset));
        if (got <= 0) return false;
        data += got;
        size -= static_cast<std::size_t>(got);
        offset += static_cast<std::uint64_t>(got);
    }
    return true;
}

bool WriteAll(int descriptor, const std::uint8_t* data, std::size_t size, std::uint64_t offset) {
    while (size > 0) {
        const ssize_t put = pwrite(descriptor, data, size, static_cast<off_t>(offset));
        if (put <= 0) return false;
        data += put;
        size -= static_cast<std::size_t>(put);
        offset += static_cast<std::uint64_t>(put);
    }
    return true;
}

bool Transform(const std::filesystem::path& file, const Keys& keys, std::uint64_t size, bool encrypt, Block& mac) {
    const int descriptor = open(file.c_str(), O_RDWR);
    if (descriptor < 0) return false;
    Context counter(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    Context chunk(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    Context chain(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    bool ok = Start(counter, EVP_aes_128_ctr(), keys.key, keys.counter) &&
              Start(chain, EVP_aes_128_cbc(), keys.key, Block{});
    std::vector<std::uint8_t> input(kLargestChunk), output(kLargestChunk), scratch(kLargestChunk);
    mac.fill(0);
    std::uint64_t offset = 0;
    for (std::size_t index = 1; ok && offset < size; ++index) {
        const std::size_t length = static_cast<std::size_t>(
            std::min<std::uint64_t>(std::min<std::size_t>(index, 8) * kSegment, size - offset));
        const std::size_t padded = (length + 15) & ~std::size_t{15};
        ok = ReadAll(descriptor, input.data(), length, offset) && Apply(counter, input.data(), output.data(), length);
        std::uint8_t* plain = encrypt ? input.data() : output.data();
        std::fill(plain + length, plain + padded, std::uint8_t{0});
        ok = ok && Start(chunk, EVP_aes_128_cbc(), keys.key, keys.mac_iv) && Apply(chunk, plain, scratch.data(), padded);
        Block chunk_mac{};
        if (ok) std::copy_n(scratch.data() + padded - 16, 16, chunk_mac.begin());
        ok = ok && Apply(chain, chunk_mac.data(), mac.data(), 16) && WriteAll(descriptor, output.data(), length, offset);
        offset += length;
    }
    return close(descriptor) == 0 && ok;
}

std::string Refusal(std::int64_t code) {
    switch (code) {
    case -3:
        return "O MEGA está ocupado. Tente de novo.";
    case -4:
        return "O MEGA recebeu pedidos demais. Tente mais tarde.";
    case -9:
        return "A tradução não existe mais no MEGA.";
    case -16:
        return "A tradução foi bloqueada no MEGA.";
    case -17:
        return "A cota de transferência do MEGA acabou. Tente mais tarde.";
    case -18:
        return "A tradução está indisponível no MEGA no momento.";
    default:
        return "O MEGA recusou o pedido (código " + std::to_string(code) + ").";
    }
}

bool StorageUrl(std::string_view url) {
    constexpr std::string_view scheme = "https://";
    if (!url.starts_with(scheme)) return false;
    url.remove_prefix(scheme.size());
    const auto slash = url.find('/');
    if (slash == url.npos) return false;
    const auto host = url.substr(0, slash);
    if (host.size() <= kStorage.size() || !host.ends_with(kStorage)) return false;
    const auto allowed = [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-'; };
    if (!std::all_of(host.begin(), host.end(), allowed)) return false;
    return std::none_of(url.begin(), url.end(), [](unsigned char c) { return c <= 32 || c == 127 || c == '\\'; });
}

}

bool ParseLink(std::string_view text, Link& link) {
    if (!text.starts_with(kPrefix)) return false;
    text.remove_prefix(kPrefix.size());
    if (text.size() != kHandleSize + 1 + kKeyText || text[kHandleSize] != '#') return false;
    const auto handle = text.substr(0, kHandleSize);
    if (std::any_of(handle.begin(), handle.end(), [](char c) { return Digit(c) < 0; })) return false;
    Link parsed;
    parsed.handle = std::string{handle};
    if (!Decode(text.substr(kHandleSize + 1), parsed.key.data(), parsed.key.size())) return false;
    link = std::move(parsed);
    return true;
}

std::string Format(const Link& link) {
    return std::string{kPrefix} + link.handle + "#" + Encode(link.key.data(), link.key.size());
}

std::string FileRequest(const Link& link) {
    return Json::array({Json{{"a", "g"}, {"g", 1}, {"ssl", 2}, {"p", link.handle}}}).dump();
}

bool ReadFileAnswer(std::string_view text, std::string& url, std::uint64_t& size, std::string& error) {
    const Json answer = Json::parse(text, nullptr, false);
    const Json& file = answer.is_array() && answer.size() == 1 ? answer[0] : answer;
    if (file.is_number_integer()) {
        error = Refusal(file.get<std::int64_t>());
        return false;
    }
    if (!file.is_object()) {
        error = "O MEGA respondeu algo inesperado.";
        return false;
    }
    if (const auto refused = file.find("e"); refused != file.end() && refused->is_number_integer()) {
        error = Refusal(refused->get<std::int64_t>());
        return false;
    }
    const auto link = file.find("g");
    const auto bytes = file.find("s");
    if (link == file.end() || !link->is_string() || !StorageUrl(link->get<std::string>()) || bytes == file.end() ||
        !bytes->is_number_unsigned() || bytes->get<std::uint64_t>() == 0) {
        error = "O MEGA não informou um endereço válido para o arquivo.";
        return false;
    }
    url = link->get<std::string>();
    size = bytes->get<std::uint64_t>();
    return true;
}

bool DecryptFile(const std::filesystem::path& file, const Link& link, std::uint64_t size, std::string& error) {
    std::error_code size_error;
    if (std::filesystem::file_size(file, size_error) != size || size_error) {
        error = "O download do MEGA veio incompleto.";
        return false;
    }
    Block mac{};
    if (!Transform(file, LinkKeys(link), size, false, mac)) {
        error = "Não foi possível decifrar o download do MEGA.";
        return false;
    }
    const auto condensed = Condense(mac);
    if (!std::equal(condensed.begin(), condensed.end(), link.key.begin() + 24)) {
        error = "O arquivo do MEGA não confere com a chave do link.";
        return false;
    }
    return true;
}

bool EncryptFile(const std::filesystem::path& file, const std::array<std::uint8_t, 16>& file_key,
                 const std::array<std::uint8_t, 8>& nonce, Link& link) {
    std::error_code size_error;
    const auto size = std::filesystem::file_size(file, size_error);
    Block mac{};
    if (size_error || !Transform(file, MakeKeys(file_key, nonce.data()), size, true, mac)) return false;
    const auto condensed = Condense(mac);
    std::copy(nonce.begin(), nonce.end(), link.key.begin() + 16);
    std::copy(condensed.begin(), condensed.end(), link.key.begin() + 24);
    for (std::size_t i = 0; i < file_key.size(); ++i) link.key[i] = file_key[i] ^ link.key[i + 16];
    return true;
}

}
