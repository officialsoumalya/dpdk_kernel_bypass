//market data generstor and broadcastor
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <thread>

// Standard 32-byte market data payload
struct MarketDataTick {
    uint64_t sequence;
    uint64_t timestamp_ns;
    char symbol[8];
    double price;
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <Target IP> <Port>\n";
        return 1;
    }

    const char* target_ip = argv[1];
    int port = std::stoi(argv[2]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    int broadcastEnable = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    
    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    inet_pton(AF_INET, target_ip, &dest_addr.sin_addr);

    MarketDataTick tick = {0, 0, "NIFTY50", 22500.50};

    for (int i = 0; i < 10000; ++i) {
        tick.sequence = i;
        
        // Capture exact nanosecond timestamp right before transmission
        tick.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        sendto(sock, &tick, sizeof(tick), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        
        // 1 millisecond micro-sleep to prevent flooding the hypervisor's virtual queues
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "Successfully injected 1000 market data ticks to " << target_ip << ":" << port << "\n";
    close(sock);
    return 0;
}