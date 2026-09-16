#include <pcap.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <csignal>
#include <iomanip>

struct MarketDataTick {
    uint64_t sequence;
    uint64_t timestamp_ns;
    char symbol[8];
    double price;
};

std::vector<double> rtt_values;
pcap_t *handle = nullptr;

// Signal handler to catch Ctrl+C and print statistics
void handle_sigint(int sig) {
    if (rtt_values.empty()) {
        std::cout << "\nNo packets captured. Exiting.\n";
        if (handle) pcap_close(handle);
        exit(0);
    }

    std::sort(rtt_values.begin(), rtt_values.end());
    size_t n = rtt_values.size();

    double sum = std::accumulate(rtt_values.begin(), rtt_values.end(), 0.0);
    double mean = sum / n;

    double sq_sum = std::inner_product(rtt_values.begin(), rtt_values.end(), rtt_values.begin(), 0.0);
    double stdev = std::sqrt(sq_sum / n - mean * mean);

    double p50 = rtt_values[n * 0.50];
    double p75 = rtt_values[n * 0.75];
    double p99 = rtt_values[n * 0.99];

    std::cout << "\n\n--- Network Latency Statistics ---\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Packets Captured : " << n << "\n";
    std::cout << "Absolute Min     : " << rtt_values.front() << " µs\n";
    std::cout << "Mean (Average)   : " << mean << " µs\n";
    std::cout << "Median (P50)     : " << p50 << " µs\n";
    std::cout << "75th Percentile  : " << p75 << " µs\n";
    std::cout << "99th Percentile  : " << p99 << " µs\n";
    std::cout << "Absolute Max     : " << rtt_values.back() << " µs\n";
    std::cout << "Std Deviation    : " << stdev << " µs\n";
    std::cout << "----------------------------------\n";

    if (handle) pcap_close(handle);
    exit(0);
}

void packet_handler(u_char *user, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
    uint64_t recv_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    int ip_header_len = (packet[14] & 0x0F) * 4;
    const u_char *payload = packet + 14 + ip_header_len + 8;

    const MarketDataTick *tick = reinterpret_cast<const MarketDataTick *>(payload);
    
    uint64_t rtt_ns = recv_time_ns - tick->timestamp_ns;
    double rtt_us = rtt_ns / 1000.0;

    rtt_values.push_back(rtt_us);

    std::cout << "Tick Seq: " << tick->sequence << " | RTT: " << rtt_us << " µs\n";
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <interface_name>\n";
        return 1;
    }
    std::signal(SIGINT, handle_sigint);
    
    char errbuf[PCAP_ERRBUF_SIZE];
    handle = pcap_open_live(argv[1], BUFSIZ, 1, 1, errbuf);
    
    if (handle == nullptr) {
        std::cerr << "Could not open device " << argv[1] << ": " << errbuf << "\n";
        return 2;
    }
    
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, "udp src port 9000", 0, PCAP_NETMASK_UNKNOWN) == -1) {
        std::cerr << "Could not parse filter\n";
        return 3;
    }
    pcap_setfilter(handle, &fp);
    
    std::cout << "Listening for echoed market data on " << argv[1] << "...\n";
    std::cout << "Press Ctrl+C to stop and calculate statistics.\n";
    
    pcap_loop(handle, 0, packet_handler, nullptr);
    
    return 0;
}