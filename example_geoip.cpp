/*
 * Copyright 2019 Tomasz bla Fortuna. All rights reserved.
 * License: MIT
 *
 * Example on how to use the flatritrie to query each IP for its country of
 * origin. To use it, download the GeoLite2 database from Maxmind:
 * https://dev.maxmind.com/geoip/geoip2/geolite2/ (CC BY-AS 4.0)
 * and unpack Locations and Country blocks files.
 */

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <boost/algorithm/string.hpp>
#include <charconv>

#include "tritrie.hpp"
#include "flatritrie.hpp"
#include "utils.hpp"

const int POLAND = 798544;
const int BITS = 4;

template<typename Fn>
void read_csv(const std::string path, Fn reader) {
    std::ifstream ifile(path);
    std::string line;
    std::vector<std::string> row;
    std::getline(ifile, line); /* skip header */

    while (std::getline(ifile, line)) {
        boost::split(row, line, boost::is_any_of(","));
        reader(row);
    }
}

auto geo_example() {
    std::vector<std::pair<std::string, int>> geo_data;
    auto net_loader = \
        [&geo_data] (auto &row) {
            int geoname_id;
            if (row[1].size() > 0) {
                /* geoname_id */
                geoname_id = std::stoi(row[1]);
            } else if (row[2].size() > 0) {
                /* registered country ? */
                geoname_id = std::stoi(row[2]);
            } else {
                std::cout << "No country for " << row[0] << std::endl;
                geoname_id = -1;
            }
            geo_data.push_back({row[0], geoname_id});
        };

    // TODO: Use it.
    std::unordered_map<int, std::string> code_map;
    auto country_loader = \
        [&code_map] (auto &row) {
            int geoname_id = std::stoi(row[0]);
            std::string &continent = row[2];
            std::string &country = row[4];
            code_map[geoname_id] = continent + country;
        };

    read_csv("GeoLite2-Country-Locations-en.csv", country_loader);

    show_mem_usage(false);
    measure("Reading GeoIP Database",
            [&net_loader] () {
                read_csv("GeoLite2-Country-Blocks-IPv4.csv", net_loader);
            });

    /* Sort by mask */
    std::sort(geo_data.begin(),
              geo_data.end(),
              [](const auto &a, const auto &b) {
                  const auto &addr_a = a.first;
                  const auto &addr_b = b.first;
                  int mask_a = 0, mask_b = 0;
                  size_t found = addr_a.find("/");
                  assert(found != std::string::npos);
                  std::from_chars(addr_a.data() + found+1,
                                  addr_a.data() + addr_a.size(), mask_a);

                  found = addr_b.find("/");
                  std::from_chars(addr_b.data() + found+1,
                                  addr_b.data() + addr_b.size(), mask_b);
                  return mask_a < mask_b;
              });

    Tritrie::Tritrie<BITS> tritrie;
    measure("Tritrie generation for GeoIP Database",
            [&tritrie, &geo_data] () {
                for (auto &item: geo_data) {
                    tritrie.add(item.first, item.second);
                }
            });

    std::cout << "Tritrie nodes created " << tritrie.size() << std::endl;
    show_mem_usage();

    const int tests = 5000000;

    /* Trivial testcase */
    int ret = tritrie.query_string("96.17.148.229");
    if (ret != POLAND)
        throw std::exception();

    int ip_initial = 0;

    test_query("Tritrie random geo query test",
               tritrie,
               ip_initial,
               [] (int32_t ip, int i) {return fastrand(); /* ip + (11*i % 100000);*/},
               tests);

    /*
     * FlaTritrie
     */
    Tritrie::Flat<BITS> flatritrie;

    measure("Flatritrie generation",
            [&] () {
                flatritrie.build(tritrie);
            });

    flatritrie.debug();

    ret = flatritrie.query_string("96.17.148.229");
    if (ret != POLAND)
        throw std::exception();

    show_mem_usage();

    test_query("Flatritrie random geo query test",
               flatritrie,
               ip_initial,
               [] (int32_t ip, int i) {return fastrand(); /* ip + (11*i % 100000);*/},
               tests);
}

int main() {
    geo_example();
}
