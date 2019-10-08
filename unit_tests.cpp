/*
 * Copyright 2019 Tomasz bla Fortuna. All rights reserved.
 * License: MIT
 *
 * Unit tests to check algorithm correctness.
 */

#include <iostream>
#include "trie.hpp"
#include "tritrie.hpp"
#include "flatritrie.hpp"
#include "hashmap.hpp"

namespace Test {

const std::vector<std::pair<std::string, int>> data = {
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

const std::vector<std::pair<std::string, int>> testcases = {
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

template<typename T>
int runner(T &algo) {
    int successes = 0;
    int failures = 0;
    for (auto &testcase: Test::testcases) {
        int ret = algo.query_string(testcase.first);
        if (ret != testcase.second) {
            std::cout << "TEST FAIL " << testcase.first
                      << " ret=" << ret << " != "
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
    for (auto &item: Test::data) {
        map.add(item.first, item.second);
    }

    std::cout << "Map testcases" << std::endl;
    int ret = Test::runner<>(map);
    return ret;
}

int testcase_trie() {
    int ret;
    Trie trie;
    for (auto &item: Test::data) {
        trie.add(item.first, item.second);
    }

    std::cout << "Trie testcases" << std::endl;
    ret = Test::runner<>(trie);

    /* Flatrie */
    FlaTrie flatrie;
    flatrie.build(trie);

    // flatrie.show();
    std::cout << "Flatrie testcases" << std::endl;
    ret += Test::runner<>(flatrie);

    return ret;
}

template<int BITS>
int testcase_tritrie() {
    int ret = 0;
    Tritrie::Tritrie<BITS> tritrie;

    std::cout << "Generating tritrie<" << BITS << ">" << std::endl;
    int id = 0;
    for (auto &item: Test::data) {
        tritrie.add(item.first, item.second);
        id++;
    }

    std::cout << "Testing tritrie<" << BITS << ">" << std::endl;
    ret = Test::runner<>(tritrie);

    /* FlaTritrie test */
    Tritrie::Flat<BITS> flatritrie;
    flatritrie.build(tritrie);
    std::cout << "Testing flatritrie<" << BITS << ">" << std::endl;
    ret += Test::runner<>(flatritrie);

    /* Should build second time as well */
    flatritrie.build(tritrie);

    return ret;
}

int main() {
    int ret = 0;
    ret = testcase_map();
    ret += testcase_trie();

    /* meh */
    ret += testcase_tritrie<1>();
    ret += testcase_tritrie<2>();
    ret += testcase_tritrie<3>();
    ret += testcase_tritrie<4>();
    ret += testcase_tritrie<5>();
    ret += testcase_tritrie<6>();
    ret += testcase_tritrie<7>();
    ret += testcase_tritrie<8>();

    return ret;
}
