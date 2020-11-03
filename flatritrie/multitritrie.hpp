 /*
  * Copyright 2019-2020 Tomasz bla Fortuna. All rights reserved.
  * License: MIT
  * bla@thera.be, https://github.com/blaa/flatritrie
  */

#ifndef _BLA_MULTITRITRIE_H_
#define _BLA_MULTITRITRIE_H_

#include <iostream>
#include <string>
#include <bitset>
#include <cassert>
#include <set>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace Tritrie {

using uint128_t = unsigned __int128;

/*
 * Trie with a configurable number of branches per level (1 to 8).
 */
template<int BITS=8, typename K=uint32_t, typename V=int32_t, V def=-1>
class MultiTritrie {
protected:
    /* NOTE: No support in numerical_limits for int128 */
    constexpr static K MASK_MAX = (K)(-1);
    constexpr static int BITS_TOTAL = (8 * sizeof(K));
    constexpr static int CHILDREN = (1<<BITS);

    struct Node {
        /* Matched with triplets of bits */
        Node *child[CHILDREN] = {};

        /* 'def' for middle node TODO: or better - empty? */
        /* Longest Prefix Match value */
        V lpm_value;
        /* Accumulated matching entries */
        std::set<V> values;

        Node() : lpm_value(def) {}

        void show() {
            std::cout << "Node lpm value="
                      << this->lpm_value
                      << " all="
                      << this->values;
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
        /* While diving deeper, we "carry" and aggregate previously passed
           values */
        std::set<V> aggregated = cur->values;

        /* Runtime sanity check */
        if (mask < this->last_mask) {
            std::cerr << "Inserting mask " << mask
                      << " after mask " << this->last_mask << std::endl;
            throw std::runtime_error("Invalid order of IP insertion to MultiTritrie");
        }
        this->last_mask = mask;

        assert(BITS_TOTAL > BITS);

        for (; mask_left >= BITS; mask_left -= BITS) {
            /* Shave "BITS" most significant bits */
            const int tri = ip >> (BITS_TOTAL - BITS);
            ip <<= BITS;

            cur = this->get_or_create(cur, tri);
            if (cur->values.size() == 0) {
                /* If we created a new node, we should pass down all aggregated
                 * values */
                cur->values.insert(aggregated.begin(), aggregated.end());
            } else {
                /* Old node - aggregate its values into set */
                aggregated.insert(cur->values.begin(), cur->values.end());
            }
        }

        /* We reached a place to add the new value */
        aggregated.insert(value);

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
            const K mask = ((MASK_MAX >> (BITS_TOTAL - mask_left))
                            << (BITS - mask_left));
            assert(mask != 0);

            for (int tri = 0; tri < CHILDREN; tri++) {
                if ((tri & mask) == ip) {
                    /* Insert here */
                    auto lvl = this->get_or_create(cur, tri);
                    lvl->lpm_value = value;
                    lvl->values.insert(aggregated.begin(), aggregated.end());
                }
            }
        } else {
            /* After using whole mask, the IP should be 0 */
            assert(ip == 0);
            cur->lpm_value = value;
            cur->values.insert(aggregated.begin(), aggregated.end());
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
    MultiTritrie(const MultiTritrie &tritrie);

public:
    MultiTritrie() {}
    ~MultiTritrie() {
        this->release(&this->root);
    }

    void add(const std::string addr_mask, V value) {
        K ip;
        int mask;
        this->ip_from_string(addr_mask, ip, mask);
        if (mask == -1) {
            throw std::runtime_error("Address without a mask");
        }

        assert(mask >= 0 && mask <= BITS_TOTAL);
        if (mask < 0 || mask > BITS_TOTAL)
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

    const std::set<V> &query_all_string(const std::string &addr) const {
        K ip;
        int mask;
        this->ip_from_string(addr, ip, mask);
        if (mask != -1 && mask != BITS_TOTAL) {
            throw std::runtime_error("Query with partial mask.");
        }
        return this->query_all(ip);
    }

    V query(K ip) const {
        const Node *cur = &this->root;
        V matched = cur->lpm_value;
        for (int mask = 0; mask < BITS_TOTAL; mask++) {
            const int tri = ip >> (BITS_TOTAL - BITS);
            cur = cur->child[tri];
            if (cur == NULL) {
                break;
            }
            if (cur->lpm_value != def) {
                matched = cur->lpm_value;
                /* We will continue search, as there might be a closer match */
            }
            ip <<= BITS;
        }
        return matched;
    }

    const std::set<V> &query_all(K ip) const {
        const Node *cur = &this->root;
        const std::set<V> *matched = &cur->values;
        for (int mask = 0; mask < BITS_TOTAL; mask++) {
            const int tri = ip >> (BITS_TOTAL - BITS);
            cur = cur->child[tri];
            if (cur == NULL) {
                break;
            }
            matched = &cur->values;
            ip <<= BITS;
        }
        return *matched;
    }

    int size() const {
        return this->nodes_cnt;
    }

    template<int B, typename TK, typename TV, TV tdef, int PAGE_SIZE> friend class Flat;
};

};

#endif
