// =========================
// coder.hpp
// =========================
#pragma once
#include <string>
#include <vector>

struct ICoder
{
    virtual ~ICoder() = default;
    virtual void encode(const std::vector<int> &u, std::vector<int> &c) const = 0;
    virtual void decode(const std::vector<int> &c_hat, std::vector<int> &u_hat) const = 0;
    virtual void decode_soft(const std::vector<double> &Lch, std::vector<int> &u_hat) const
    {
        (void)Lch;
        u_hat.clear();
    }
    virtual double rate() const = 0;
    virtual bool supports_soft() const { return false; }
};

// Uncoded passthrough coder
struct Uncoded : public ICoder
{
    void encode(const std::vector<int> &u, std::vector<int> &c) const override { c = u; }
    void decode(const std::vector<int> &c_hat, std::vector<int> &u_hat) const override { u_hat = c_hat; }
    double rate() const override { return 1.0; }
};

// Convolutional K=7, R=1/2 coder
struct ConvK7R12 : public ICoder
{
    void encode(const std::vector<int> &u, std::vector<int> &c) const override;
    void decode(const std::vector<int> &c_hat, std::vector<int> &u_hat) const override;
    double rate() const override { return 0.5; }
};
