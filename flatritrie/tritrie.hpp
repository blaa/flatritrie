/*
 * Copyright 2019 Tomasz bla Fortuna. All rights reserved.
 * License: MIT
 * bla@thera.be, https://github.com/blaa/flatritrie
 */

#ifndef _TRITRIE_H_
#define _TRITRIE_H_

#include <iostream>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace Tritrie {

/* Common defaults for tritrie and flatritrie */
constexpr int DEFAULT_BITS = 8;

/*
 * Trie with more than 1 bit (2 branches) per level.
 */
template<int BITS=DEFAULT_BITS, typename K=uint32_t, typename V=int32_t, V def=-1>
class Tritrie {
protected:
    constexpr static int MASK_MAX = std::numeric_limits<K>::max();
    constexpr static int BITS_TOTAL = std::numeric_limits<K>::digits;
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

    void add_ip(K ip, int32_t mask, V value) {
        int mask_left = mask;
        Node *cur = &this->root;

        assert(mask >= this->last_mask);
        this->last_mask = mask;

        while (mask_left >= BITS) {
            const int tri = ip >> (BITS_TOTAL - BITS);

            cur = this->get_or_create(cur, tri);

            /* Next one! */
            ip <<= BITS;
            mask_left -= BITS;
        }

        if (mask_left) {
            /* Handle last level appropriately */

            ip >>= (BITS_TOTAL - BITS);

            /*
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
            const int mask = (
                (MASK_MAX >> (BITS_TOTAL - mask_left)) << (BITS - mask_left)
            );

            for (uint32_t tri = 0; tri < CHILDREN; tri++) {
                if ((tri & mask) == ip) {
                    /* Insert here */
                    auto lvl = this->get_or_create(cur, tri);
                    lvl->value = value;
                }
            }
        } else {
            /* After using whole mask, the IP should be 0 */
            assert(ip == 0);

            /* No duplicates at the same level are acceptable */
            if (cur->value != -1) {
                std::cout << "New ID " << value << " collided with existing "
                          << cur->value << std::endl;
            }
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

    /* Don't copy. */
    Tritrie(const Tritrie &tritrie);

public:
    Tritrie() {}
    ~Tritrie() {
        this->release(&this->root);
    }

    void add(const std::string &addr, V value) {
        std::vector<std::string> addr_mask;
        boost::split(addr_mask, addr, boost::is_any_of("/"));
        assert(addr_mask.size() == 2);

        const int mask = atoi(addr_mask[1].c_str());
        in_addr ip_parsed;
        int ret = inet_aton(addr_mask[0].c_str(), &ip_parsed);
        assert(ret != 0);
        assert(mask >= 1 && mask <= BITS_TOTAL);
        if (ret == 0 || mask < 1 || mask > BITS_TOTAL)
            throw std::exception();

        uint32_t ip_network = ntohl(ip_parsed.s_addr);
        this->add_ip(ip_network, mask, value);
    }

    V query_string(const std::string &addr) const {
        in_addr ip_parsed;
        int ret = inet_aton(addr.c_str(), &ip_parsed);
        assert(ret != 0);
        if (ret == 0)
            throw std::exception();
        uint32_t ip_network = ntohl(ip_parsed.s_addr);
        return this->query(ip_network);
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
