// =========================
// config.hpp
// =========================
#pragma once
#include <string>
#include <cstdint>

struct Config
{
    // Core parameters
    double snr_start_db;
    double snr_stop_db;
    double snr_step_db;

    uint64_t min_errors;
    uint64_t max_bits;
    double ber_floor;

    std::string outfile;
    uint64_t seed;

    std::string modem;
    std::string channel;
    std::string coder;
    int frame_len;

    double ci_level;
    double ci_abs;
    double ci_rel;
    uint64_t ci_min_bits;

    unsigned threads;

    static Config load(const std::string &path = "../config.ini");
};
