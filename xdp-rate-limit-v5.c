#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <bpf/bpf_helpers.h>

#define RATE_LIMIT_PKTPS 250 // Max packets per second
#define TIME_WINDOW_NS 1000000000 // 1 second in nanoseconds
#define RATE_LIMIT_BPS 10000000  // Number bits per second (10 Mbps)

struct rate_limit_entry {
    __u64 last_update; // Timestamp of the last update
    __u64 bit_count; //Number of bits of packet count within the time window
    __u32 packet_count; // Packet count within the time window
};

// Hash map to track rate limits for each source IP
struct bpf_map_def SEC("maps") rate_limit_map = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = sizeof(__u32), // IPv4 source address
    .value_size = sizeof(struct rate_limit_entry),
    .max_entries = 1024,
};

SEC("xdp") int xdp_rate_limit(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data; // Parse Ethernet header
    struct ethhdr *eth = data;
    
    if ((void *)(eth + 1) > data_end)
        return XDP_PASS;
    
    // Check for IP packets
    if (eth->h_proto != __constant_htons(ETH_P_IP))
        return XDP_PASS;
    
    // Parse IP header
    struct iphdr *iph = (void *)(eth + 1);
    if ((void *)(iph + 1) > data_end)
        return XDP_PASS;
        
    __u32 src_ip = iph->saddr; // Source IP address

    /*
    // Compare two ways of parsing IP header
    struct iphdr *ip_test = data + sizeof(struct ethhdr);
    __u32 src_ip_test = ip_test->saddr;
    bpf_printk("xdp-rate-limit-v5: src_ip: %u\n", src_ip);
    bpf_printk("xdp-rate-limit-v5: src_ip_test: %u\n", src_ip_test);
    */
    
    // Lookup rate limit entry for this IP
    struct rate_limit_entry *entry = bpf_map_lookup_elem(&rate_limit_map, &src_ip);
    
    // Get current time in nanoseconds
    __u64 current_time = bpf_ktime_get_ns();

    // Packet size in bits of new packet
    __u64 pkt_size_bits = (data_end - data) * 8;
    
    if (entry) {
        // Check if we're in the same time window
        // bpf_printk("xdp-rate-limit-v5: time window: %u\n", current_time - entry->last_update);
        if (current_time - entry->last_update < TIME_WINDOW_NS) {
            entry->packet_count++;
            entry->bit_count = entry->bit_count + pkt_size_bits;
            // bpf_printk("xdp-rate-limit-v5: entry->bit_count: %u\n", entry->bit_count);
            if (entry->bit_count > RATE_LIMIT_BPS) {
                return XDP_DROP; // Drop packet if rate exceeds threshold
            }
        } else {
            // New time window, reset counter
            entry->last_update = current_time;
            entry->packet_count = 1;
            entry->bit_count = pkt_size_bits;
        }
    } else {
        // Initialize rate limit entry for new IP
        struct rate_limit_entry new_entry;
        // Zero out padding bytes
        __builtin_memset(&new_entry, 0, sizeof(new_entry));
        new_entry.last_update = current_time;
        new_entry.packet_count = 1;
        new_entry.bit_count = pkt_size_bits;
        bpf_map_update_elem(&rate_limit_map, &src_ip, &new_entry, BPF_ANY);
    }
    return XDP_PASS; // Allow packet if under threshold   
}

char _license[] SEC("license") = "GPL";
