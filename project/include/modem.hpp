// =========================
// modem.hpp
// =========================
#pragma once
#include <vector>
#include <cmath>

struct IModem
{
    virtual ~IModem() = default;
    virtual double modulate(const int bits[]) const = 0;
    virtual void demodulate(double r, int bits_out[]) const = 0;
    virtual void demodulate_llr(double r, double sigma2, double L[]) const = 0;
    virtual int bits_per_symbol() const = 0;
    virtual double symbol_energy() const = 0;
};

// 2-ASK modem
struct Ask2Modem : public IModem
{
    double modulate(const int bits[]) const override { return bits[0] ? -1.0 : +1.0; }
    void demodulate(double r, int bits_out[]) const override { bits_out[0] = (r < 0.0) ? 1 : 0; }
    void demodulate_llr(double r, double sigma2, double L[]) const override { L[0] = 2.0 * r / sigma2; }
    int bits_per_symbol() const override { return 1; }
    double symbol_energy() const override { return 1.0; }
};

// 4-ASK modem: symbols {-3, -1, +1, +3}
struct Ask4Modem : public IModem
{
    enum class Mapping
    {
        Gray,
        Natural
    };

    explicit Ask4Modem(Mapping m = Mapping::Gray) : mapping(m) {}

    double modulate(const int bits[]) const override
    {
        const int b0 = bits[0], b1 = bits[1];
        const int val = (b0 << 1) | b1;

        if (mapping == Mapping::Gray)
        {
            // Gray: 00->-3, 01->-1, 11->+1, 10->+3
            switch (val)
            {
            case 0:
                return -3.0;
            case 1:
                return -1.0;
            case 3:
                return +1.0;
            case 2:
                return +3.0;
            }
        }
        else
        {
            // Natural: 00->-3, 01->-1, 10->+1, 11->+3
            switch (val)
            {
            case 0:
                return -3.0;
            case 1:
                return -1.0;
            case 2:
                return +1.0;
            case 3:
                return +3.0;
            }
        }
        return 0.0;
    }
    void demodulate(double r, int bits_out[]) const override
    {
        // thresholds at -2, 0, +2 → sym 0:-3, 1:-1, 2:+1, 3:+3
        int sym = (r < -2.0) ? 0 : (r < 0.0) ? 1
                               : (r < 2.0)   ? 2
                                             : 3;

        if (mapping == Mapping::Gray)
        {
            // Gray: 00->-3, 01->-1, 11->+1, 10->+3
            switch (sym)
            {
            case 0:
                bits_out[0] = 0;
                bits_out[1] = 0;
                break; // -3 → 00
            case 1:
                bits_out[0] = 0;
                bits_out[1] = 1;
                break; // -1 → 01
            case 2:
                bits_out[0] = 1;
                bits_out[1] = 1;
                break; // +1 → 11   <-- FIX
            case 3:
                bits_out[0] = 1;
                bits_out[1] = 0;
                break; // +3 → 10   <-- FIX
            }
        }
        else
        {
            // Natural: 00->-3, 01->-1, 10->+1, 11->+3
            switch (sym)
            {
            case 0:
                bits_out[0] = 0;
                bits_out[1] = 0;
                break;
            case 1:
                bits_out[0] = 0;
                bits_out[1] = 1;
                break;
            case 2:
                bits_out[0] = 1;
                bits_out[1] = 0;
                break;
            case 3:
                bits_out[0] = 1;
                bits_out[1] = 1;
                break;
            }
        }
    }

    void demodulate_llr(double r, double sigma2, double L[]) const override
    {
        // Likelihoods for symbols at [-3, -1, +1, +3]
        const double d0 = std::exp(-std::pow(r + 3, 2) / (2 * sigma2)); // -3
        const double d1 = std::exp(-std::pow(r + 1, 2) / (2 * sigma2)); // -1
        const double d2 = std::exp(-std::pow(r - 1, 2) / (2 * sigma2)); // +1
        const double d3 = std::exp(-std::pow(r - 3, 2) / (2 * sigma2)); // +3

        const double P[4] = {d0, d1, d2, d3}; // [-3, -1, +1, +3]

        if (mapping == Mapping::Gray)
        {
            // MSB: {-3,-1} vs {+1,+3}
            L[0] = std::log((P[0] + P[1]) / (P[2] + P[3]));
            // LSB: {-3,+3} vs {-1,+1}
            L[1] = std::log((P[0] + P[3]) / (P[1] + P[2])); // <-- ensure this, not (P[0]+P[2])/(P[1]+P[3])
        }
        else
        {
            // Natural mapping:
            // MSB: {-3,-1} vs {+1,+3}
            L[0] = std::log((P[0] + P[1]) / (P[2] + P[3]));
            // LSB: {-3,+1} vs {-1,+3}
            L[1] = std::log((P[0] + P[2]) / (P[1] + P[3]));
        }
    }

    int bits_per_symbol() const override { return 2; }
    double symbol_energy() const override { return 5.0; }

private:
    Mapping mapping;
};
