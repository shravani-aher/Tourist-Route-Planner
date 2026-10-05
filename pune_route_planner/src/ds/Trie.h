#pragma once

/**
 * @file Trie.h
 * @brief Hand-written Trie (Prefix Tree) data structure for autocomplete search.
 *
 * DATA STRUCTURE CONCEPT:
 * - Tree data structure where edges represent characters and nodes represent prefixes.
 * - Time Complexity:
 *     - insert(key): O(L) where L is the length of key string.
 *     - search_prefix(prefix): O(P + K) where P is prefix length and K is the number of matches.
 * - Application in Project:
 *     Powers instantaneous autocomplete suggestions in the browser search box as the user
 *     types place names (e.g. typing "shan" -> "Shaniwar Wada", "pata" -> "Pataleshwar Cave Temple").
 */

#include "DynArray.h"
#include <string>
#include <cctype>
#include <algorithm>

namespace ds {

class Trie {
public:
    struct Match {
        std::string key;
        std::string place_id;
        std::string full_name;
    };

private:
    struct TrieNode {
        TrieNode* children[128] = {nullptr};
        bool is_terminal = false;
        std::string place_id;
        std::string full_name;

        ~TrieNode() {
            for (int i = 0; i < 128; ++i) {
                delete children[i];
            }
        }
    };

    TrieNode* root_ = nullptr;

    static std::string normalize(const std::string& s) {
        std::string res;
        res.reserve(s.size());
        for (char c : s) {
            res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return res;
    }

    void collect_matches(TrieNode* node, std::string& current_prefix, DynArray<Match>& results, size_t limit) const {
        if (!node || results.size() >= limit) return;

        if (node->is_terminal) {
            results.push_back(Match{current_prefix, node->place_id, node->full_name});
            if (results.size() >= limit) return;
        }

        for (int c = 0; c < 128; ++c) {
            if (node->children[c]) {
                current_prefix.push_back(static_cast<char>(c));
                collect_matches(node->children[c], current_prefix, results, limit);
                current_prefix.pop_back();
            }
        }
    }

public:
    Trie() : root_(new TrieNode()) {}

    ~Trie() {
        delete root_;
    }

    // Non-copyable for simplicity
    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;

    Trie(Trie&& other) noexcept : root_(other.root_) {
        other.root_ = new TrieNode();
    }

    Trie& operator=(Trie&& other) noexcept {
        if (this != &other) {
            delete root_;
            root_ = other.root_;
            other.root_ = new TrieNode();
        }
        return *this;
    }

    void insert(const std::string& word, const std::string& place_id, const std::string& full_name) {
        std::string norm = normalize(word);
        TrieNode* curr = root_;
        for (char c : norm) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (uc >= 128) uc = '?';
            if (!curr->children[uc]) {
                curr->children[uc] = new TrieNode();
            }
            curr = curr->children[uc];
        }
        curr->is_terminal = true;
        curr->place_id = place_id;
        curr->full_name = full_name;
    }

    DynArray<Match> search_prefix(const std::string& prefix, size_t limit = 10) const {
        DynArray<Match> results;
        std::string norm = normalize(prefix);
        TrieNode* curr = root_;

        for (char c : norm) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (uc >= 128) uc = '?';
            if (!curr->children[uc]) {
                return results; // no matches
            }
            curr = curr->children[uc];
        }

        std::string current_prefix = norm;
        collect_matches(curr, current_prefix, results, limit);
        return results;
    }
};

} // namespace ds
