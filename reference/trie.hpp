/*
 * Copyright 2019 Tomasz bla Fortuna. All rights reserved.
 * License: MIT
 * bla@thera.be, https://github.com/blaa/flatritrie
 */

#ifndef _TRIE_H_
#define _TRIE_H_

#include <iostream>
#include <string>
#include <vector>
#include <bitset>
#include <boost/algorithm/string.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * Basic Trie with 1 bit on a path. Browsing requires random jumps over a memory
 * and therefore doesn't play nice with caches.
 *
 * Would work for IPv6. The Trie is browsed until the end and the best "value"
 * match is updated, so we can match the best matching entry.
 */
class Trie {
protected:
    struct Node {
        /* 0 for bit 0, 1 for bit 1 */
        Node *child[2];
        /* -1 for middle node */
        int id;

        Node() : child(), id(-1) {}

        void show() {
            std::cout << "Node id="
                      << this->id
                      << " left=" << (this->child[0] != NULL)
                      << " right=" << (this->child[1] != NULL)
                      << std::endl;
        }
    };

    Node root;
    int nodes_cnt = 0;

    Node *get_or_create(Node *cur, const int bit) {
        if (cur->child[bit] == NULL) {
            cur->child[bit] = new Node();
            this->nodes_cnt += 1;
        }
        return cur->child[bit];
    }

    void add_ip(uint32_t ip, int32_t mask, int id) {
        int mask_left = mask;
        Node *cur = &this->root;

        while (mask_left > 0) {
            const int bit = ip >> 31;
            cur = this->get_or_create(cur, bit);

            /* Next one! */
            ip <<= 1;
            mask_left--;
        }

        /* After using whole mask, the IP should be 0 */
        assert(ip == 0);

        /* No duplicates at the same level are acceptable */
        if (cur->id != -1) {
            std::cout << "New ID " << id << " collided with existing "
                      << cur->id << std::endl;
            throw new std::exception();
        }
        assert(cur->id == -1);
        cur->id = id;
    }

    void show_tree(Node *cur, int indent) {
        const std::string indent_s = std::string(indent, ' ');
        std::cout << indent_s << "Node: " << cur->id << std::endl;
        if (cur->child[0]) {
            std::cout << indent_s << "0: SUB" << std::endl;
            this->show_tree(cur->child[0], indent + 3);
        } else {
            std::cout << indent_s << "0: NULL" << std::endl;
        }
        if (cur->child[1]) {
            std::cout << indent_s << "1: SUB" << std::endl;
            this->show_tree(cur->child[1], indent + 3);
        } else {
            std::cout << indent_s << "1 NULL" << std::endl;
        }
    }

public:
    Trie() {}

    void add(const std::string &addr, int id) {
        std::vector<std::string> addr_mask;
        boost::split(addr_mask, addr, boost::is_any_of("/"));
        assert(addr_mask.size() == 2);

        const int mask = atoi(addr_mask[1].c_str());
        in_addr ip_parsed;
        int ret = inet_aton(addr_mask[0].c_str(), &ip_parsed);
        if (ret == 0)
            throw std::exception();
        assert(mask >= 1 && mask <= 32);

        uint32_t ip_network = ntohl(ip_parsed.s_addr);
        this->add_ip(ip_network, mask, id);
    }

    int query_string(const std::string &addr) {
        in_addr ip_parsed;
        int ret = inet_aton(addr.c_str(), &ip_parsed);
        if (ret == 0)
            throw std::exception();
        assert(ret != 0);
        uint32_t ip_network = ntohl(ip_parsed.s_addr);
        return this->query(ip_network);
    }

    int query(uint32_t ip) const {
        const Node *cur = &this->root;
        int matched_id = -1;
        for (int mask = 0; mask < 32; mask++) {
            const int bit = ip >> 31;
            cur = cur->child[bit];
            if (cur == NULL) {
                break;
            }
            if (cur->id != -1) {
                matched_id = cur->id;
                /* We will continue search, as there might be a closer match */
            }
            ip <<= 1;
        }
        return matched_id;
    }

    void show() {
        this->show_tree(&this->root, 0);
    }

    int size() const {
        return this->nodes_cnt;
    }

    friend class FlaTrie;
};


