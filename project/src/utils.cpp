// =========================
// utils.cpp
// =========================
#include "utils.hpp"
#include <chrono>
#include <random>
#include <cmath>

// Choose a seed: if requested==0, synthesize one from time and random_device
uint64_t make_seed(uint64_t requested)
{
    if (requested != 0)
        return requested;
    const auto t = (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::random_device rd; // may be deterministic on some systems; XOR with time is fine here
    return t ^ (uint64_t(rd()) << 1);
}

// Build an inclusive grid: start, start+step, ..., stop
std::vector<double> make_snr_grid(double start_db, double stop_db, double step_db)
{
    const int n = (int)std::floor((stop_db - start_db) / step_db + 0.5) + 1;
    std::vector<double> v;
    v.reserve(std::max(0, n));
    for (int i = 0; i < n; ++i)
        v.push_back(start_db + i * step_db);
    return v;
}
