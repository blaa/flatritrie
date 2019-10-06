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
 *
 */
template<int BITS=DEFAULT_BITS>
class Tritrie {
protected:
    constexpr static int CHILDREN = (1<<BITS);

    struct Node {
        /* Matched with triplets of bits */
        Node *child[CHILDREN] = {};

        /* -1 for middle node */
        int id;

        Node() : id(-1) {}

        void show() {
            std::cout << "Node id="
                      << this->id;
            for (int i=0; i < CHILDREN; i++) {
                std::cout << " child_" << i << "=" << this->child[i] << " ";
            }
            std::cout << std::endl;
        }
    };

    Node root;
    int nodes_cnt = 0;

    Node *get_or_create(Node *cur, const uint8_t tri) {
        if (cur->child[tri] == NULL) {
            cur->child[tri] = new Node();
            this->nodes_cnt += 1;
        }
        return cur->child[tri];
    }

    void add_ip(uint32_t ip, int32_t mask, int id) {
        int mask_left = mask;
        Node *cur = &this->root;

        while (mask_left >= BITS) {
            const int tri = ip >> (32 - BITS);

            cur = this->get_or_create(cur, tri);

            /* Next one! */
            ip <<= BITS;
            mask_left -= BITS;
        }

        if (mask_left) {
            /* Handle last level appropriately */
            /* mask_left is either 1 or 2 */
            // std::cout << "  mask_left " << mask_left << std::endl;

            ip >>= (32 - BITS);

            /*
             * mask_left   final_mask  BITS
             * 1           100            3
             * 2           110            3
             *
             * 1           1000           4
             * 2           1100           4
             * 3           1110           4
             * etc.
             * (0xffffffff >> (32-mask_left)) << (BITS - mask_left)
             */
            const int mask = (0xffffffff >> (32 - mask_left)) << (BITS - mask_left);

            for (uint32_t tri = 0; tri < CHILDREN; tri++) {
                if ((tri & mask) == ip) {
                    /* Insert here */
                    auto lvl = this->get_or_create(cur, tri);
                    lvl->id = id;
                }
            }
        } else {
            /* After using whole mask, the IP should be 0 */
            assert(ip == 0);

            /* No duplicates at the same level are acceptable */
            if (cur->id != -1) {
                std::cout << "New ID " << id << " collided with existing "
                          << cur->id << std::endl;
            }
            cur->id = id;
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

    void add(const std::string &addr, int id) {
        // std::cout << "Add " << addr << " " << id << std::endl;

        std::vector<std::string> addr_mask;
        boost::split(addr_mask, addr, boost::is_any_of("/"));
        assert(addr_mask.size() == 2);

        const int mask = atoi(addr_mask[1].c_str());
        in_addr ip_parsed;
        int ret = inet_aton(addr_mask[0].c_str(), &ip_parsed);
        assert(ret != 0);
        assert(mask >= 1 && mask <= 32);
        if (ret == 0 || mask < 1 || mask > 32)
            throw std::exception();

        uint32_t ip_network = ntohl(ip_parsed.s_addr);
        this->add_ip(ip_network, mask, id);
    }

    int query_string(const std::string &addr) const {
        in_addr ip_parsed;
        int ret = inet_aton(addr.c_str(), &ip_parsed);
        assert(ret != 0);
        if (ret == 0)
            throw std::exception();
        uint32_t ip_network = ntohl(ip_parsed.s_addr);
        return this->query(ip_network);
    }

    int query(uint32_t ip) const {
        const Node *cur = &this->root;
        int matched_id = -1;
        for (int mask = 0; mask < 32; mask++) {
            const int tri = ip >> (32 - BITS);
            cur = cur->child[tri];
            if (cur == NULL) {
                break;
            }
            if (cur->id != -1) {
                matched_id = cur->id;
                /* We will continue search, as there might be a closer match */
            }
            ip <<= BITS;
        }
        return matched_id;
    }

    int size() const {
        return this->nodes_cnt;
    }

    template<int B, int PS> friend class Flat;
};

};
#endif
