#include "map_downloads.h"
#include "live_mods.h"
#include "Engine/Core/Log/logging.h"
#include "Engine/Vfs/https_download.h"
#include "Engine/Vfs/mod_catalog.h"
#include "Engine/Vfs/mod_list.h"
#include "Engine/Vfs/mod_package.h"
#include "Extension/UI/Overlay/overlay.h"
#include "Launcher/mod_manager.h"
#include "Launcher/thunderstore.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

namespace dingosdk::map_downloads {
namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

constexpr wchar_t agent[] = L"ReSkate-MapDownload/1";
constexpr std::uint64_t max_answer_bytes = 1024 * 1024;
constexpr std::uint32_t lookup_timeout_seconds = 15;
// A large map over a slow line; the transfer stops early only when it stalls (WinHTTP's own timeouts).
constexpr std::uint32_t download_timeout_seconds = 2 * 60 * 60;
constexpr std::uint64_t max_download_bytes = 8ull << 30;
// The live apply hands the new map's level list to the client tick, which reads the game's
// level registry again: give that a moment before joining, so the map is found.
constexpr auto rejoin_delay = std::chrono::seconds(2);

// One offer and, once accepted, its download. The worker keeps its own reference, so a
// dismissed job finishes (or notices `cancel`) without touching the next one.
struct Job {
    std::mutex mutex;
    View view;
    mods::PackageId id;
    std::string download_url;
    bool have_it{}; // the same version is already in Mods: only switch it on and apply
    std::atomic<bool> cancel{}, accepted{};
    bool rejoin{};
    Clock::time_point rejoin_at{};
};

std::mutex current_mutex;
std::shared_ptr<Job> current;

void log(logging::Level level, const std::string &text) {
    logging::log(level, logging::Channel::assets, "Map download: {}", text);
}

std::wstring wide(std::string_view text) {
    if (text.empty()) return {};
    const auto length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

bool same_name(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
        const auto fold = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; };
        return fold(x) == fold(y);
    });
}

void set(Job &job, auto &&change) {
    std::lock_guard lock(job.mutex);
    change(job.view);
}

void fail(Job &job, std::string why) {
    log(logging::Level::warning, job.view.package + ": " + why);
    set(job, [&](View &shown) {
        shown.stage = Stage::failed;
        shown.detail = std::move(why);
    });
}

// The lookup found nothing to offer: the join's own message, with why.
void refuse(Job &job, std::string why) {
    const auto map = job.view.map, host = job.view.host;
    fail(job, why);
    if (job.cancel) return; // dismissed, or replaced by another offer
    overlay::notify(overlay::NoticeLevel::warning, "Map not installed",
                    "The " + host + " is on " + map + ", which is not installed on this PC, and it could not be downloaded: " + why);
}

// A GET of a small Thunderstore answer; a 404 means the package or version is not there.
std::optional<std::string> fetch(const std::wstring &url, bool &missing) {
    https::Download result;
    auto text = https::get_text(url, max_answer_bytes, lookup_timeout_seconds, agent, &result);
    missing = !text && result.http_status == 404;
    return text;
}

// The installed copy of the package, if any: its folder's entry in the Mods folder.
std::optional<mods::ModEntry> installed_copy(const mods::ModList &list, const mods::PackageId &id) {
    for (const auto &entry : list.entries)
        if (same_name(entry.mod.name, id.full_name())) return entry;
    return std::nullopt;
}

