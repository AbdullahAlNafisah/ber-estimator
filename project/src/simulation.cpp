// =========================
// simulation.cpp
// =========================
#include "simulation.hpp"
#include "modem.hpp"
#include "channel.hpp"
#include "coder.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include <tuple>
#include <vector>

// Inverse standard normal CDF (Acklam approximation)
static double inv_norm_cdf(double p)
{
    if (p <= 0.0 || p >= 1.0)
        return std::numeric_limits<double>::quiet_NaN();
    static const double a1 = -39.69683028665376, a2 = 220.9460984245205, a3 = -275.9285104469687,
                        a4 = 138.3577518672690, a5 = -30.66479806614716, a6 = 2.506628277459239;
    static const double b1 = -54.47609879822406, b2 = 161.5858368580409, b3 = -155.6989798598866,
                        b4 = 66.80131188771972, b5 = -13.28068155288572;
    static const double c1 = -7.784894002430293e-03, c2 = -3.223964580411365e-01, c3 = -2.400758277161838,
                        c4 = -2.549732539343734, c5 = 4.374664141464968, c6 = 2.938163982698783;
    static const double d1 = 7.784695709041462e-03, d2 = 3.224671290700398e-01, d3 = 2.445134137142996,
                        d4 = 3.754408661907416;
    const double plow = 0.02425, phigh = 1 - plow;
    double q, r;
    if (p < plow)
    {
        q = std::sqrt(-2 * std::log(p));
        return (((((c1 * q + c2) * q + c3) * q + c4) * q + c5) * q + c6) / (((((d1 * q + d2) * q + d3) * q + d4) * q) + 1);
    }
    else if (p > phigh)
    {
        q = std::sqrt(-2 * std::log(1 - p));
        return -(((((c1 * q + c2) * q + c3) * q + c4) * q + c5) * q + c6) / (((((d1 * q + d2) * q + d3) * q + d4) * q) + 1);
    }
    else
    {
        q = p - 0.5;
        r = q * q;
        return (((((a1 * r + a2) * r + a3) * r + a4) * r + a5) * r + a6) * q / (((((b1 * r + b2) * r + b3) * r + b4) * r + b5) * r + 1);
    }
}

