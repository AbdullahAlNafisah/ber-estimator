// pluto_txrx_csv.cpp
// Minimal demo: transmit random binary (BPSK on Q by default), receive I/Q,
// and save raw samples to CSV for offline processing.
//
// Build:
//   g++ -O2 -std=c++17 pluto_txrx_csv.cpp -liio -o pluto_txrx_csv
//
// Notes:
// - Devices (PlutoSDR):
//     * PHY control dev: "ad9361-phy"
//     * RX stream dev:   "cf-ad9361-lpc"
//     * TX stream dev:   "cf-ad9361-dds-core-lpc"
//     * Channels: "voltage0" (I), "voltage1" (Q)

#include <iio.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <vector>

static void fatal(const std::string& msg) {
    std::cerr << "ERROR: " << msg << std::endl;
    std::exit(1);
}

static void write_attr_ll_dbg(struct iio_channel* ch, const char* attr, long long val) {
    int ret = iio_channel_attr_write_longlong(ch, attr, val);
    if (ret < 0) {
        char buf[128];
        iio_strerror(-ret, buf, sizeof(buf));
        std::cerr << "ERROR: write " << attr << "=" << val
                  << " -> " << buf << " (" << ret << ")\n";
        std::exit(1);
    }
}

static void write_attr_str_dbg(struct iio_channel* ch, const char* attr, const char* val) {
    int ret = iio_channel_attr_write(ch, attr, val);
    if (ret < 0) {
        char buf[128];
        iio_strerror(-ret, buf, sizeof(buf));
        std::cerr << "ERROR: write " << attr << "=" << val
                  << " -> " << buf << " (" << ret << ")\n";
        std::exit(1);
    }
}

