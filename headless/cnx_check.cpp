#include "cnx.h"

#include "mega.h"
#include "miniz.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using namespace Eden::CNX;

constexpr const char* kStorage = "https://gfs1.userstorage.mega.co.nz/dl/fixture";
constexpr const char* kCatalog = "https://www.costelabr.xyz/nx-links/nx-links-v423.json";
constexpr const char* kMirror = "https://www.gamemod.com.br/nx-links/nx-links-v423.json";
constexpr std::array<std::uint8_t, 16> kFileKey = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
constexpr std::array<std::uint8_t, 8> kNonce = {21, 22, 23, 24, 25, 26, 27, 28};

int checks = 0;

void Require(bool ok, const char* message) {
    ++checks;
    if (!ok) {
        std::fprintf(stderr, "CNX FAIL: %s\n", message);
        std::exit(1);
    }
}

void Write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

std::string MegaLink() {
    Eden::Mega::Link link;
    link.handle = "AbCd-_12";
    for (std::size_t i = 0; i < link.key.size(); ++i) link.key[i] = static_cast<std::uint8_t>(i * 7 + 3);
    return Eden::Mega::Format(link);
}

Json Translation(const std::string& title = "0100ABCD00001000") {
    return {{"enabled", true}, {"size", "53.7 MB"},
            {"link", MegaLink()},
            {"installed", {{"hash_file", title + "/romfs/lang/en.bin"}, {"hash_size", 4},
                           {"uninstall", {{"path01", title + "/romfs"}}}}}};
}

Entry ParseOne(Json value) {
    std::string error;
    const auto entries = ParseCatalog(Json{{"translations", {{"Game", value}}}}.dump(), error);
    Require(entries.size() == 1, "parse one catalog entry");
    return entries.front();
}