/*
 * Structure built from the existing Trie which optimizes it for fast memory accesses:
 * - Consecutive 0, or 1 bits are compressed into a single entry.
 * - Kinda similar to Radix Trie.
 * - Data are flatten and stored in a table with jumps. Might be better for cache.
 * - Idea is to create "finite state automata" for browsing a Trie efficiently.
 */
class FlaTrie {
private:
    struct Entry;

    struct Side {
        Entry *jump = NULL;
        /* Number of bits (0 or 1) required to match */
        int16_t count = -1;

        Side() {}
        Side(int count, Entry *jump) : jump(jump), count(count) {}
    };

    struct Entry {
        /* Displaying only */
        int pos;

        /* Value (id) if reached this table entry */
        int id = -1;

        /* Two children: child[0] for bit 0, and [1] for bit 1 */
        Side child[2];

        void show(int pos) {
            std::cout << "entry=" << pos
                      << " id=" << this->id
                      << " 0x" << (int)this->child[0].count << "->"
                      << (this->child[0].jump == NULL ? -1 : this->child[0].jump->pos)
                      << " 1x" << (int)this->child[1].count << "->"
                      << (this->child[1].jump == NULL ? -1 : this->child[1].jump->pos)
                      << std::endl;
        }
    };

    Entry *table = 0;
    int used = 0;

    Side build_node(Trie::Node *node, int current_bit) {
        if (node == NULL) {
            /* Reached the end of the path */
            return Side(0, NULL);
        }

        const int other_bit = !current_bit;

        /* ID node or possible crossing - new node. */
        if (node->id != -1 || node->child[other_bit] != NULL) {
            Entry &entry = this->table[this->used];
            assert(entry.id == -1);
            entry.pos = this->used;
            entry.id = node->id;
            this->used++;

            auto left_path = build_node(node->child[0], 0);
            auto right_path = build_node(node->child[1], 1);
            entry.child[0] = left_path;
            entry.child[1] = right_path;
            return Side(1, &entry);
        }

        /* Create nodes for crossings or for IDs */
        if (current_bit == 0) {
            /* Continue 0 path - no node */
            assert(node->child[1] == NULL);
            auto path = build_node(node->child[0], 0);
            return Side(path.count + 1, path.jump);
        } else {
            /* Continue 1 path - no node */
            assert(node->child[0] == NULL);
            auto path = build_node(node->child[1], 1);
            return Side(path.count + 1, path.jump);
        }
    }
public:
    /* Build flatrie from a trie */
    void build(const Trie &trie) {
        delete[] this->table;
        /* NOTE: Memory management here is rather simplified. */
        this->table = new Entry[trie.size()];
        used = 1;
        Entry &entry = this->table[0];

        auto path = build_node(trie.root.child[0], 0);
        entry.child[0] = path;

        path = build_node(trie.root.child[1], 1);
        entry.child[1] = path;

        std::cout << "Used " << this->used
                  << " entries in the Flatrie table" << std::endl;
    }

    int query_string(const std::string &addr) {
        in_addr ip_parsed;
        int ret = inet_aton(addr.c_str(), &ip_parsed);
        assert(ret != 0);
        if (ret == 0)
            throw std::exception();
        uint32_t ip_network = ntohl(ip_parsed.s_addr);
        return this->query(ip_network);
    }

    /* Query by network byte order IP */
    int query(uint32_t ip) const {
        const Entry *cur = &this->table[0];
        int matched_id = -1;
        int hops = 0;

        for (;;) {
            const int bit = ip >> 31;
            auto &side = cur->child[bit];

            if (side.jump == NULL) {
                /* Nowhere to run */
                return matched_id;
            }

            /* Skip X bits */
            /* TODO: clz */
            for (int i = 0; i < side.count; i++) {
                const int cur_bit = ip >> 31;
                if (cur_bit != bit) {
                    return matched_id;
                }
                ip <<= 1;
            }
            cur = side.jump;
            hops += 1;
            if (cur->id != -1) {
                matched_id = cur->id;
            }
            continue;
        }
        return matched_id;
    }

    void show() {
        for (int i = 0; i < this->used; i++) {
            Entry &entry = this->table[i];
            entry.show(i);
        }
    }
};

#endif
