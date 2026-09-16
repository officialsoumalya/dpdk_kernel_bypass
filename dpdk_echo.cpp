#include <iostream>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

struct MarketDataTick {
    uint64_t sequence;
    uint64_t timestamp_ns;
    char symbol[8];
    double price;
};

int main(int argc, char *argv[]) {
    if (rte_eal_init(argc, argv) < 0) {
        rte_exit(EXIT_FAILURE, "EAL initialization failed\n");
    }

    uint16_t port_id = 0;
    
    //allocate the memory pool for zero-copy hardware DMA
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", 8191,
        250, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    //configure port with 1 RX and 1 TX queue
    struct rte_eth_conf port_conf = {};
    rte_eth_dev_configure(port_id, 1, 1, &port_conf);
    rte_eth_rx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id), NULL, mbuf_pool);
    rte_eth_tx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id), NULL);

    rte_eth_dev_start(port_id);
    rte_eth_promiscuous_enable(port_id);

    std::cout << "DPDK kernel bypass receiver listening...\n";

    struct rte_mbuf *bufs[32];
    
    // continuous hardware polling
    while (true) {
        const uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, 32);
        
        for (int i = 0; i < nb_rx; i++) {
            struct rte_mbuf *m = bufs[i];
            
            //casting direct hardware memory to network headers
            auto *eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
            if (rte_be_to_cpu_16(eth_hdr->ether_type) != RTE_ETHER_TYPE_IPV4) {
                rte_pktmbuf_free(m); continue;
            }

            auto *ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
            if (ip_hdr->next_proto_id != IPPROTO_UDP) {
                rte_pktmbuf_free(m); continue;
            }

            auto *udp_hdr = (struct rte_udp_hdr *)((unsigned char *)ip_hdr + sizeof(struct rte_ipv4_hdr));
            if (rte_be_to_cpu_16(udp_hdr->dst_port) != 9000) {
                rte_pktmbuf_free(m); continue;
            }

            //Extracting application payload
            auto *tick = (MarketDataTick *)(udp_hdr + 1);
//            std::cout << "DPDK Fastpath - Seq: " << tick->sequence 
  //                    << " | Symbol: " << tick->symbol 
    //                  << " | Price: " << tick->price << "\n";

            //hardware Echo - Swap L2, L3, L4 routing, to send to the server for latency benchmarking
            struct rte_ether_addr tmp_mac = eth_hdr->dst_addr;
            eth_hdr->dst_addr = eth_hdr->src_addr;
            eth_hdr->src_addr = tmp_mac;

            uint32_t tmp_ip = ip_hdr->dst_addr;
            ip_hdr->dst_addr = ip_hdr->src_addr;
            ip_hdr->src_addr = tmp_ip;

            uint16_t tmp_port = udp_hdr->dst_port;
            udp_hdr->dst_port = udp_hdr->src_port;
            udp_hdr->src_port = tmp_port;

            //pushing directly back to the transmission ring buffer
            if (rte_eth_tx_burst(port_id, 0, &m, 1) < 1) {
                rte_pktmbuf_free(m);
            }
        }
    }
    return 0;
}