// Wilson score interval for a proportion (errors/bits)
static inline std::tuple<double, double, double> wilson_ci(uint64_t errs, uint64_t bits, double z)
{
    if (bits == 0)
        return {0.0, 1.0, 0.5};
    const double n = (double)bits, p = (double)errs / n, z2 = z * z;
    const double denom = 1.0 + z2 / n;
    const double center = (p + z2 / (2 * n)) / denom;
    const double rad = z * std::sqrt((p * (1.0 - p)) / n + z2 / (4 * n * n)) / denom;
    return {std::max(0.0, center - rad), std::min(1.0, center + rad), rad};
}

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
                             double ber_floor)
{
    const double R = coder.rate();
    const int m = modem.bits_per_symbol();
    const double Es = modem.symbol_energy();
    const double ebn0_lin = std::pow(10.0, ebn0_db / 10.0);
    const double N0 = Es / (R * m * ebn0_lin);
    const double sigma = std::sqrt(0.5 * N0);

    const double alpha = 1.0 - ci_level;
    const double z = (ci_level > 0.0 && ci_level < 1.0) ? inv_norm_cdf(1.0 - alpha / 2.0) : 0.0;

    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0)
        hw = 1;
    if (n_threads == 0)
        n_threads = hw;

    std::atomic<uint64_t> total_err{0}, total_bits{0};
    std::atomic<bool> stop{false};

    auto ci_goals_met = [&](uint64_t bits, uint64_t errs)
    {
        if ((ci_abs <= 0.0) && (ci_rel <= 0.0))
            return true; // CI disabled
        if (bits == 0 || bits < ci_min_bits)
            return false;
        auto [lo, hi, half] = wilson_ci(errs, bits, z);
        (void)lo;
        (void)hi;
        const double p = (bits > 0) ? double(errs) / double(bits) : 0.0;
        bool ok_abs = (ci_abs <= 0.0) || (half <= ci_abs);
        bool ok_rel = (ci_rel <= 0.0) || (half <= ci_rel * std::max(p, 1e-12));
        return ok_abs && ok_rel;
    };
    auto floor_met = [&](uint64_t bits, uint64_t errs)
    {
        if (ber_floor <= 0.0)
            return false;
        if (bits == 0 || bits < ci_min_bits)
            return false;
        auto [lo, hi, half] = wilson_ci(errs, bits, z);
        (void)lo;
        (void)half;
        return (hi <= ber_floor);
    };

    // Per-thread seeds
    std::vector<uint64_t> seeds(n_threads);
    const uint64_t base = rng();
    for (unsigned t = 0; t < n_threads; ++t)
        seeds[t] = base ^ (0x9E3779B97F4A7C15ULL * (t + 1));

    auto worker = [&](unsigned tid)
    {
        std::mt19937_64 trng(seeds[tid]);
        std::bernoulli_distribution bitgen(0.5);

        std::vector<int> u, c, c_hat, u_hat;
        u.reserve(frame_len_bits);
        std::vector<double> llr;
        llr.reserve(frame_len_bits * 2);

        while (!stop.load(std::memory_order_relaxed))
        {
            // 1) Info bits
            u.clear();
            for (int i = 0; i < frame_len_bits; ++i)
                u.push_back(bitgen(trng));

            // 2) Encode
            coder.encode(u, c);

            // 3) Modulate → channel → (equalize) → demodulate
            c_hat.clear();
            llr.clear();
            const int mbs = modem.bits_per_symbol();
            size_t i = 0;
            while (i < c.size())
            {
                int inbits[8] = {0};
                for (int k = 0; k < mbs; ++k)
                    if (i + k < c.size())
                        inbits[k] = c[i + k];
                const double s = modem.modulate(inbits);

                // Draw fading gain and apply; noise added implicitly via demodulation using sigma
                const auto ch = channel.transmit(s, trng, sigma); // thread-local RNG (fixes data race)

                // Equalize
                const double g = (ch.gain > 0.0) ? ch.gain : 1.0;
                const double r_eq = (ch.gain > 0.0) ? (ch.y / ch.gain) : ch.y;
                const double sigma2_eq = (sigma * sigma) / (g * g);

                if (coder.supports_soft())
                {
                    double L[8] = {0};
                    modem.demodulate_llr(r_eq, sigma2_eq, L);
                    for (int k = 0; k < mbs; ++k)
                        if (i + k < c.size())
                            llr.push_back(L[k]);
                }
                else
                {
                    int outbits[8] = {0};
                    modem.demodulate(r_eq, outbits);
                    for (int k = 0; k < mbs; ++k)
                        if (i + k < c.size())
                            c_hat.push_back(outbits[k]);
                }
                i += mbs;
            }

            // 4) Decode
            if (coder.supports_soft())
                coder.decode_soft(llr, u_hat);
            else
                coder.decode(c_hat, u_hat);

            // 5) Count errors on info bits
            const size_t L = std::min(u.size(), u_hat.size());
            uint64_t local_err = 0;
            for (size_t j = 0; j < L; ++j)
                local_err += (u[j] != u_hat[j]);

            // Publish
            uint64_t bits_after = total_bits.fetch_add(L, std::memory_order_relaxed) + L;
            uint64_t errs_after = total_err.fetch_add(local_err, std::memory_order_relaxed) + local_err;

            bool stop_by_max = (max_bits > 0 && bits_after >= max_bits);
            bool stop_by_floor = floor_met(bits_after, errs_after);
            bool stop_by_ci = ((min_errors == 0 || errs_after >= min_errors) && ci_goals_met(bits_after, errs_after));
            if (stop_by_max || stop_by_floor || stop_by_ci)
            {
                stop.store(true, std::memory_order_relaxed);
                break;
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(n_threads);
    for (unsigned t = 0; t < n_threads; ++t)
        pool.emplace_back(worker, t);
    for (auto &th : pool)
        th.join();

    auto [lo, hi, half] = ((ci_abs > 0.0 || ci_rel > 0.0) && total_bits > 0 && z > 0.0)
                              ? wilson_ci(total_err.load(), total_bits.load(), z)
                              : std::tuple<double, double, double>{0.0, 0.0, 0.0};

    const double ber = (total_bits > 0) ? double(total_err.load()) / double(total_bits.load()) : 0.0;
    return {ber, total_bits.load(), total_err.load(), lo, hi};
}
