#include "Engine/Core/Console/command_registry.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace dingosdk::console;
struct Context {};
using Commands = Registry<Context>;

int failures = 0;

void expect(bool condition, std::string_view what) {
    if (condition) return;
    ++failures;
    std::printf("FAIL: %.*s\n", static_cast<int>(what.size()), what.data());
}

Commands::Entry entry(std::string name, std::vector<std::string> aliases = {}) {
    Commands::Entry result;
    result.name = std::move(name);
    result.aliases = std::move(aliases);
    result.run = [](const Context &, const Values &, const Output &) {};
    return result;
}

std::vector<std::string> names(const std::vector<const Commands::Entry *> &entries) {
    std::vector<std::string> result;
    for (const auto *e : entries) result.push_back(e->name);
    return result;
}

std::vector<std::string> run(const Commands &registry, std::string_view text) {
    std::vector<std::string> lines;
    const Output out{[&lines](const std::string &line) { lines.push_back(line); }};
    registry.execute(text, Context{}, out);
    return lines;
}
} // namespace

int main() {
    Commands registry;
    registry.add(entry("perf"));
    registry.add(entry("perf hud"));
    registry.add(entry("perf status"));
    registry.add(entry("performance"));
    registry.add(entry("textures", {"perf textures"}));
    registry.add(entry("log", {"log status"}));
    registry.add(entry("log flush"));
    registry.add(entry("loglevel", {"log level"}));
    registry.add(entry("status"));

    // A command that is also the first word of others: help lists those, by name or alias.
    expect(names(registry.under("perf")) == std::vector<std::string>{"perf hud", "perf status", "textures"},
           "under perf: perf hud, perf status, textures; not perf itself or performance");
    expect(names(registry.under("PERF")).size() == 3, "under is case-insensitive");
    // `log` is also `log status`, but is not under itself.
    expect(names(registry.under("log")) == std::vector<std::string>{"log flush", "loglevel"}, "under log: log flush, loglevel");
    expect(registry.under("status").empty(), "nothing is under status");
    expect(registry.under("perf hud").empty(), "nothing is under perf hud");

    // A word after a command that takes none: the error says where its neighbours are.
    auto lines = run(registry, "perf hdu");
    expect(lines.size() == 1 && lines[0] == "error: Too many arguments. Usage: perf. Use help perf for the commands under it.",
           "perf hdu points to help perf: " + (lines.empty() ? std::string("(nothing)") : lines[0]));
    lines = run(registry, "status now");
    expect(lines.size() == 1 && lines[0] == "error: Too many arguments. Usage: status", "status now: usage only");
    // An alias with more words still reaches its command.
    expect(run(registry, "perf textures").empty(), "perf textures runs textures");

    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("command registry: all passed\n");
    return 0;
}
