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
#include <charconv>

#include "trie.hpp"
#include "tritrie.hpp"
#include "flatritrie.hpp"
// #include "flat4.hpp"

#include "hashmap.hpp"
#include "utils.hpp"

/* Test suite with all tests. */
template<typename T>
void test_suite(T &algo, const std::string &name,
                const std::vector<uint32_t> &test_queries)
{
    std::cout << "== Test Suite for " << name << std::endl;

    /* Selected to be a deep /32 entry */
    const uint32_t ip_positive_deep = ip_to_hl("72.247.182.140");

    /* Selected to be a deep /32 entry */
    const uint32_t ip_negative = ip_to_hl("134.191.220.74");

    const int queries_cnt = test_queries.size();

    test_query("True random query test",
               algo,
               [] (int i) {return fastrand();});

    test_query("Positive random query test",
               algo,
               [test_queries, queries_cnt] (int i) {
                 return test_queries[i % queries_cnt];
               });

    test_query("Repetitive positive /32 query test",
               algo,
               [ip_positive_deep] (int i) {return ip_positive_deep;});

    test_query("Repetitive negative query test",
               algo,
               [ip_negative] (int i) {return ip_negative;});

    show_mem_usage(false);
}

void test_map(const std::vector<std::string> &test_data,
              const std::vector<uint32_t> &test_queries) {
    /* Construct map */
    IPMap<> map;
    test_generation("Hashmap", map, test_data);
    std::cout << "Hashmap size is " << map.size() << std::endl;

    test_suite(map, "Hashmap", test_queries);
    std::cout << std::endl;
}

void test_trie(const std::vector<std::string> &test_data,
               const std::vector<uint32_t> &test_queries) {
    Trie trie;
    test_generation("Trie", trie, test_data);
    std::cout << "Nodes created " << trie.size() << std::endl;

    test_suite(trie, "Trie", test_queries);

    FlaTrie flatrie;
    measure("Flatrie generation",
            [&flatrie, &trie] () {
                flatrie.build(trie);
            });

    test_suite(flatrie, "Flatrie", test_queries);

    show_mem_usage();
    std::cout << std::endl;
}

template<int BITS=8>
void test_tritrie(const std::string &name,
                  const std::vector<std::string> &test_data,
                  const std::vector<uint32_t> &test_queries) {
    Tritrie::Tritrie<BITS> tritrie;

    test_generation("Tritrie" + name, tritrie, test_data);
    std::cout << "Nodes created " << tritrie.size() << std::endl;
    test_suite(tritrie, "Tritrie" + name, test_queries);

    Tritrie::Flat<BITS> flatritrie;
    measure("Flatritrie" + name + " generation",
            [&] () {
                flatritrie.build(tritrie);
            });
    test_suite(flatritrie, "Flatritrie" + name, test_queries);
    flatritrie.debug();
    std::cout << std::endl;
}

int main() {
    #ifndef NDEBUG
    std::cout << "Watchout - for good results use benchmarks with RELEASE=1"
              << std::endl;
    #endif

    /* Prepare subnet data and query data */
    auto test_data = load_test_data("test_data.txt");
    auto test_queries = get_rnd_test_data(test_data);

    /* To get accurate RAM measurements, test one structure at a time */
    show_mem_usage(true);
    test_trie(test_data, test_queries);

    show_mem_usage(true);
    test_tritrie<8>("<8>", test_data, test_queries);

    show_mem_usage(true);
    test_tritrie<6>("<6>", test_data, test_queries);

    show_mem_usage(true);
    test_tritrie<4>("<4>", test_data, test_queries);

    show_mem_usage(true);
    test_map(test_data, test_queries);
    return 0;
}
