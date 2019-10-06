/*
 * Copyright 2019 Tomasz bla Fortuna. All rights reserved.
 * License: MIT
 * bla@thera.be, https://github.com/blaa/flatritrie
 */

#include <unordered_map>
#include <boost/algorithm/string.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * Store IPs in a expanded form: a pair: (IP=1.0.0.0/8, ID=1) will get
 * expanded to 16777216 IPs and stored separately in a map.
 *
 * IP Addresses are stored from the most generic (eg. /8) to more specific (like
 * /24 or /32) and more specific entries overwrite the previously stored generic
 * ones.
 *
 * This takes a lot of RAM, might not be very cache local, but should be fast
 * for querying. Won't do for IPv6 and /48 mask though.
 */
template<typename K=uint32_t, typename V=int32_t>
class IPMap {
protected:
    void expand_ip(V value, const std::string &address) {
        std::vector<std::string> address_split;
        boost::split(address_split, address, boost::is_any_of("/"));
        assert(address_split.size() == 2);
        in_addr_t addr = inet_addr(address_split[0].c_str());
        assert((int32_t)addr != -1);

        int mask = atoi(address_split[1].c_str());

        int ip_first = ntohl(addr);
        int ip_last = ip_first | (0xFFFFFFFF >> mask);

        if (mask == 32) {
            this->map[ip_first] = value;
            return;
        }

        for (int ip = ip_first; ip <= ip_last; ip++) {
            this->map[ip] = value;
        }
    }

    /* Forbid copying */
    IPMap(const IPMap &map);

    std::unordered_map<K, V> map;
public:
    IPMap(int reserve=1000000) {
        /* Reserving huge space can decrease number of collisions and improve
         * performance a lot */
        this->map.reserve(reserve);
    }

    void add(const std::string &addr, V value) {
        this->expand_ip(value, addr);
    }

    int query_string(const std::string &addr) {
        in_addr ip_parsed;
        int ret = inet_aton(addr.c_str(), &ip_parsed);
        assert(ret != 0);
        uint32_t ip_network = ntohl(ip_parsed.s_addr);
        return this->query(ip_network);
    }

    V query(K ip) const {
        auto t = map.find(ip);
        if (t == map.end()) {
            return -1;
        } else {
            return t->second;
        }
    }

    int size() const {
        return this->map.size();
    }
};
