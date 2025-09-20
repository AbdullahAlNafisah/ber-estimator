// =========================
// channel.cpp
// =========================
#include "channel.hpp"
#include <random>
#include <cmath>

// AWGN: y = s + n, gain = 1
ChannelOutput AwgnChannel::transmit(double s, std::mt19937_64 &rng, double sigma) const
{
    std::normal_distribution<double> n(0.0, sigma);
    return {s + n(rng), 1.0};
}

// Real-valued Rayleigh fading: y = h*s + n, with h ~ |N(0,1)|, report gain=h
ChannelOutput RayleighChannel::transmit(double s, std::mt19937_64 &rng, double /*sigma*/) const
{
    // Draw fading first; noise is added by caller after equalization (see simulation.cpp)
    std::normal_distribution<double> g(0.0, 1.0);
    const double h = std::abs(g(rng));
    return {h * s, h};
}
