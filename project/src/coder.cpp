// =========================
// coder.cpp
// =========================
#include "coder.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <iostream>

#define VITERBI_DEBUG 0

// ---- helpers for Conv encoder/decoder ----
static inline int parity32(uint32_t x)
{
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    x &= 0xF;
    static const int p4[16] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};
    return p4[x];
}

// K=7 (memory M=6) generators in octal: g0=133, g1=171
static constexpr uint32_t G0 = 0b1011011;
static constexpr uint32_t G1 = 0b1111001;
static constexpr int M = 6;

void ConvK7R12::encode(const std::vector<int> &u, std::vector<int> &c) const
{
    c.clear();
    c.reserve(u.size() * 2 + 2 * M);
    uint32_t sr = 0; // shift register (bit0 newest)
    auto push = [&](int bit)
    {
        sr = ((sr << 1) | (bit & 1)) & ((1u << (M + 1)) - 1);
        const int v0 = parity32(sr & G0);
        const int v1 = parity32(sr & G1);
        c.push_back(v0);
        c.push_back(v1);
    };
    for (int b : u)
        push(b);
    for (int i = 0; i < M; ++i)
        push(0); // zero-termination
}

// Hard-decision Viterbi for R=1/2, K=7, zero-terminated
void ConvK7R12::decode(const std::vector<int> &c_hat, std::vector<int> &u_hat) const
{
    const size_t n_sym = c_hat.size() / 2;
    if (n_sym == 0)
    {
        u_hat.clear();
        return;
    }

    constexpr int NSTATE = 1 << M;
    constexpr int INF = 1e9;

    struct Trans
    {
        int next;
        int out;
    };
    Trans T0[NSTATE], T1[NSTATE];
    for (int s = 0; s < NSTATE; ++s)
    {
        // b=0
        {
            const uint32_t sr = ((uint32_t(s) << 1) | 0u) & ((1u << (M + 1)) - 1);
            const int v0 = parity32(sr & G0), v1 = parity32(sr & G1);
            T0[s] = {int(sr & ((1 << M) - 1)), (v0 << 1) | v1};
        }
        // b=1
        {
            const uint32_t sr = ((uint32_t(s) << 1) | 1u) & ((1u << (M + 1)) - 1);
            const int v0 = parity32(sr & G0), v1 = parity32(sr & G1);
            T1[s] = {int(sr & ((1 << M) - 1)), (v0 << 1) | v1};
        }
    }

    std::vector<int> pm_prev(NSTATE, INF), pm_curr(NSTATE, INF);
    std::vector<int16_t> pred(n_sym * NSTATE, -1);
    std::vector<uint8_t> dec(n_sym * NSTATE, 0);
    pm_prev[0] = 0;

    for (size_t t = 0; t < n_sym; ++t)
    {
        const int r = (c_hat[2 * t] << 1) | c_hat[2 * t + 1];
        std::fill(pm_curr.begin(), pm_curr.end(), INF);
        for (int s = 0; s < NSTATE; ++s)
        {
            const int pm = pm_prev[s];
            if (pm >= INF)
                continue;
            // b=0
            {
                const int ns = T0[s].next, ref = T0[s].out;
                const int dist = (((ref >> 1) & 1) ^ ((r >> 1) & 1)) + ((ref & 1) ^ (r & 1));
                const int m = pm + dist;
                if (m < pm_curr[ns])
                {
                    pm_curr[ns] = m;
                    pred[t * NSTATE + ns] = s;
                    dec[t * NSTATE + ns] = 0;
                }
            }
            // b=1
            {
                const int ns = T1[s].next, ref = T1[s].out;
                const int dist = (((ref >> 1) & 1) ^ ((r >> 1) & 1)) + ((ref & 1) ^ (r & 1));
                const int m = pm + dist;
                if (m < pm_curr[ns])
                {
                    pm_curr[ns] = m;
                    pred[t * NSTATE + ns] = s;
                    dec[t * NSTATE + ns] = 1;
                }
            }
        }
        pm_prev.swap(pm_curr);
    }

    int state = 0; // prefer zero state due to termination
#if VITERBI_DEBUG
    std::cerr << "[VITERBI] final metric at state 0: " << pm_prev[state] << "\n";
#endif

    const size_t K = (n_sym > size_t(M)) ? (n_sym - M) : 0;
    u_hat.assign(K, 0);
    for (size_t t = n_sym; t-- > 0;)
    {
        const int b = dec[t * NSTATE + state];
        const int p = pred[t * NSTATE + state];
        if (t < K)
            u_hat[t] = b;
        state = (p >= 0) ? p : 0;
    }
#if VITERBI_DEBUG
    std::cerr << "[VITERBI] traceback done. u_hat size=" << u_hat.size() << "\n";
#endif
}
