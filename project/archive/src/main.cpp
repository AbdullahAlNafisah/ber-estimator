// ------------------------------
// Standard library headers
// ------------------------------
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <ctime>

// ------------------------------
// Project headers
// ------------------------------
#include "config.hpp"
#include "simulation.hpp"
#include "utils.hpp"
#include "modem.hpp"
#include "channel.hpp"
#include "coder.hpp"

namespace
{

    // Replace every occurrence of "from" in s with "to"
    void replace_all(std::string &s, const std::string &from, const std::string &to)
    {
        if (from.empty())
            return;
        std::size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos)
        {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    // Ensure parent dir exists and return cfg.outfile
    static std::string resolve_output_path(const Config &cfg)
    {
        namespace fs = std::filesystem;

        // Treat outfile as directory if it ends with '/' or '\' OR is an existing dir
        const std::string &f = cfg.outfile;
        const bool ends_with_slash = (!f.empty() && (f.back() == '/' || f.back() == '\\'));
        const bool is_dir = std::filesystem::exists(f) && std::filesystem::is_directory(f);

        // Helper: filesystem-safe token
        auto slug = [](std::string s)
        {
            for (auto &ch : s)
            {
                unsigned char u = static_cast<unsigned char>(ch);
                if (std::isalnum(u))
                    ch = static_cast<char>(std::tolower(u));
                else if (ch == '.' || ch == '-' || ch == '_')
                { /* keep */
                }
                else
                    ch = '_';
            }
            return s;
        };

        if (ends_with_slash || is_dir)
        {
            fs::path dir = fs::path(f);
            // Ensure directory exists
            if (!dir.empty())
                fs::create_directories(dir);

            // Auto filename: coder_modem_channel.csv
            const std::string name =
                slug(cfg.coder) + "_" + slug(cfg.modem) + "_" + slug(cfg.channel) + ".csv";

            return (dir / name).string();
        }

        // Otherwise: treat as a literal file path; ensure parent dir exists
        fs::path p{f};
        if (!p.parent_path().empty())
            fs::create_directories(p.parent_path());
        return p.string();
    }

    // Select modem implementation by name
    static std::unique_ptr<IModem> make_modem(const std::string &name)
    {
        std::string s = name;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);

        if (s == "ask2" || s == "ask2")
            return std::make_unique<Ask2Modem>();

        if (s == "ask4" || s == "ask4_gray")
        {
            return std::make_unique<Ask4Modem>(Ask4Modem::Mapping::Gray);
        }
        if (s == "ask4_natural" || s == "ask4_binary" || s == "ask4_nogray")
        {
            return std::make_unique<Ask4Modem>(Ask4Modem::Mapping::Natural);
        }

        throw std::invalid_argument("Unknown modem: " + name);
    }

    // Select channel implementation by name
    std::unique_ptr<IChannel> make_channel(const std::string &name)
    {
        std::string s = name;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        if (s == "awgn")
            return std::make_unique<AwgnChannel>();
        if (s == "rayleigh")
            return std::make_unique<RayleighChannel>();
        throw std::invalid_argument("Unknown channel: " + name);
    }

    // Select coder implementation from config
    std::unique_ptr<ICoder> make_coder(const Config &cfg)
    {
        std::string s = cfg.coder;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        if (s == "uncoded")
            return std::make_unique<Uncoded>();
        if (s == "conv_k7_r12")
            return std::make_unique<ConvK7R12>();
        throw std::invalid_argument("Unknown coder: " + cfg.coder);
    }

} // end anonymous namespace

int main()
try
{
    // Load configuration (throws if required keys are missing)
    const Config cfg = Config::load();

    // Create components from config
    auto modem = make_modem(cfg.modem);
    auto channel = make_channel(cfg.channel);
    auto coder = make_coder(cfg);

    // Seed RNG
    std::mt19937_64 rng(make_seed(cfg.seed));

    // Decide thread count
    unsigned nthreads = cfg.threads;
    if (nthreads == 0)
    {
        nthreads = std::thread::hardware_concurrency();
        if (nthreads == 0)
            nthreads = 1;
    }
    std::cout << "Using " << nthreads << " threads\n";

    // Resolve output path and open file
    const std::string out_path = resolve_output_path(cfg);
    std::ofstream ofs(out_path);
    if (!ofs)
    {
        std::cerr << "Error: cannot open output file: " << out_path << "\n";
        return 2;
    }

    // Write CSV header
    ofs << std::fixed << std::setprecision(6);
    ofs << "snr_db,ber,num_bits,num_errors,ci_low,ci_high\n";
    std::cout << "Saving results to: " << out_path << "\n";

    // Number of SNR points (inclusive)
    const int n = static_cast<int>(std::floor((cfg.snr_stop_db - cfg.snr_start_db) / cfg.snr_step_db + 0.5)) + 1;

    for (int i = 0; i < n; ++i)
    {
        const double snr_db = cfg.snr_start_db + i * cfg.snr_step_db;

        // Run simulation at this SNR
        const BerResult r = simulate_framewise(
            snr_db, cfg.min_errors, cfg.max_bits, cfg.frame_len,
            *modem, *channel, *coder,
            cfg.ci_level, cfg.ci_abs, cfg.ci_rel, cfg.ci_min_bits,
            rng, nthreads, cfg.ber_floor);

        // Append one CSV line
        ofs << snr_db << "," << r.ber << "," << r.bits << "," << r.errs
            << "," << r.ci_lo << "," << r.ci_hi << "\n";

        // Print progress to console
        std::cout << std::fixed
                  << "SNR(dB)=" << std::setw(6) << std::setprecision(2) << snr_db
                  << "  BER=" << std::setprecision(6) << r.ber
                  << "  bits=" << r.bits
                  << "  errors=" << r.errs << "\n";

        // Stop early if BER floor reached
        const double ber_for_stop = (r.ci_hi > 0.0 ? r.ci_hi : r.ber);
        if (cfg.ber_floor > 0.0 && ber_for_stop <= cfg.ber_floor)
        {
            std::cout << "Stopping sweep early: BER floor reached at SNR=" << snr_db << " dB\n";
            break;
        }
    }

    return 0;
}
catch (const std::exception &e)
{
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
}
