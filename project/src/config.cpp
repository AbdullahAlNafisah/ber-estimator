// config.cpp — minimal, no get_or, matches your ini as-is
#include "config.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

// Trim both ends
static std::string trim(std::string s)
{
    auto sp = [](unsigned char c)
    { return std::isspace(c); };
    while (!s.empty() && sp((unsigned char)s.front()))
        s.erase(s.begin());
    while (!s.empty() && sp((unsigned char)s.back()))
        s.pop_back();
    return s;
}

// INI → map<string,string>, keys like "snr.start_db"
static std::unordered_map<std::string, std::string> parse_ini(const std::string &path)
{
    std::ifstream ifs(path);
    if (!ifs)
        throw std::runtime_error("Cannot open config file: " + path);
    std::unordered_map<std::string, std::string> kv;
    std::string line, section;
    while (std::getline(ifs, line))
    {
        auto cut = line.find_first_of("#;");
        if (cut != std::string::npos)
            line = line.substr(0, cut);
        line = trim(line);
        if (line.empty())
            continue;
        if (line.front() == '[' && line.back() == ']')
        {
            section = line.substr(1, line.size() - 2);
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        auto key = trim(line.substr(0, eq));
        auto val = trim(line.substr(eq + 1));
        if (!section.empty())
            key = section + "." + key;
        if (!key.empty() && !val.empty())
            kv[key] = val;
    }
    return kv;
}

// Require and convert
template <typename F>
static auto require_as(const std::unordered_map<std::string, std::string> &kv,
                       const std::string &key,
                       F conv) -> decltype(conv(std::string{}))
{
    auto it = kv.find(key);
    if (it == kv.end())
        throw std::invalid_argument("Missing required key in config.ini: " + key);
    try
    {
        return conv(it->second);
    }
    catch (const std::exception &e)
    {
        throw std::invalid_argument("Invalid value for key '" + key + "': '" + it->second + "' (" + e.what() + ")");
    }
}

Config Config::load(const std::string &path)
{
    const auto kv = parse_ini(path);

    Config c{}; // clear/init

    // Core SNR
    c.snr_start_db = require_as(kv, "snr.start_db", [](const std::string &s)
                                { return std::stod(s); });
    c.snr_stop_db = require_as(kv, "snr.stop_db", [](const std::string &s)
                               { return std::stod(s); });
    c.snr_step_db = require_as(kv, "snr.step_db", [](const std::string &s)
                               { return std::stod(s); });

    // Stopping
    c.min_errors = require_as(kv, "stopping.min_errors", [](const std::string &s)
                              { return (uint64_t)std::stoull(s); });
    c.max_bits = require_as(kv, "stopping.max_bits", [](const std::string &s)
                            { return (uint64_t)std::stoull(s); });
    c.ber_floor = require_as(kv, "stopping.ber_floor", [](const std::string &s)
                             { return std::stod(s); });

    // IO + RNG
    c.outfile = require_as(kv, "io.file", [](const std::string &s)
                           { return s; });
    c.seed = require_as(kv, "rng.seed", [](const std::string &s)
                        { return (uint64_t)std::stoull(s); });

    // Model
    c.modem = require_as(kv, "model.modem", [](const std::string &s)
                         { return s; });
    c.channel = require_as(kv, "model.channel", [](const std::string &s)
                           { return s; });
    c.coder = require_as(kv, "model.coder", [](const std::string &s)
                         { return s; });
    c.frame_len = require_as(kv, "model.frame_len", [](const std::string &s)
                             { return std::stoi(s); });

    // CI
    c.ci_level = require_as(kv, "ci.level", [](const std::string &s)
                            { return std::stod(s); });
    c.ci_abs = require_as(kv, "ci.abs", [](const std::string &s)
                          { return std::stod(s); });
    c.ci_rel = require_as(kv, "ci.rel", [](const std::string &s)
                          { return std::stod(s); });
    c.ci_min_bits = require_as(kv, "ci.min_bits", [](const std::string &s)
                               { return (uint64_t)std::stoull(s); });

    // Threads
    c.threads = require_as(kv, "parallel.threads", [](const std::string &s)
                           { return (unsigned)std::stoul(s); });

    // Validation
    if (c.snr_step_db <= 0.0)
        throw std::invalid_argument("snr.step_db must be > 0");
    if (c.snr_stop_db < c.snr_start_db)
        throw std::invalid_argument("snr.stop_db must be >= snr.start_db");
    if (c.ber_floor < 0.0)
        throw std::invalid_argument("stopping.ber_floor must be >= 0");
    if (c.frame_len <= 0)
        throw std::invalid_argument("model.frame_len must be > 0");
    if (!(c.ci_level > 0.0 && c.ci_level < 1.0))
        throw std::invalid_argument("ci.level must be in (0,1)");
    if (c.ci_abs < 0.0 || c.ci_rel < 0.0)
        throw std::invalid_argument("ci.abs and ci.rel must be >= 0");

    return c;
}
