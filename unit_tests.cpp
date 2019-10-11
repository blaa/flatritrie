/*
 * Copyright 2019 Tomasz bla Fortuna. All rights reserved.
 * License: MIT
 *
 * Unit tests to check algorithm correctness.
 */

#include <iostream>
#include <cstdint>
#include "trie.hpp"
#include "tritrie.hpp"
#include "flatritrie.hpp"
#include "hashmap.hpp"

namespace Test {

const std::vector<std::pair<std::string, int>> data_v4 = {
    /* Address, ID */
    {"255.0.0.0/8", 0},
    {"255.255.0.0/16", 1},
    {"10.255.0.0/16", 2},

    /* Colliding testcases */
    {"95.175.112.0/21", 4},
    {"95.175.144.0/21", 5},

    /* Collides tritrie */
    {"170.85.200.0/22", 6},
    {"170.85.202.0/24", 7},

    {"10.255.0.3/32", 3},
};

const std::vector<std::pair<std::string, int>> testcases_v4 = {
    {"10.255.0.0", 2},
    {"10.255.1.0", 2},
    {"10.255.255.255", 2},
    {"10.255.0.3", 3},

    {"255.0.0.0", 0},
    {"255.1.0.0", 0},
    {"255.255.0.0", 1},
    {"255.255.255.0", 1},
    {"255.255.123.42", 1},

    {"254.0.0.0", -1},
    {"0.0.0.0", -1},

    {"170.85.200.0", 6},
    {"170.85.200.1", 6},
    {"170.85.203.255", 6},

    {"170.85.202.0", 7},
    {"170.85.202.255", 7},

    {"95.175.111.255", -1},
    {"95.175.112.0", 4},
    {"95.175.119.255", 4},
    {"95.175.120.0", -1},
    {"95.175.144.1", 5},
    {"95.175.151.254", 5},
};

const std::vector<std::pair<std::string, int>> data_v6 = {
    /* Address, ID */
    {"2001:200::/32", 1},
    {"2001:200:4000::/38", 2},

    {"2001:200:4490::/44", 3},
    {"2001:200:4488::/45", 4},

    {"2001:470:0:285::/64", 23},

    {"2001:470:0:284::2000/115", 11},
    {"2001:470:0:284::1000/116", 10},

    {"2001:470:0:284::2/127", 22},
    {"2001:470:0:284::/128", 20},
    {"2001:470:0:284::1/128", 21},

    {"2001:470:1f0b:a9:9dc3:6ed8:e819:f89a/128", 40},

};

const std::vector<std::pair<std::string, int>> testcases_v6 = {
    {"2001:200::/128", 1},
    {"2001:200::10/128", 1},
    {"2001:200:1:2:3:4:5:6", 1},

    {"2001:200:4000::1", 2},

    {"2001:200:4000:ffff:ffff:ffff:ffff:ffff", 2},
    {"2001:200:4490::1", 3},
    {"2001:200:4488::1", 4},

    {"2001:470:0:284::1000", 10},
    {"2001:470:0:284::1fff", 10},
    {"2001:470:0:284::999", -1},

    {"2001:470:0:284::2000", 11},
    {"2001:470:0:284::", 20},
    {"2001:470:0:284::1", 21},
    {"2001:470:0:284::2", 22},
    {"2001:470:0:284::3", 22},

    {"2001:470:0:285::", 23},
    {"2001:470:0:285:a:b:c:d", 23},

    {"2001:470:1f0b:a9:9dc3:6ed8:e819:f89a", 40},
    {"2001:470:1f0b:a9:9dc3:6ed8:e819:f89b", -1},
    {"2001:470:1f0b:a9:9dc3:6ed8:e819:f899", -1},
    {"2002:470:1f0b:a9:9dc3:6ed8:e819:f89a", -1},
};


template<typename T, typename K>
int runner(T &algo, K &testcases) {
    int successes = 0;
    int failures = 0;
    for (auto &testcase: testcases) {
        int ret = algo.query_string(testcase.first);
        if (ret != testcase.second) {
            std::cout << "TEST FAIL " << testcase.first
                      << " returned " << ret << " should "
                      << testcase.second
                      << std::endl;
            failures += 1;
        } else {
            successes += 1;
        }
    };
    std::cout << "TESTS: OK=" << successes << " FAILED="
              << failures << std::endl;
    std::cout << std::endl;
    return failures;
}

}

int testcase_map() {
    IPMap<> map;
    for (auto &item: Test::data_v4) {
        map.add(item.first, item.second);
    }

    std::cout << "Map testcases" << std::endl;
    int ret = Test::runner<>(map, Test::testcases_v4);
    return ret;
}

int testcase_trie() {
    int ret;
    Trie trie;
    for (auto &item: Test::data_v4) {
        trie.add(item.first, item.second);
    }

    std::cout << "Trie testcases" << std::endl;
    ret = Test::runner<>(trie, Test::testcases_v4);

    /* Flatrie */
    FlaTrie flatrie;
    flatrie.build(trie);

    // flatrie.show();
    std::cout << "Flatrie testcases" << std::endl;
    ret += Test::runner<>(flatrie, Test::testcases_v4);

    return ret;
}

template<int BITS>
int testcase_tritrie() {
    int ret = 0;
    Tritrie::Tritrie<BITS> tritrie;

    std::cout << "Generating tritrie<" << BITS << ">" << std::endl;
    int id = 0;
    for (auto &item: Test::data_v4) {
        tritrie.add(item.first, item.second);
        id++;
    }

    std::cout << "Testing tritrie<" << BITS << ">" << std::endl;
    ret = Test::runner<>(tritrie, Test::testcases_v4);

    /* FlaTritrie test */
    Tritrie::Flat<BITS> flatritrie;
    flatritrie.build(tritrie);
    std::cout << "Testing flatritrie<" << BITS << ">" << std::endl;
    ret += Test::runner<>(flatritrie, Test::testcases_v4);

    /* Should build second time as well */
    flatritrie.build(tritrie);


    /* Error handling */
    try {
        tritrie.add("8.8.8.8", 100); /* Throws exception */
        std::cout << "Parsing error" << std::endl;
        ret += 1;
    } catch(std::runtime_error &re) {
    }
    return ret;
}

template<int BITS>
int testcase_ipv6() {
    int ret = 0;
    Tritrie::Tritrie<BITS, Tritrie::uint128_t> tritrie;

    std::cout << "Generating tritrie<" << BITS << "> for IPv6" << std::endl;
    int id = 0;
    for (auto &item: Test::data_v6) {
        tritrie.add(item.first, item.second);
        id++;
    }

    ret += Test::runner<>(tritrie, Test::testcases_v6);

    return ret;
}

int main() {
    int ret = 0;

    ret = testcase_map();
    ret += testcase_trie();
    // meh
    ret += testcase_tritrie<1>();
    ret += testcase_tritrie<2>();
    ret += testcase_tritrie<3>();
    ret += testcase_tritrie<4>();
    ret += testcase_tritrie<5>();
    ret += testcase_tritrie<6>();
    ret += testcase_tritrie<7>();
    ret += testcase_tritrie<8>();

    testcase_ipv6<8>();

    return ret;
}
