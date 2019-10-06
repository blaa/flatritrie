/*
 * Copyright 2019 Tomasz bla Fortuna. All rights reserved.
 * License: MIT
 * bla@thera.be, https://github.com/blaa/flatritrie
 */

#ifndef _FLATRITRIE_H_
#define _FLATRITRIE_H_

#include <tritrie.hpp>

/*
 * A specialized dictionary-like structure for mapping keys (IP addresses) to
 * values (like int or pointer). Solves efficiently a problem which in hardware
 * is usually solved by using the TCAM memory.
 *
 * Tabelarized finite state automata for fast searching of information related
 * to an IP address - like geoip information, white/black listing, etc.
 *
 * - Finds most detailed match (matched /32 trumps /16 match).
 * - Immutable after being build.
 * - Optimized for querying.
 * - Works with big networks (like /8 IPV4, /48 in ipv6).
 * - Doesn't require expansion of ip/masks.
 * - Limits random memory reads when possible.
 */

namespace Tritrie {

template<int BITS=DEFAULT_BITS, int PAGE_SIZE=10000>
class Flat {
protected:
    constexpr static int CHILDREN = (1<<BITS);
    constexpr static int BITS_COMPLEMENT = (32 - BITS);

    struct Entry {
        /* ID if reached this place */
        int id = -1;

        /* {000 -> Entry, 001 -> Entry, 010 -> NULL, ...} for BITS=3 */
        Entry *child[CHILDREN];
    };

    /* Alocating multiple pages at once to improve cache locality */
    std::vector<Entry *> pages;
    Entry *page_current = 0;
    int used_in_page = 0;
    int used_total = 0;

    Entry *alloc_entry() {
        /* Allocate new page if required */
        if (used_in_page == PAGE_SIZE or page_current == NULL) {
            this->page_current = new Entry[PAGE_SIZE];
            this->pages.push_back(this->page_current);
            used_in_page = 0;
        }

        /* Alloc entry within a page */
        Entry *entry = &this->page_current[this->used_in_page];
        this->used_in_page++;
        this->used_total++;
        return entry;
    }

    Entry *build_node(typename Tritrie<BITS>::Node *node) {
        if (node == NULL) {
            /* Reached the end of the path */
            return NULL;
        }

        Entry *entry = this->alloc_entry();
        entry->id = node->id;

        for (int i = 0; i < CHILDREN; i++) {
            entry->child[i] = build_node(node->child[i]);
        }
        return entry;
    }

    void cleanup() {
        for (auto *page: this->pages) {
            delete[] page;
        }
        this->pages.clear();
        this->used_in_page = 0;
        this->used_total = 0;
        this->page_current = NULL;
    }

    /* Don't copy. */
    Flat(const Flat &flatritrie);

public:
    Flat() {}

    ~Flat() {
        this->cleanup();
    }

    void build(Tritrie<BITS> &trie) {
        this->cleanup();
        this->build_node(&trie.root);
    }

    int query_string(const std::string &addr) const {
        in_addr ip_parsed;
        int ret = inet_aton(addr.c_str(), &ip_parsed);
        if (ret == 0)
            throw std::exception();

        uint32_t ip_network = ntohl(ip_parsed.s_addr);
        return this->query(ip_network);
    }

    int query(uint32_t ip) const {
        /* Querying uninitialized structure will fail */
        assert(this->used_total > 0);

        const Entry *cur = &this->pages[0][0];

        int matched_id = -1;
        for (;;) {
            const int tri = ip >> (BITS_COMPLEMENT);
            const auto *child = cur->child[tri];

            if (child == NULL) {
                /* Nowhere to run */
                return matched_id;
            }
            cur = child;
            if (cur->id != -1) {
                matched_id = cur->id;
            }
            ip <<= BITS;
        }
        return matched_id;
    }

    int size() const {
        return this->used;
    }

    void debug() {
        std::cout << "Flatritrie debug stats:" << std::endl
                  << "  allocated pages = " << this->pages.size()
                  << " of size " << PAGE_SIZE << std::endl
                  << "  entries total = " << this->used_total
                  << " on last page = " << this->used_in_page
                  << std::endl;
    }
};

};
#endif
