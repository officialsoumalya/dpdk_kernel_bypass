#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

struct MarketDataTick {
    uint64_t sequence;
    uint64_t timestamp_ns;
    char symbol[8];
    double price;
};

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(9000);
    
    bind(sock, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    
    std::cout << "Kernel receiver listening on UDP 9000...\n";
    
    MarketDataTick buffer;
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    
    while (true) {
        ssize_t len = recvfrom(sock, &buffer, sizeof(buffer), 0, 
                               (struct sockaddr *)&client_addr, &client_len);
        if (len > 0) {
           // std::cout << "Received - Seq: " << buffer.sequence 
             //         << " | Symbol: " << buffer.symbol 
               //       << " | Price: " << buffer.price << "\n";
            // Instantly echo back to the source to calculate network delta
            sendto(sock, &buffer, len, 0, (struct sockaddr *)&client_addr, client_len);
        }
    }
    
    return 0;
}