void check(const std::shared_ptr<Job> &job) {
    const auto &id = job->id;
    const auto community = thunderstore::community();
    bool missing{};
    const auto standing_text = fetch(thunderstore::package_url(id.owner, id.name), missing);
    if (!standing_text)
        return refuse(*job, missing ? id.full_name() + " is not on Thunderstore."
                                  : "Thunderstore could not be reached. Check your connection and join again.");
    thunderstore::Standing standing;
    try {
        standing = thunderstore::parse_standing(*standing_text);
    } catch (const std::exception &failure) {
        return refuse(*job, failure.what());
    }
    // A host names any package it likes: only one the ReSkate community lists is downloaded.
    if (!standing.listed_in(community)) return refuse(*job, id.full_name() + " is not a ReSkate package on Thunderstore.");
    const auto release_text = fetch(thunderstore::version_url(id.owner, id.name, id.version), missing);
    const auto release = release_text ? thunderstore::parse_release(*release_text) : std::nullopt;
    if (!release)
        return refuse(*job, release_text || missing ? "Version " + id.version + " of " + id.full_name() + " is no longer on Thunderstore."
                                                  : "Thunderstore could not be reached. Check your connection and join again.");
    const auto size = https::download_size(wide(release->download_url), agent).value_or(0);
    std::string installed;
    bool have_it{};
    if (const auto copy = installed_copy(mods::scan_mods(mods::catalog().data_root), id)) {
        installed = copy->mod.version;
        have_it = copy->mod.package == id.text();
    }
    if (job->cancel) return;
    auto title = id.name; // as the site shows it: underscores as spaces
    std::replace(title.begin(), title.end(), '_', ' ');
    log(logging::Level::info, "offering " + id.text() + " (" + std::to_string(size) + " bytes" +
                                  (have_it ? ", already in Mods" : installed.empty() ? "" : ", replacing v" + installed) + ")");
    std::string map;
    set(*job, [&](View &shown) {
        shown.stage = Stage::offered;
        shown.title = title;
        shown.description = release->description;
        shown.size = size;
        shown.installed = installed;
        shown.deprecated = standing.deprecated;
        shown.nsfw = standing.nsfw;
        map = shown.map;
    });
    {
        std::lock_guard lock(job->mutex);
        job->download_url = release->download_url;
        job->have_it = have_it;
        job->view.same_version = have_it;
    }
    overlay::notify(overlay::NoticeLevel::info, map + " is on Thunderstore",
                    "Open the ReSkate menu's Multiplayer page to download it and join again.");
}

// The package's zip, streamed into Mods/.reskate-download (the launcher's folder for the same).
void download(Job &job, const fs::path &archive) {
    std::string url;
    std::uint64_t expected{};
    {
        std::lock_guard lock(job.mutex);
        url = job.download_url;
        expected = job.view.size;
    }
    std::error_code error;
    fs::create_directories(archive.parent_path(), error);
    fs::remove(archive, error);
    FILE *file{};
    if (_wfopen_s(&file, archive.c_str(), L"wb") || !file) throw std::runtime_error("Could not write the download into the Mods folder.");
    bool written = true;
    std::uint64_t received{};
    const auto result = https::stream(wide(url), expected ? expected + (1 << 20) : max_download_bytes, download_timeout_seconds, agent,
                                      [&](const char *data, std::size_t size, std::uint64_t announced) {
                                          if (job.cancel) return false;
                                          if (std::fwrite(data, 1, size, file) != size) {
                                              written = false;
                                              return false;
                                          }
                                          received += size;
                                          set(job, [&](View &shown) {
                                              shown.received = received;
                                              if (announced) shown.size = announced;
                                          });
                                          return true;
                                      });
    std::fclose(file);
    if (job.cancel) throw std::runtime_error("Download cancelled.");
    if (!written) throw std::runtime_error("Could not write the download (is the disk full?).");
    if (!result.ok)
        throw std::runtime_error("The download stopped (" + (result.http_status && result.http_status != 200
                                                                 ? "HTTP " + std::to_string(result.http_status)
                                                                 : "Windows error " + std::to_string(result.error)) + "). Join again to retry.");
}

// mods.json lists the package switched off: switch it on, keeping every other row.
void enable(const mods::PackageId &id) {
    auto list = mods::scan_mods(mods::catalog().data_root);
    if (!list.issue.empty()) throw std::runtime_error("mods.json could not be read (" + list.issue + "); fix it in the launcher's MODS page.");
    for (auto &entry : list.entries)
        if (same_name(entry.mod.name, id.full_name())) {
            if (entry.enabled) return;
            entry.enabled = true;
            mods::save_mod_order(list.root, list.entries);
            log(logging::Level::info, "switched " + entry.mod.name + " on in mods.json");
            return;
        }
    throw std::runtime_error(id.full_name() + " is not in the Mods folder after installing it.");
}