int main() {
    // ---------- User settings ----------
    const char*     URI          = "usb:1.6.5";     // e.g., "usb:1.5.5" or "ip:192.168.2.1"
    const long long SAMPLE_RATE  = 3840000;         // 3.84 MSPS (supported by DMA cores)
    const long long RX_LO_HZ     = 2400000000LL;    // 2.4 GHz
    const long long TX_LO_HZ     = 2400000000LL;    // 2.4 GHz
    const size_t    NSAMPLES     = 16384;           // complex samples to send/receive
    const int16_t   AMP          = 100;             // TX symbol amplitude (reduce if RX clips)
    const std::string CSV_PATH   = "../samples.csv";

    // ---------- Create IIO context ----------
    iio_context* ctx = iio_create_context_from_uri(URI);
    if (!ctx) fatal("Failed to create IIO context. Is the Pluto attached and permissions ok?");

    // ---------- Find devices ----------
    iio_device* phy = iio_context_find_device(ctx, "ad9361-phy");
    if (!phy) fatal("Device 'ad9361-phy' not found");

    iio_device* rx = iio_context_find_device(ctx, "cf-ad9361-lpc");
    if (!rx) fatal("Device 'cf-ad9361-lpc' (RX) not found");

    iio_device* tx = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
    if (!tx) fatal("Device 'cf-ad9361-dds-core-lpc' (TX) not found");

    // ---------- Configure LO and baseband via PHY ----------
    iio_channel* rx_lo = iio_device_find_channel(phy, "altvoltage0", true); // RX LO
    iio_channel* tx_lo = iio_device_find_channel(phy, "altvoltage1", true); // TX LO
    if (!rx_lo || !tx_lo) fatal("Failed to find LO channels on ad9361-phy");

    write_attr_ll_dbg(rx_lo, "frequency", RX_LO_HZ);
    write_attr_ll_dbg(tx_lo, "frequency", TX_LO_HZ);

    iio_channel* rx_bb = iio_device_find_channel(phy, "voltage0", false); // RX baseband ctrl
    iio_channel* tx_bb = iio_device_find_channel(phy, "voltage0", true);  // TX baseband ctrl
    if (!rx_bb || !tx_bb) fatal("Failed to find baseband channels on ad9361-phy");

    // Optional RX gain control (uncomment ONE of the following):
    // write_attr_str_dbg(rx_bb, "gain_control_mode", "slow_attack");
    // write_attr_str_dbg(rx_bb, "gain_control_mode", "manual");
    // write_attr_str_dbg(rx_bb, "hardwaregain", "0"); // valid when "manual"

    // Optional: lower TX analog power a lot (more negative = less power)
    // write_attr_str_dbg(tx_bb, "hardwaregain", "-70"); // dB

    // Shared Pluto rate: set once on RX baseband
    write_attr_ll_dbg(rx_bb, "sampling_frequency", SAMPLE_RATE);

    // Keep RX/TX RF bandwidth consistent with sample rate
    write_attr_ll_dbg(rx_bb, "rf_bandwidth", 5000000);
    write_attr_ll_dbg(tx_bb, "rf_bandwidth", 5000000);

    // ---------- Disable TX DDS test tones ----------
    for (const char* tone : {"altvoltage0","altvoltage1","altvoltage2","altvoltage3"}) {
        iio_channel* ch = iio_device_find_channel(tx, tone, true);
        if (ch) write_attr_str_dbg(ch, "raw", "0");
    }

    // ---------- Prepare RX channels & buffer ----------
    iio_channel* rx_i = iio_device_find_channel(rx, "voltage0", false); // I
    iio_channel* rx_q = iio_device_find_channel(rx, "voltage1", false); // Q
    if (!rx_i || !rx_q) fatal("RX I/Q channels not found");
    iio_channel_enable(rx_i);
    iio_channel_enable(rx_q);

    const size_t RX_BUF_SAMPLES = 4096; // complex samples per RX buffer
    iio_buffer* rxbuf = iio_device_create_buffer(rx, RX_BUF_SAMPLES, false);
    if (!rxbuf) fatal("Could not create RX buffer");

    // ---------- Prepare TX channels & buffer ----------
    iio_channel* tx_i_ch = iio_device_find_channel(tx, "voltage0", true); // I
    iio_channel* tx_q_ch = iio_device_find_channel(tx, "voltage1", true); // Q
    if (!tx_i_ch || !tx_q_ch) fatal("TX I/Q channels not found");
    iio_channel_enable(tx_i_ch);
    iio_channel_enable(tx_q_ch);

    const size_t TX_BUF_SAMPLES = 4096; // complex samples per TX buffer
    iio_buffer* txbuf = iio_device_create_buffer(tx, TX_BUF_SAMPLES, false);
    if (!txbuf) fatal("Could not create TX buffer");

    // ---------- Generate and stream random BPSK on Q (I=0, Q=±AMP) ----------
    std::mt19937 rng(42);
    std::bernoulli_distribution bitdist(0.5);

    std::vector<int16_t> all_tx_i; all_tx_i.reserve(NSAMPLES);
    std::vector<int16_t> all_tx_q; all_tx_q.reserve(NSAMPLES);
    std::vector<int16_t> all_rx_i; all_rx_i.reserve(NSAMPLES);
    std::vector<int16_t> all_rx_q; all_rx_q.reserve(NSAMPLES);

    size_t total_sent = 0, total_recv = 0;

    while (total_sent < NSAMPLES || total_recv < NSAMPLES) {
        // ---- TX: fill buffer with random BPSK symbols on Q ----
        if (total_sent < NSAMPLES) {
            void* tx_start = iio_buffer_first(txbuf, tx_i_ch);
            void* tx_end   = iio_buffer_end(txbuf);
            ptrdiff_t inc  = iio_buffer_step(txbuf);

            uint8_t* p = static_cast<uint8_t*>(tx_start);
            for (; p < tx_end && total_sent < NSAMPLES; p += inc) {
                int16_t* s = reinterpret_cast<int16_t*>(p);
                const int bit_I = bitdist(rng) ? 1 : 0;
                const int bit_Q = bitdist(rng) ? 1 : 0;
                const int16_t i_val = bit_I ? AMP : -AMP;
                const int16_t q_val = bit_Q ? AMP : -AMP;
                s[0] = i_val; // I
                s[1] = q_val; // Q
                all_tx_i.push_back(i_val);
                all_tx_q.push_back(q_val);
                total_sent++;
            }
            if (iio_buffer_push(txbuf) < 0) fatal("iio_buffer_push(tx) failed");
        }

        // ---- RX: pull a buffer and copy samples ----
        if (total_recv < NSAMPLES) {
            if (iio_buffer_refill(rxbuf) < 0) fatal("iio_buffer_refill(rx) failed");

            void* rx_start = iio_buffer_first(rxbuf, rx_i);
            void* rx_end   = iio_buffer_end(rxbuf);
            ptrdiff_t inc  = iio_buffer_step(rxbuf);

            uint8_t* p = static_cast<uint8_t*>(rx_start);
            for (; p < rx_end && total_recv < NSAMPLES; p += inc) {
                const int16_t* s = reinterpret_cast<const int16_t*>(p);
                all_rx_i.push_back(s[0]);
                all_rx_q.push_back(s[1]);
                total_recv++;
            }
        }
    }

    // ---------- Clean up streaming ----------
    iio_buffer_destroy(txbuf);
    iio_buffer_destroy(rxbuf);
    iio_channel_disable(tx_i_ch);
    iio_channel_disable(tx_q_ch);
    iio_channel_disable(rx_i);
    iio_channel_disable(rx_q);

    // ---------- Write CSV (raw only): n,tx_i,tx_q,rx_i,rx_q ----------
    std::ofstream ofs(CSV_PATH);
    if (!ofs) fatal("Failed to open CSV for writing");
    ofs << "n,tx_i,tx_q,rx_i,rx_q\n";
    for (size_t n = 0; n < NSAMPLES; ++n) {
        const int16_t txi = (n < all_tx_i.size()) ? all_tx_i[n] : 0;
        const int16_t txq = (n < all_tx_q.size()) ? all_tx_q[n] : 0;
        const int16_t rxi = (n < all_rx_i.size()) ? all_rx_i[n] : 0;
        const int16_t rxq = (n < all_rx_q.size()) ? all_rx_q[n] : 0;
        ofs << n << "," << txi << "," << txq << "," << rxi << "," << rxq << "\n";
    }
    ofs.close();

    // ---------- Destroy context ----------
    iio_context_destroy(ctx);

    std::cout << "Done. Wrote " << CSV_PATH
              << " with " << NSAMPLES << " samples." << std::endl;
    return 0;
}
