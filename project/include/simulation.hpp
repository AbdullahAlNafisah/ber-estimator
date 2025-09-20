// =========================
// simulation.hpp
// =========================
#pragma once
#include <cstdint>
#include <random>

struct IModem;
struct IChannel;
struct ICoder;

struct BerResult
{
    double ber;
    uint64_t bits;
    uint64_t errs;
    double ci_lo;
    double ci_hi;
};

BerResult simulate_framewise(double ebn0_db,
                             uint64_t min_errors,
                             uint64_t max_bits,
                             int frame_len_bits,
                             const IModem &modem,
                             const IChannel &channel,
                             const ICoder &coder,
                             double ci_level,
                             double ci_abs,
                             double ci_rel,
                             uint64_t ci_min_bits,
                             std::mt19937_64 &rng,
                             unsigned n_threads,
                             double ber_floor);