void Parsing() {
    auto entry = ParseOne(Translation());
    Require(entry.error.empty() && entry.size == "53.7 MB", "size is text");
    Require(entry.title_id == 0x0100ABCD00001000ull && entry.hash_file == "romfs/lang/en.bin" &&
            entry.hash_size == 4, "hash-file title and relative path");
    auto value = Translation();
    value["installed"].erase("hash_file");
    value["installed"].erase("hash_size");
    value["installed"]["uninstall"]["path01"] = "0100abcd00001000";
    entry = ParseOne(value);
    Require(entry.error.empty() && entry.hash_file.empty() && entry.title_id == 0x0100ABCD00001000ull,
            "Dark Devotion shape uses uninstall title without hash");
    value.erase("installed");
    entry = ParseOne(value);
    Require(!entry.error.empty() && entry.title_id == 0, "translation without title metadata rejected");
    value["installed"] = {{"hash_file", "0100abcd00001000/romfs/en.bin"}, {"hash_size", 4}};
    entry = ParseOne(value);
    Require(entry.error.empty() && entry.title_id == 0x0100ABCD00001000ull, "hash title without filename title");
    value["installed"]["uninstall"]["path01"] = "0100FFFF00002000/romfs";
    entry = ParseOne(value);
    Require(entry.title_id == 0x0100ABCD00001000ull && !entry.error.empty(), "hash source priority and conflict");
    value = Translation();
    value["installed"].erase("hash_file");
    value["installed"]["uninstall"]["path01"] = "0100FFFF00002000/romfs";
    entry = ParseOne(value);
    Require(entry.title_id == 0x0100FFFF00002000ull && entry.error.empty(), "uninstall title used without hash file");
    entry = ParseOne(Translation("0100414d32524e00"));
    Require(entry.error.empty() && entry.title_id == 0x0100414D32524E00ull, "preserve homebrew title suffix");
    value = Translation();
    value["installed"]["hash_file"] = "100ABCD00001000/romfs/lang/en.bin";
    value["installed"]["uninstall"]["path01"] = "100ABCD00001000/romfs";
    entry = ParseOne(value);
    Require(entry.error.empty() && entry.title_id == 0x0100ABCD00001000ull &&
            entry.hash_file == "romfs/lang/en.bin", "metadata without leading hexadecimal zero");
    value["installed"]["hash_size"] = 47.129;
    Require(!ParseOne(value).error.empty(), "fractional hash size rejected");
    value = Translation();
    value["installed"]["hash_file"] = "0100ABCD00001000/romfs/../../escape";
    Require(!ParseOne(value).error.empty(), "unsafe metadata path rejected");
    value = Translation();
    value["installed"].erase("hash_size");
    Require(!ParseOne(value).error.empty(), "missing hash size rejected");
    value = Translation();
    value["size"] = 537;
    Require(!ParseOne(value).error.empty(), "numeric size rejected");
    for (const std::string& link : std::vector<std::string>{"http" + MegaLink().substr(5), MegaLink().substr(0, MegaLink().size() - 1),
                                   MegaLink() + "A", "https://mega.nz/folder/AbCd-_12#" + MegaLink().substr(30),
                                   "https://github.com/AMSNX/Traducoes-BR-NX/releases/download/test/0100ABCD00001000.zip",
                                   "https://my.microsoftpersonalcontent.com/personal/x/0100ABCD00001000.zip"}) {
        value = Translation();
        value["link"] = link;
        Require(!ParseOne(value).error.empty(), "only complete HTTPS MEGA file links");
    }
    std::string error;
    const auto entries = ParseCatalog(Json{{"translations", {{"Valid", Translation()},
        {"Disabled", {{"enabled", false}}}}}, {"modifications", {{"Not a translation", Translation()}}}}.dump(), error);
    Require(error.empty() && entries.size() == 1 && entries[0].name == "Valid", "only enabled translations");
    Require(ParseCatalog("[]", error).empty() && !error.empty(), "invalid top-level shape");
    Require(ParseCatalog("not JSON", error).empty() && !error.empty(), "malformed JSON");
    const auto duplicates = ParseCatalog(Json{{"translations", {{"First", Translation()},
        {"Second", Translation()}}}}.dump(), error);
    Require(duplicates.size() == 2 && !duplicates[0].error.empty() && !duplicates[1].error.empty(),
            "duplicate valid title IDs are ambiguous");
    auto wrong = Translation();
    wrong["installed"]["uninstall"]["path01"] = "0100FFFF00002000/romfs";
    const auto distinct = ParseCatalog(Json{{"translations", {{"First", Translation()},
        {"Second", wrong}}}}.dump(), error);
    Require(error.empty() && distinct.size() == 2 && distinct[0].error.empty() && !distinct[1].error.empty(),
            "invalid conflicting entry does not block valid entry");
}

void Detection(const fs::path& base) {
    auto entry = ParseOne(Translation());
    const fs::path root = base / "detection" / "mods";
    Require(!Detect(root, entry).installed, "missing mods folder");
    Write(root / "0100abcd00001000" / "ptbr" / "ROMFS" / "LANG" / "en.bin", "data");
    auto state = Detect(root, entry);
    Require(state.installed && !state.managed && state.manual_mod == "ptbr", "manual translation and folder case");
    Write(root / "0100abcd00001000" / "ptbr" / "ROMFS" / "LANG" / "en.bin", "bad");
    Require(!Detect(root, entry).installed, "wrong hash size is not installed");
    const auto managed = root / "0100abcd00001000" / kModName;
    Write(managed / "romfs/lang/en.bin", "data");
    state = Detect(root, entry);
    Require(state.installed && !state.managed && state.manual_mod == kModName,
            "manual mod with reserved name is not owned");
    std::string error;
    Require(!Remove(root, entry, error) && fs::exists(managed / "romfs/lang/en.bin"),
            "manual mod with reserved name is never removed");
    const auto lower_managed = root / "0100abcd00001000/pt-br cnx";
    fs::rename(managed, lower_managed);
    Require(!Install(root, entry, [](const auto&, const auto&, auto&) { return false; }, error) &&
            fs::exists(lower_managed / "romfs/lang/en.bin"), "manual reserved folder case preserved");
    fs::rename(lower_managed, managed);
    Write(managed / ".cnx-owned", "CNX PT-BR 1\n");
    state = Detect(root, entry);
    Require(state.installed && state.managed, "managed translation detected");
    entry.hash_file.clear();
    Require(Detect(root, entry).installed, "no-hash entry uses managed folder");
    fs::remove_all(managed);
    Require(!Detect(root, entry).installed, "no-hash ignores unrelated manual folder");
    entry = ParseOne(Translation());
    const auto external = base / "external";
    Write(external / "romfs/lang/en.bin", "data");
    fs::create_directory_symlink(external, root / "0100abcd00001000" / "linked");
    Require(!Detect(root, entry).installed, "symlink mod is not followed");
    fs::create_directory_symlink(external, managed);
    Require(!Remove(root, entry, error) && fs::exists(external / "romfs/lang/en.bin"), "managed symlink removal refused");
    fs::remove(managed);
    fs::remove(root / "0100abcd00001000" / "linked");
}

