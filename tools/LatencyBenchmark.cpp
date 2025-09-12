#include "Communicator.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

struct Args {
    std::string address = "127.0.0.1";
    int base = 10000;
    size_t size = 1 << 20; // 1MB
    int iters = 20;
    // Optional: measured network characteristics to compute theoretical latency
    double rtt_ms = -1.0;          // ping RTT in milliseconds
    double bandwidth_gbps = -1.0;  // iperf3 throughput in Gbps
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : ""; };
        if (s == "--address") a.address = next();
        else if (s == "--base") a.base = std::stoi(next());
        else if (s == "--size") a.size = static_cast<size_t>(std::stoll(next()));
        else if (s == "--iters") a.iters = std::stoi(next());
        else if (s == "--rtt_ms") a.rtt_ms = std::stod(next());
        else if (s == "--bandwidth_gbps") a.bandwidth_gbps = std::stod(next());
        else if (s == "-h" || s == "--help") {
            std::cout << "Usage: LatencyBenchmark [--address 127.0.0.1] [--base 10000] [--size 1048576] [--iters 20]\\n"
                         "                          [--rtt_ms <ms>] [--bandwidth_gbps <Gbps>]\\n";
            std::exit(0);
        }
    }
    return a;
}

int main(int argc, char** argv) {
    auto args = parseArgs(argc, argv);
    std::cout << "Latency benchmark\n"
              << " address=" << args.address
              << " base=" << args.base
              << " size=" << args.size
              << " iters=" << args.iters << "\n";

    // Party A (router id=1), Party B (dealer id=2)
    Communicator router{1, args.base, args.address, 2};
    Communicator dealer{2, args.base, args.address, 2};
    router.setUpRouter();
    dealer.setUpRouterDealer();

    // Prepare random payload (binary-safe)
    std::string payload;
    payload.resize(args.size);
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, 255);
    for (size_t i = 0; i < args.size; ++i) payload[i] = static_cast<char>(dist(rng));

    // Warm-up
    for (int i = 0; i < 3; ++i) {
        dealer.dealerSendTo(1, payload);
        std::string from, recv;
        router.routerReceive(from, recv, 1000);
    }

    std::vector<double> times_ms;
    times_ms.reserve(args.iters);

    for (int i = 0; i < args.iters; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        if (!dealer.dealerSendTo(1, payload)) {
            std::cerr << "send failed at iter " << i << "\n";
            return 2;
        }
        std::string from, recv;
        if (!router.routerReceive(from, recv, 5000)) {
            std::cerr << "receive timeout at iter " << i << "\n";
            return 3;
        }
        auto t1 = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double, std::milli>(t1 - t0).count();
        times_ms.push_back(dt);
    }

    auto copy = times_ms;
    std::sort(copy.begin(), copy.end());
    auto pct = [&](double p) {
        if (copy.empty()) return 0.0;
        size_t idx = static_cast<size_t>(p * (copy.size() - 1));
        return copy[idx];
    };

    double sum = 0.0;
    for (double v : times_ms) sum += v;
    double avg = sum / times_ms.size();
    double med = pct(0.5);
    double p95 = pct(0.95);

    double avg_s = avg / 1000.0;
    double throughput_MBps = (args.size / (1024.0 * 1024.0)) / avg_s;

    std::cout << std::fixed << std::setprecision(3)
              << "Results (DEALER->ROUTER, one-way, same process)\n"
              << " avg_ms=" << avg
              << " med_ms=" << med
              << " p95_ms=" << p95
              << " throughput_MBps~=" << throughput_MBps << "\n";

    // If user provided RTT and bandwidth, compute theoretical one-way
    if (args.rtt_ms > 0.0 && args.bandwidth_gbps > 0.0) {
        // Transfer time for one message: bits / (Gbps * 1e9) seconds
        const double bits = static_cast<double>(args.size) * 8.0;
        const double xfer_ms = (bits / (args.bandwidth_gbps * 1e9)) * 1000.0;
        const double theory_ms = (args.rtt_ms / 2.0) + xfer_ms;
        const double delta_ms = avg - theory_ms;
        const double overhead_pct = theory_ms > 0.0 ? (delta_ms / theory_ms) * 100.0 : 0.0;

        std::cout << std::fixed << std::setprecision(3)
                  << "Theoretical (one-way) = RTT/2 + size/bw = "
                  << args.rtt_ms/2.0 << " + " << xfer_ms << " = " << theory_ms << " ms\n"
                  << "Delta (measured - theoretical) = " << delta_ms << " ms"
                  << " (" << overhead_pct << "%)\n";
    } else {
        std::cout << "Theoretical one-way ~= RTT/2 + size/bandwidth\n"
                  << " Provide --rtt_ms and --bandwidth_gbps to compute delta.\n"
                  << " Example RTT: ping -c 5 " << args.address << "  (avg rtt)\n"
                  << " Example BW: iperf3 -s (server), iperf3 -c " << args.address
                  << " -n " << (args.size * args.iters) << " (throughput)\n";
    }

    return 0;
}
