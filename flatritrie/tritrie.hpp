/*
 * Copyright 2019 Tomasz bla Fortuna. All rights reserved.
 * License: MIT
 * bla@thera.be, https://github.com/blaa/flatritrie
 */

#ifndef _BLA_TRITRIE_H_
#define _BLA_TRITRIE_H_

#include <iostream>
#include <string>
#include <vector>
#include <bitset>
#include <cassert>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace Tritrie {

using uint128_t = unsigned __int128;

/* Debug: support for printing unsigned __int128 */
std::ostream &operator<<(std::ostream &os, const uint128_t &data) {
    union {
        uint128_t whole;
        struct {
            uint64_t low;
            uint64_t high;
        };
    } broken;
    broken.whole = data;
    os << "H" << std::bitset<64>(broken.high);
    os << "L" << std::bitset<64>(broken.low);
    return os;
}

/* Common defaults for tritrie and flatritrie */
constexpr int DEFAULT_BITS = 8;

/*
 * Trie with more than 1 bit (2 branches) per level.
 */
template<int BITS=DEFAULT_BITS, typename K=uint32_t, typename V=int32_t, V def=-1>
class Tritrie {
protected:
    /* NOTE: No support in numerical_limits for int128 */
    constexpr static K MASK_MAX = (K)(-1);
    constexpr static int BITS_TOTAL = (8 * sizeof(K));
    constexpr static int CHILDREN = (1<<BITS);

    struct Node {
        /* Matched with triplets of bits */
        Node *child[CHILDREN] = {};

        /* 'def' for middle node */
        V value;

        Node() : value(def) {}

        void show() {
            std::cout << "Node value="
                      << this->value;
            for (int i=0; i < CHILDREN; i++) {
                std::cout << " child_" << i << "=" << this->child[i] << " ";
            }
            std::cout << std::endl;
        }
    };

    Node root;
    int nodes_cnt = 0;

    /* Mask during insertion can only grow or stay the same */
    int last_mask = 0;

    Node *get_or_create(Node *cur, const uint8_t tri) {
        if (cur->child[tri] == NULL) {
            cur->child[tri] = new Node();
            this->nodes_cnt += 1;
        }
        return cur->child[tri];
    }

    void add_ip(K ip, int mask, V value) {
        int mask_left = mask;
        Node *cur = &this->root;

        if (mask < this->last_mask) {
            std::cerr << "Inserting mask " << mask
                      << " after mask " << this->last_mask << std::endl;
            throw std::runtime_error("Invalid order of IP insertion to Tritrie");
        }

        assert(BITS_TOTAL > BITS);
        this->last_mask = mask;

        while (mask_left >= BITS) { /* TODO: CONV to for */
            /* Shave "BITS" most significant bits */
            const int tri = ip >> (BITS_TOTAL - BITS);
            ip <<= BITS;

            cur = this->get_or_create(cur, tri);

            /* Next one! */
            mask_left -= BITS;
        }

        /* Handle last level appropriately */
        if (mask_left) {
            /* Mask is not aligned and splits the Tritrie level */

            ip >>= (BITS_TOTAL - BITS);

            /*
             * Possibilities for BITS=3 or 4:
             * mask_left   final_mask  BITS
             * 1           100            3
             * 2           110            3
             *
             * 1           1000           4
             * 2           1100           4
             * 3           1110           4
             * etc.
             * (0xffffffff >> (BITS_TOTAL-mask_left)) << (BITS - mask_left)
             */
            const K mask = (
                (MASK_MAX >> (BITS_TOTAL - mask_left)) << (BITS - mask_left)
            );
            assert(mask != 0);

            for (int tri = 0; tri < CHILDREN; tri++) {
                if ((tri & mask) == ip) {
                    /* Insert here */
                    auto lvl = this->get_or_create(cur, tri);
                    lvl->value = value;
                }
            }
        } else {
            /* After using whole mask, the IP should be 0 */
            assert(ip == 0);
            cur->value = value;
        }
    }

    void release(Node *node) {
        for (int i = 0; i < CHILDREN; i++) {
            if (node->child[i] != NULL) {
                release(node->child[i]);
                assert(this->nodes_cnt >= 0);
                delete node->child[i];
                node->child[i] = NULL;
                this->nodes_cnt -= 1;
            }
        }
    }

    /**
     * Decompose string form of an IP to numerical address and mask.
     * Sets mask to -1 if it's not given.
     */
    void ip_from_string(const std::string &addr_mask, K &ip_n, int &mask_n) const {
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

    /* Don't copy. */
    Tritrie(const Tritrie &tritrie);

public:
    Tritrie() {}
    ~Tritrie() {
        this->release(&this->root);
    }

    void add(const std::string addr_mask, V value) {
        K ip;
        int mask;
        this->ip_from_string(addr_mask, ip, mask);
        if (mask == -1) {
            throw std::runtime_error("Address without a mask");
        }

        assert(mask >= 1 && mask <= BITS_TOTAL);
        if (mask < 1 || mask > BITS_TOTAL)
            throw std::exception();
        this->add_ip(ip, mask, value);
    }

    V query_string(const std::string &addr) const {
        K ip;
        int mask;
        this->ip_from_string(addr, ip, mask);
        if (mask != -1 && mask != BITS_TOTAL) {
            throw std::runtime_error("Query with partial mask.");
        }

        return this->query(ip);
    }

    V query(K ip) const {
        const Node *cur = &this->root;
        V matched = def;
        for (int mask = 0; mask < BITS_TOTAL; mask++) {
            const int tri = ip >> (BITS_TOTAL - BITS);
            cur = cur->child[tri];
            if (cur == NULL) {
                break;
            }
            if (cur->value != -1) {
                matched = cur->value;
                /* We will continue search, as there might be a closer match */
            }
            ip <<= BITS;
        }
        return matched;
    }

    int size() const {
        return this->nodes_cnt;
    }

    template<int B, typename TK, typename TV, TV tdef, int PAGE_SIZE> friend class Flat;
};

};

#endif