void Zip(const fs::path& path, const std::vector<std::pair<std::string, std::string>>& files,
         bool symlink = false) {
    mz_zip_archive archive{};
    Require(mz_zip_writer_init_file(&archive, path.c_str(), 0) != 0, "create fixture ZIP");
    std::vector<std::pair<std::string, std::string>> replacements;
    for (const auto& [name, data] : files) {
        std::string accepted = name;
        if (!accepted.empty() && accepted.front() == '/') {
            accepted.front() = 'x';
            replacements.emplace_back(accepted, name);
        }
        Require(mz_zip_writer_add_mem(&archive, accepted.c_str(), data.data(), data.size(), MZ_DEFAULT_COMPRESSION) != 0,
                "write fixture ZIP entry");
    }
    Require(mz_zip_writer_finalize_archive(&archive) != 0, "finalize fixture ZIP");
    Require(mz_zip_writer_end(&archive) != 0, "close fixture ZIP");
    if (symlink || !replacements.empty()) {
        std::fstream stream(path, std::ios::binary | std::ios::in | std::ios::out);
        std::string data{std::istreambuf_iterator<char>(stream), {}};
        for (const auto& [accepted, original] : replacements)
            for (std::size_t at = 0; (at = data.find(accepted, at)) != data.npos; at += original.size())
                data.replace(at, accepted.size(), original);
        if (symlink) {
            const auto offset = data.find(std::string("PK\1\2", 4));
            Require(offset != data.npos, "fixture ZIP central directory");
            data[offset + 5] = 3;
            const std::uint32_t attributes = 0120777u << 16;
            for (unsigned i = 0; i < 4; ++i) data[offset + 38 + i] = static_cast<char>(attributes >> (i * 8));
        }
        stream.seekp(0);
        stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
}

struct Served {
    std::string link;
    Fetch fetch;
};

Served Serve(const fs::path& archive, bool tamper = false) {
    const fs::path sealed = archive.string() + ".sealed";
    fs::copy_file(archive, sealed, fs::copy_options::overwrite_existing);
    Eden::Mega::Link link;
    link.handle = "Fixture1";
    Require(Eden::Mega::EncryptFile(sealed, kFileKey, kNonce, link), "seal fixture for MEGA");
    if (tamper) {
        std::fstream stream(sealed, std::ios::binary | std::ios::in | std::ios::out);
        stream.seekp(10);
        stream.put('\x5a');
    }
    const std::uint64_t size = fs::file_size(sealed);
    Fetch fetch = [sealed, size](const Request& request, const fs::path& destination, std::string&) {
        if (request.url == Eden::Mega::kApiUrl) {
            Require(request.small && request.body.find("Fixture1") != std::string::npos, "MEGA asked for the file");
            Write(destination, Json::array({Json{{"g", kStorage}, {"s", size}}}).dump());
            return true;
        }
        Require(request.url == kStorage && request.limit == size && !request.small && request.body.empty(),
                "MEGA file downloaded from its storage address");
        std::error_code error;
        return fs::copy_file(sealed, destination, fs::copy_options::overwrite_existing, error);
    };
    return {Eden::Mega::Format(link), fetch};
}

bool InstallFrom(const fs::path& root, Entry entry, const fs::path& archive, std::string& error, bool tamper = false) {
    const auto served = Serve(archive, tamper);
    entry.link = served.link;
    return Install(root, entry, served.fetch, error);
}

bool NoTemporary(const fs::path& root) {
    for (const auto& child : fs::directory_iterator(root))
        if (child.path().filename().string().starts_with(".cnx-")) return false;
    return true;
}

void Installing(const fs::path& base) {
    const auto entry = ParseOne(Translation());
    const auto archive = base / "fixture.zip";
    Zip(archive, {{"0100abcd00001000/romfs/lang/en.bin", "data"}});
    std::string error;
    for (const auto& path : {base / "data/eglantineps5/mods", base / "mnt/ext1/ROMs/Switch/mods"}) {
        Require(InstallFrom(path, entry, archive, error), "first installation and alternate games root");
        Require(fs::exists(path / "0100ABCD00001000/PT-BR CNX/romfs/lang/en.bin"), "ZIP title root stripped");
        Require(Detect(path, entry).installed, "installed files match metadata");
        const auto managed = path / "0100ABCD00001000/pt-br cnx";
        fs::rename(path / "0100ABCD00001000/PT-BR CNX", managed);
        Require(Detect(path, entry).managed, "managed folder detected after case-only rename");
        Require(!InstallFrom(path, entry, archive, error), "existing mod is never overwritten");
        if (geteuid() != 0) {
            const auto locked = managed / "locked";
            Write(locked / "data.bin", "preserve ownership until complete removal");
            Require(chmod(locked.c_str(), 0) == 0, "restrict directory for partial removal test");
            Require(!Remove(path, entry, error) && Detect(path, entry).managed,
                    "failed partial removal preserves ownership for retry");
            Require(chmod(locked.c_str(), 0700) == 0, "restore removable directory");
        }
        Write(path / "0100ABCD00001000/ptbr/romfs/lang/en.bin", "data");
        Require(Remove(path, entry, error), "remove managed mod");
        Require(!fs::exists(managed) &&
                fs::exists(path / "0100ABCD00001000/ptbr/romfs/lang/en.bin"), "manual translation preserved");
        Require(Detect(path, entry).installed, "manual state survives removal");
        Require(NoTemporary(path), "successful temporary cleanup");
    }
    const auto root = base / "unsafe/mods";
    const std::vector<std::string> unsafe = {
        "../escape", "/0100ABCD00001000/romfs/evil", "0100ABCD00001000/../escape",
        "0100ABCD00001000/romfs/../../escape", "0100ABCD00001000\\romfs\\evil",
        "0100ABCD00001000/romfs/C:evil", "0100FFFF00002000/romfs/evil",
        "readme.txt", "0100ABCD00001000/./romfs/evil", "0100ABCD00001000//romfs/evil"
    };
    for (const auto& name : unsafe) {
        Zip(archive, {{"0100ABCD00001000/romfs/lang/en.bin", "data"}, {name, "evil"}});
        Require(!InstallFrom(root, entry, archive, error), "unsafe archive refused before extraction");
        Require(!fs::exists(root / "0100ABCD00001000/PT-BR CNX"), "unsafe archive left no managed mod");
        Require(NoTemporary(root), "failed temporary cleanup");
    }
    Zip(archive, {{"0100ABCD00001000/romfs/lang/en.bin", "data"}}, true);
    Require(!InstallFrom(root, entry, archive, error), "ZIP symlink refused");
    Zip(archive, {{"0100ABCD00001000/romfs/lang/en.bin", "data"},
                  {"0100ABCD00001000/ROMFS/lang/EN.BIN", "evil"}});
    Require(!InstallFrom(root, entry, archive, error), "case-colliding ZIP entries refused");
    Zip(archive, {{"0100ABCD00001000/romfs/lang/en.bin", "data"},
                  {"0100ABCD00001000/ROMFS/other.bin", "evil"}});
    Require(!InstallFrom(root, entry, archive, error), "case-colliding ZIP parent directories refused");
    Zip(archive, {{"0100ABCD00001000/romfs", "evil"},
                  {"0100ABCD00001000/romfs/lang/en.bin", "data"}});
    Require(!InstallFrom(root, entry, archive, error), "ZIP file cannot also be a parent directory");
    Zip(archive, {{"0100ABCD00001000/romfs/lang/en.bin", "wrong"}});
    Require(!InstallFrom(root, entry, archive, error), "ZIP hash size mismatch refused");
    Zip(archive, {{"0100ABCD00001000/romfs/lang/en.bin", "data"}});
    Require(!InstallFrom(root, entry, archive, error, true) && error.find("chave") != std::string::npos &&
            !fs::exists(root / "0100ABCD00001000/PT-BR CNX"), "MEGA download that fails its MAC is refused");
    auto gone = entry;
    gone.link = MegaLink();
    Require(!Install(root, gone, [](const Request& request, const fs::path& destination, std::string&) {
        Write(destination, request.url == Eden::Mega::kApiUrl ? "[-9]" : "");
        return true;
    }, error) && error.find("não existe") != std::string::npos, "file removed from MEGA is reported");
    Require(NoTemporary(root), "validation failure cleanup");
    Require(!Install(root, entry, [](const auto&, const auto& destination, std::string& failure) {
        Write(destination, "partial download"); failure = "offline"; return false;
    }, error) && NoTemporary(root), "partial failed download cleanup");
    auto conflict = entry;
    conflict.error = "IDs inconsistentes";
    bool requested = false;
    Require(!Install(root, conflict, [&](const auto&, const auto&, auto&) { requested = true; return false; }, error)
            && !requested, "inconsistent metadata cannot download");
}

std::string Index() {
    return Json{{"4.2.2", "https://www.costelabr.xyz/nx-links/nx-links-v422.json,"},
                {"4.2.3", std::string{kCatalog} + "," + kMirror + ","},
                {"4.2.3b", "https://www.costelabr.xyz/nx-links/nx-links-v424.json,"}}.dump();
}

Fetch Catalog(const std::string& body, int& requests) {
    return [body, &requests](const Request& request, const fs::path& destination, std::string&) {
        if (request.url == kIndexUrls[0] || request.url == kIndexUrls[1]) {
            if (request.url == kIndexUrls[0]) ++requests;
            Write(destination, Index());
            return true;
        }
        Require((request.url == kCatalog || request.url == kMirror) && request.small && request.body.empty(),
                "newest stable CNX catalog");
        Write(destination, body);
        return true;
    };
}

void Indexing() {
    Require(CatalogUrls(Index()) == std::vector<std::string>{kCatalog, kMirror},
            "index gives the newest stable catalog and its mirror");
    const auto versions = Json{{"4.9.9", "https://costelabr.xyz/nx-links/nx-links-v499.json"},
                               {"4.10.0", "https://costelabr.xyz/nx-links/nx-links-v4100.json"},
                               {"5.0", 7}, {"latest", "https://costelabr.xyz/nx-links/nx-links-v9.json"}}.dump();
    Require(CatalogUrls(versions) == std::vector<std::string>{"https://costelabr.xyz/nx-links/nx-links-v4100.json"},
            "versions compare by number and odd entries are skipped");
    const auto foreign = Json{{"4.2.3", "https://evil.example/nx-links/nx-links-v423.json,http://www.costelabr.xyz/nx-links/nx-links-v423.json,"
                                        "https://www.costelabr.xyz/nx-links/other.json,https://www.costelabr.xyz/nx-links/nx-links-v42x.json"}}.dump();
    Require(CatalogUrls(foreign).empty(), "catalogs only from the known HTTPS addresses");
    Require(CatalogUrls("not JSON").empty() && CatalogUrls("[]").empty(), "malformed index");
}

void Mega(const fs::path& base) {
    Eden::Mega::Link link;
    Require(Eden::Mega::ParseLink(MegaLink(), link) && link.handle == "AbCd-_12" && link.key[31] == 220 &&
            Eden::Mega::Format(link) == MegaLink(), "MEGA link round trip");
    Require(Json::parse(Eden::Mega::FileRequest(link)) ==
            Json::array({Json{{"a", "g"}, {"g", 1}, {"ssl", 2}, {"p", "AbCd-_12"}}}), "MEGA file request");
    std::string url, error;
    std::uint64_t size = 0;
    Require(Eden::Mega::ReadFileAnswer(Json::array({Json{{"g", kStorage}, {"s", 5}}}).dump(), url, size, error) &&
            url == kStorage && size == 5, "MEGA answer gives the storage address");
    for (const std::string answer : {"[-9]", "-3", "[{\"e\": -17}]"}) {
        error.clear();
        Require(!Eden::Mega::ReadFileAnswer(answer, url, size, error) && !error.empty(), "MEGA refusals reported");
    }
    for (const std::string address : {"http://gfs1.userstorage.mega.co.nz/dl/x", "https://evil.example/dl/x",
                                      "https://gfs1.userstorage.mega.co.nz.evil.example/dl/x",
                                      "https://userstorage.mega.co.nz/dl/x", "https://gfs1.userstorage.mega.co.nz/dl/a b"})
        Require(!Eden::Mega::ReadFileAnswer(Json::array({Json{{"g", address}, {"s", 5}}}).dump(), url, size, error),
                "MEGA storage address must be HTTPS on its storage hosts");
    Require(!Eden::Mega::ReadFileAnswer(Json::array({Json{{"g", kStorage}}}).dump(), url, size, error) &&
            !Eden::Mega::ReadFileAnswer("not JSON", url, size, error), "incomplete MEGA answers refused");
    const fs::path file = base / "mega.bin";
    for (const std::size_t length : {std::size_t{1}, std::size_t{16}, std::size_t{0x20000 * 3 + 5}, std::size_t{(5u << 20) + 7}}) {
        std::string data(length, '\0');
        for (std::size_t i = 0; i < length; ++i) data[i] = static_cast<char>((i * 131 + length) & 0xff);
        Write(file, data);
        Eden::Mega::Link sealed;
        sealed.handle = "Fixture1";
        Require(Eden::Mega::EncryptFile(file, kFileKey, kNonce, sealed), "MEGA encryption");
        std::ifstream encrypted(file, std::ios::binary);
        Require(std::string{std::istreambuf_iterator<char>(encrypted), {}} != data, "MEGA file is encrypted");
        Require(Eden::Mega::DecryptFile(file, sealed, length, error), "MEGA decryption checks its MAC");
        std::ifstream plain(file, std::ios::binary);
        Require(std::string{std::istreambuf_iterator<char>(plain), {}} == data, "MEGA decryption restores the file");
        Require(Eden::Mega::EncryptFile(file, kFileKey, kNonce, sealed), "MEGA encryption again");
        Require(!Eden::Mega::DecryptFile(file, sealed, length + 1, error), "MEGA size mismatch refused");
        sealed.key[30] ^= 1;
        Require(!Eden::Mega::DecryptFile(file, sealed, length, error), "MEGA MAC mismatch refused");
    }
}

void Caching(const fs::path& base) {
    const auto cache = base / "cache/cnx.json";
    const auto good = Json{{"translations", {{"Game", Translation()}}}}.dump();
    std::vector<Entry> entries;
    bool cached = false;
    std::string error;
    int requests = 0;
    const Fetch online = Catalog(good, requests);
    Require(LoadCatalog(cache, 100000, online, entries, cached, error) && !cached && entries.size() == 1,
            "initial catalog saved");
    Require(LoadCatalog(cache, 100100, online, entries, cached, error) && cached && requests == 1,
            "same-day cache avoids network");
    Require(LoadCatalog(cache, 186400, online, entries, cached, error) && !cached && requests == 2,
            "catalog updates after 24 hours");
    const Fetch offline = [&](const Request& request, const fs::path&, std::string& failure) {
        if (request.url == kIndexUrls[0]) ++requests;
        failure = "offline";
        return false;
    };
    Require(LoadCatalog(cache, 272800, offline, entries, cached, error) && cached && !error.empty() &&
            entries.size() == 1 && entries[0].name == "Game", "offline uses last saved catalog");
    Require(LoadCatalog(cache, 272801, offline, entries, cached, error) && cached && requests == 3,
            "failed daily update is not repeated");
    int ignored = 0;
    const Fetch invalid = Catalog("{broken JSON", ignored);
    Require(LoadCatalog(cache, 359200, invalid, entries, cached, error) && cached && !error.empty(),
            "bad response preserves valid cache");
    Require(LoadCatalog(cache, 359201, offline, entries, cached, error) && entries.size() == 1,
            "valid cache survives restart after bad response");
    const Fetch unusable = Catalog(Json{{"translations", {{"Broken", {{"size", "53 MB"}}}}}}.dump(), ignored);
    Require(LoadCatalog(cache, 445600, unusable, entries, cached, error) && cached && !error.empty() &&
            entries.size() == 1 && entries.front().name == "Game", "unusable entries never replace valid cache");
    Require(!LoadCatalog(base / "no-cache/cnx.json", 100000, offline, entries, cached, error) &&
            entries.empty() && !error.empty(), "offline without saved catalog reports error");
    const Fetch online_mirror = Catalog(good, ignored);
    const Fetch mirror = [&](const Request& request, const fs::path& destination, std::string& failure) {
        if (request.url == kIndexUrls[0]) {
            failure = "offline";
            return false;
        }
        return online_mirror(Request{request.url == kIndexUrls[1] ? kIndexUrls[0] : request.url, request.body,
                                     request.limit, request.small}, destination, failure);
    };
    Require(LoadCatalog(base / "mirror/cnx.json", 100000, mirror, entries, cached, error) && entries.size() == 1,
            "second index address used when the first is down");
    const Fetch broken_first = [&](const Request& request, const fs::path& destination, std::string&) {
        Write(destination, request.url == kIndexUrls[0] ? Index() : request.url == kCatalog ? "<html>erro</html>" : good);
        return true;
    };
    Require(LoadCatalog(base / "broken-first/cnx.json", 100000, broken_first, entries, cached, error) &&
            entries.size() == 1 && error.empty(), "an invalid catalog answer falls back to the mirror");
    const auto old_format = base / "old-format/cnx.json";
    Write(old_format, Json{{"checked_at", 100000}, {"updated_at", 100000}, {"catalog", Json::parse(good)}}.dump());
    int refreshed = 0;
    Require(LoadCatalog(old_format, 100100, Catalog(good, refreshed), entries, cached, error) && !cached &&
            refreshed == 1 && entries.size() == 1, "a list saved by an older version is downloaded again");
}

}

int main(int argc, char** argv) {
    std::string temporary = "/private/tmp/cnx-check-XXXXXX";
#if !defined(__APPLE__)
    temporary = "/tmp/cnx-check-XXXXXX";
#endif
    Require(mkdtemp(temporary.data()) != nullptr, "create test directory");
    const fs::path base = temporary;
    Parsing();
    Detection(base);
    Installing(base);
    Indexing();
    Mega(base);
    Caching(base);
    if (argc > 1) {
        std::ifstream stream(argv[1], std::ios::binary);
        const std::string body{std::istreambuf_iterator<char>(stream), {}};
        std::string error;
        const auto entries = ParseCatalog(body, error);
        Require(error.empty() && !entries.empty(), "live catalog has usable entries");
        for (const auto& entry : entries)
            if (!entry.error.empty()) std::printf("CNX blocked: %s: %s\n", entry.name.c_str(), entry.error.c_str());
        std::printf("CNX usable: %zu of %zu\n",
                    static_cast<std::size_t>(std::count_if(entries.begin(), entries.end(), [](const Entry& entry) { return entry.error.empty(); })),
                    entries.size());
    }
    fs::remove_all(base);
    std::printf("CNX: %d checks passed\n", checks);
}
