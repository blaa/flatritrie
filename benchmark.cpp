/*
 * Copyright 2019 Tomasz bla Fortuna. All rights reserved.
 * License: MIT
 *
 * Benchmarks to approximate the performance.
 */

#include <iostream>
#include <algorithm>
#include <functional>
#include <bitset>
#include <boost/algorithm/string.hpp>

#include "trie.hpp"
#include "tritrie.hpp"
#include "flatritrie.hpp"
#include "hashmap.hpp"
#include "utils.hpp"

/* Tritrie and Flatritrie configuration */
const int BITS = 8;

/* Test suite with all tests. */
template<typename T>
void test_suite(T &algo, const std::string &name)
{
    std::cout << "== Test Suite for " << name << std::endl;

    /* Used IPs are picked at random from test data */

    /* IP from the beginning of a large range of matching IPs */
    const uint32_t ip_initial = ip_to_hl("77.83.16.0");

    /* Selected to be a deep /32 entry */
    const uint32_t ip_positive_deep = ip_to_hl("72.247.182.140");

    /* Middle /16 entry */
    const uint32_t ip_positive_middle = ip_to_hl("109.241.1.2");

    /* Selected to be a deep /32 entry */
    const uint32_t ip_negative = ip_to_hl("134.191.220.74");

    test_query("Random query test",
               algo,
               ip_initial,
               [] (int32_t ip, int i) {return ip ^ i;});

    test_query("Repetitive positive /32 query test",
               algo,
               ip_positive_deep,
               [] (int32_t ip, int i) {return ip;});

    test_query("Repetitive positive /16 query test",
               algo,
               ip_positive_middle,
               [] (int32_t ip, int i) {return ip;});

    test_query("Repetitive negative query test",
               algo,
               ip_negative,
               [] (int32_t ip, int i) {return ip;});

    show_mem_usage(false);
}

void test_map(const std::vector<std::string> &test_data) {
    /* Construct map */
    IPMap<> map;
    test_generation("Hashmap", map, test_data);
    std::cout << "Hashmap size is " << map.size() << std::endl;

    test_suite(map, "Hashmap");
    std::cout << std::endl;
}

void test_trie(const std::vector<std::string> &test_data) {
    Trie trie;
    test_generation("Trie", trie, test_data);
    std::cout << "Nodes created " << trie.size() << std::endl;

    test_suite(trie, "Trie");

    FlaTrie flatrie;
    measure("Flatrie generation",
            [&flatrie, &trie] () {
                flatrie.build(trie);
            });

    test_suite(flatrie, "Flatrie");

    show_mem_usage();
    std::cout << std::endl;
}

void test_tritrie(const std::vector<std::string> &test_data) {
    Tritrie::Tritrie<BITS> tritrie;

    test_generation("Tritrie", tritrie, test_data);
    std::cout << "Nodes created " << tritrie.size() << std::endl;
    test_suite(tritrie, "Tritrie");

    Tritrie::Flat<BITS> flatritrie;
    measure("Flatritrie generation",
            [&] () {
                flatritrie.build(tritrie);
            });
    test_suite(flatritrie, "Flatritrie");
    flatritrie.debug();
    std::cout << std::endl;
}

std::vector<std::string> load_test_data(const std::string &path) {
    std::ifstream in(path);
    std::string line;
    std::vector<std::string> addresses;

    while (getline(in, line)) {
        addresses.push_back(line);
    }
    /* Sort by mask - mostly required for hashmap */
    std::sort(addresses.begin(),
              addresses.end(),
              [](const std::string &a, const std::string &b) {
                  int mask_a, mask_b;
                  std::vector<std::string> addr_mask;
                  boost::split(addr_mask, a, boost::is_any_of("/"));
                  mask_a = atoi(addr_mask[1].c_str());

                  boost::split(addr_mask, b, boost::is_any_of("/"));
                  mask_b = atoi(addr_mask[1].c_str());
                  return mask_a < mask_b;
              });
    return addresses;
}

int main() {
    #ifndef NDEBUG
    std::cout << "Watchout - for good results use benchmarks with RELEASE=1"
              << std::endl;
    #endif

    /* Sort testdata from most generic to most specific */
    auto test_data = load_test_data("test_data.txt");

    /* To get accurate RAM measurements, test one structure at a time */
    show_mem_usage(true);
    test_trie(test_data);

    show_mem_usage(true);
    test_tritrie(test_data);

    show_mem_usage(true);
    test_map(test_data);
    return 0;
}
