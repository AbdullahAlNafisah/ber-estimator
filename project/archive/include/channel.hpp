// =========================
// channel.hpp
// =========================
#pragma once
#include <random>

struct ChannelOutput
{
    double y;    // received symbol
    double gain; // channel gain (for equalization)
};

struct IChannel
{
    virtual ~IChannel() = default;
    virtual ChannelOutput transmit(double s, std::mt19937_64 &rng, double sigma) const = 0;
};

struct AwgnChannel : public IChannel
{
    ChannelOutput transmit(double s, std::mt19937_64 &rng, double sigma) const override;
};

struct RayleighChannel : public IChannel
{
    ChannelOutput transmit(double s, std::mt19937_64 &rng, double sigma) const override;
};