// The live apply, once any other one running has finished. False when this game cannot apply
// mods live (no merged patch at launch): a restart loads it.
bool apply_live() {
    for (int attempt = 0; attempt < 3; ++attempt) {
        while (live_mods::busy()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto started = live_mods::apply(false);
        if (!live_mods::busy()) {
            if (started.starts_with("No merged mod patch")) return false;
            continue; // another apply began first
        }
        while (live_mods::busy()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto status = live_mods::status();
        if (!status.starts_with("applied")) throw std::runtime_error("The mods could not be applied: " + status);
        return true;
    }
    throw std::runtime_error("The mods could not be applied: " + live_mods::status());
}

void install(const std::shared_ptr<Job> &job) {
    auto &state = *job;
    const auto id = job->id;
    try {
        bool have_it{};
        {
            std::lock_guard lock(job->mutex);
            have_it = job->have_it;
        }
        const auto root = mods::catalog().root;
        // The same version already in Mods (switched off, or not applied yet) needs no download.
        if (!have_it) {
            const auto folder = root / L".reskate-download";
            const auto archive = folder / (wide(id.full_name()) + L".zip");
            struct Cleanup {
                fs::path folder, archive;
                ~Cleanup() {
                    std::error_code ignored;
                    fs::remove(archive, ignored);
                    fs::remove(folder, ignored);
                }
            } cleanup{folder, archive};
            set(state, [](View &shown) { shown.stage = Stage::downloading; });
            download(state, archive);
            set(state, [](View &shown) {
                shown.stage = Stage::installing;
                shown.progress = 0;
            });
            launcher_mods::InstallOptions options;
            options.folder = id.full_name();
            options.author = id.owner;
            options.require_content = true;
            const auto name = launcher_mods::install(root, archive, true,
                                                     [&](float fraction) { set(state, [&](View &shown) { shown.progress = fraction; }); },
                                                     job->cancel, options);
            log(logging::Level::info, "installed " + id.text() + " into Mods\\" + name);
        }
        if (job->cancel) return;
        enable(id);
        set(state, [](View &shown) { shown.stage = Stage::applying; });
        const bool live = apply_live();
        std::string map;
        {
            std::lock_guard lock(job->mutex);
            job->view.stage = Stage::done;
            job->view.restart = !live;
            job->view.detail = live ? "Joining the " + job->view.host + " again..."
                                    : "Restart the game to load it, then join again.";
            job->rejoin = live;
            job->rejoin_at = Clock::now() + rejoin_delay;
            map = job->view.map;
        }
        log(logging::Level::info, live ? id.text() + " applied; joining again" : id.text() + " installed; a restart loads it");
        overlay::notify(overlay::NoticeLevel::info, "Installed " + map,
                        live ? "Joining again..." : "Restart the game to load it, then join again.");
    } catch (const std::exception &failure) {
        fail(state, failure.what());
        if (!job->cancel) overlay::notify(overlay::NoticeLevel::warning, "Map download failed", failure.what());
    }
}

// Runs `work` on its own thread; a throw ends the job as failed.
void start(const std::shared_ptr<Job> &job, void (*work)(const std::shared_ptr<Job> &)) {
    std::thread([job, work] {
        try {
            work(job);
        } catch (const std::exception &failure) {
            fail(*job, failure.what());
        } catch (...) {
            fail(*job, "The map download failed.");
        }
    }).detach();
}

bool running(Stage stage) {
    return stage == Stage::downloading || stage == Stage::installing || stage == Stage::applying;
}
} // namespace

bool offer(std::string package, std::string map, std::string host) {
    const auto id = mods::parse_package(package);
    if (!id) return false;
    auto job = std::make_shared<Job>();
    job->id = *id;
    job->view.stage = Stage::checking;
    job->view.package = std::move(package);
    job->view.map = std::move(map);
    job->view.host = std::move(host);
    {
        std::lock_guard lock(current_mutex);
        if (current) {
            if (current->accepted && !current->cancel) {
                std::lock_guard view_lock(current->mutex);
                if (running(current->view.stage)) return false;
            }
            current->cancel = true;
        }
        current = job;
    }
    log(logging::Level::info, "looking up " + job->view.package + " for " + job->view.map);
    start(job, check);
    return true;
}

std::string accept() {
    std::shared_ptr<Job> job;
    {
        std::lock_guard lock(current_mutex);
        job = current;
    }
    if (!job) return "There is no map to download.";
    {
        std::lock_guard lock(job->mutex);
        if (job->view.stage != Stage::offered) return "The download is not ready.";
        job->view.stage = Stage::downloading;
        job->view.received = 0;
    }
    job->accepted = true;
    log(logging::Level::info, "accepted " + job->view.package);
    start(job, install);
    return "Downloading the map...";
}

void dismiss() {
    std::lock_guard lock(current_mutex);
    if (!current) return;
    current->cancel = true;
    current.reset();
}

View view() {
    std::shared_ptr<Job> job;
    {
        std::lock_guard lock(current_mutex);
        job = current;
    }
    if (!job) return {};
    std::lock_guard lock(job->mutex);
    return job->view;
}

bool take_rejoin() {
    std::shared_ptr<Job> job;
    {
        std::lock_guard lock(current_mutex);
        job = current;
    }
    if (!job || job->cancel) return false;
    std::lock_guard lock(job->mutex);
    if (!job->rejoin || Clock::now() < job->rejoin_at) return false;
    job->rejoin = false;
    return true;
}

} // namespace dingosdk::map_downloads
