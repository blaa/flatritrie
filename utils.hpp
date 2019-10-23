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

/* Fill structure with data and measure time */
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

/* Query structure for a given IP, possibly mutate IPs between queries */
template<typename T, typename Fn, typename IP>
void test_query(const std::string &name, T &algo,
                IP initial_ip,
                Fn mutate_ip,
                const int tests = 5000000) {
    int found = 0, nx = 0;
    auto took = measure("",
                        [&] () {
                            for (int i = 0; i < tests; i++) {
                                const IP test_ip = mutate_ip(initial_ip, i);
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

/* Convert dot-ipv4 notation to network-byte-order binary */
uint32_t ip_to_hl(const std::string &addr) {
    in_addr ip_parsed;
    int ret = inet_aton(addr.c_str(), &ip_parsed);
    if (ret == 0)
        throw std::exception();

    uint32_t ip_network = ntohl(ip_parsed.s_addr);
    return ip_network;
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

int fastrand(void) {
    static unsigned long next = 1;
    next = next * 1103515245 + 12345;
    return((unsigned)(next/65536) % RAND_MAX);
}

#endif
