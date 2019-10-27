/*
 * Copyright 2019 Tomasz bla Fortuna. All rights reserved.
 * License: MIT
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <chrono>
#include <iostream>
#include <fstream>
#include <boost/algorithm/string.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


/** Measure execution time of a lambda */
template<typename Fn>
int measure(std::string desc, Fn execute) {
    using std::chrono::system_clock;
    auto start = system_clock::now();
    execute();
    auto took = system_clock::now() - start;
    if (desc.length() > 0) {
        const double s = took.count() / 1e9;
        if (s < 1) {
            std::cout << desc << " took " << s * 1000 << "ms"
                      << std::endl;
        } else {
            std::cout << desc << " took " << s << "s"
                      << std::endl;
        }
    }
    return took.count();
}


/** Fill structure with data and measure time */
template<typename T, typename D>
void test_generation(const std::string &name, T &algo, const D &data) {
    measure(name + " generation",
            [&algo, &data] () {
                int id = 0;
                for (auto &item: data) {
                    algo.add(item, id);
                    /* Simulate a client ID / ptr */
                    id++;
                }
            });
}


/** Query structure for a given IP, possibly mutate IPs between queries */
template<typename T, typename Fn>
void test_query(const std::string &name, T &algo,
                Fn mutate_ip,
                const int tests = 5000000) {
    int found = 0, nx = 0;
    auto took = measure("",
                        [&] () {
                            for (int i = 0; i < tests; i++) {
                                const auto test_ip = mutate_ip(i);
                                const int ret = algo.query(test_ip);
                                if (ret == -1) {
                                    nx += 1;
                                } else {
                                    found += 1;
                                }
                            }
                        });
    const double per_s = tests / (took / 1e9);
    const double ns_per_q = 1.0 * took / tests;
    std::cout
        << name << " finished:" << std::endl
        << "  found=" << 100.0 * found / (found + nx) << "%"
        << " (" << found << " / " << nx << ")" << std::endl
        << "  queries " << tests << " in " << took / 1e9 << "s -> "
        << per_s / 1e6 << " Mq/s; "
        << ns_per_q << " ns/q"
        << std::endl;
}


/** Convert dot-ipv4 notation to network-byte-order binary */
uint32_t ip_to_hl(const std::string &addr) {
    in_addr ip_parsed;
    int ret = inet_aton(addr.c_str(), &ip_parsed);
    if (ret == 0)
        throw std::exception();

    uint32_t ip_network = ntohl(ip_parsed.s_addr);
    return ip_network;
}


/** Parse IP / mask */
template<typename K>
void ip_from_string(const std::string &addr_mask, K &ip_n, int &mask_n) {
    constexpr static int BITS_TOTAL = (8 * sizeof(K));

    std::string addr;
    size_t found = addr_mask.find("/");
    if (found == std::string::npos) {
        mask_n = -1;
        addr = addr_mask;
    } else {
        addr = addr_mask.substr(0, found);
        std::string mask_s = addr_mask.substr(found + 1, addr_mask.size());
        mask_n = std::stoi(mask_s);
    }

    if constexpr (BITS_TOTAL == 32) {
            in_addr ip_parsed;
            int ret = inet_pton(AF_INET, addr.c_str(), &ip_parsed);
            if (ret == 0)
                throw std::runtime_error("Unable to parse IPv4 address");

            ip_n = ntohl(ip_parsed.s_addr);
        } else if constexpr (BITS_TOTAL == 128) {
            in6_addr ip_parsed;
            int ret = inet_pton(AF_INET6, addr.c_str(), &ip_parsed);
            if (ret == 0)
                throw std::runtime_error("Unable to parse IPv6 address");

            /* Convert IPv6 to host order, so that bitshifts work ok */
            ip_n = 0;
            for (int i=0; i<16; i++) {
                ip_n |= ((K)ip_parsed.s6_addr[i]) << (120 - 8*i);
            }
        } else {
        throw std::runtime_error("IP Address of unknown lenght");
    }
}


void show_mem_usage(bool quiet = false)
{
    const std::string prefix = "VmRSS";
    std::ifstream status("/proc/self/status");
    std::string buffer;
    std::vector<std::string> columns;
    static int last_rss_kb = -1;

    while (std::getline(status, buffer)) {
        if (boost::starts_with(buffer, prefix)) {
            boost::split(columns, buffer, boost::is_any_of(" "),
                         boost::algorithm::token_compress_on);
            assert(columns.size() >= 2);
            const int rss_kb = atoi(columns[1].c_str());
            if (not quiet) {
                std::cout << "-> Process RSS: " << rss_kb << "kB;";
                if (last_rss_kb != -1) {
                    std::cout << " difference: " << rss_kb - last_rss_kb << "kB";
                }
                std::cout << std::endl;
            }
            last_rss_kb = rss_kb;
            break;
        }
    }
}


uint32_t fastrand(void) {
    static unsigned long next = 1;
    next = next * 1103515245 + 12345;
    return((unsigned)(next/65536) % RAND_MAX);
}


/** Using random input data (networks) generate random query data (IPs). */
std::vector<uint32_t> get_rnd_test_data(std::vector<std::string> &input_data,
                                        int count=5000000) {
    const int input_len = input_data.size();
    std::vector<uint32_t> data;
    std::string addr;

    for (int i = 0; i < count; i++) {
        auto addr_mask = input_data[fastrand() % input_len];
        uint32_t netip;
        int mask_n;
        ip_from_string<uint32_t>(addr_mask, netip, mask_n);
        assert(mask_n != -1);

        uint32_t mask = 0xffffffff << (32 - mask_n);
        uint32_t host_rnd = fastrand() & ~mask;
        uint32_t rnd_ip = netip | host_rnd;

        data.push_back(rnd_ip);
    }
    return data;
};


/** Load subnets from file and sort them by mask */
std::vector<std::string> load_test_data(const std::string &path) {
    std::ifstream in(path);
    std::string line;
    std::vector<std::string> addresses;

    while (getline(in, line)) {
        addresses.push_back(line);
    }

    /* Sort by mask */
    std::sort(addresses.begin(),
              addresses.end(),
              [](const std::string &a, const std::string &b) {
                  int mask_a = 0, mask_b = 0;
                  size_t found = a.find("/");
                  assert(found != std::string::npos);
                  std::from_chars(a.data() + found+1, a.data() + a.size(),
                                  mask_a);

                  found = b.find("/");
                  std::from_chars(b.data() + found+1, b.data() + b.size(),
                                  mask_b);
                  return mask_a < mask_b;
              });
    return addresses;
}

#endif
