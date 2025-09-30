// =========================
// utils.hpp
// =========================
#pragma once
#include <cstdint>
#include <vector>

uint64_t make_seed(uint64_t requested);
std::vector<double> make_snr_grid(double start_db, double stop_db, double step_db);
