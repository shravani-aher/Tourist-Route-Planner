#pragma once

/**
 * @file UnionFind.h
 * @brief Hand-written Disjoint Set Union (DSU / Union-Find) data structure.
 *
 * DATA STRUCTURE CONCEPT:
 * - Maintains a partition of N elements into disjoint sets.
 * - Optimizations:
 *     1. Union-by-Rank: Attaches the shorter tree under the root of the taller tree,
 *        preventing degeneration into linked lists.
 *     2. Path Compression: Flattens the tree structure along the traversal path during find(),
 *        pointing each visited node directly to the root.
 * - Time Complexity:
 *     - Amortized O(\alpha(N)) per operation, where \alpha is the inverse Ackermann function
 *       (\alpha(N) < 5 for all practical universe sizes).
 * - Application in Project:
 *     Calculates and visualizes the connected components of Pune city's road network
 *     in real-time when roads are dynamically closed/blocked, identifying isolated attractions.
 */

#include "DynArray.h"
#include <cstddef>

namespace ds {

class UnionFind {
private:
    DynArray<int> parent_;
    DynArray<int> rank_;
    int component_count_ = 0;

public:
    UnionFind() = default;

    explicit UnionFind(size_t n) {
        reset(n);
    }

    void reset(size_t n) {
        parent_.clear();
        rank_.clear();
        parent_.reserve(n);
        rank_.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            parent_.push_back(static_cast<int>(i));
            rank_.push_back(0);
        }
        component_count_ = static_cast<int>(n);
    }

    int find(int i) {
        if (parent_[i] == i) {
            return i;
        }
        // Path compression
        return parent_[i] = find(parent_[i]);
    }

    bool unite(int i, int j) {
        int root_i = find(i);
        int root_j = find(j);
        if (root_i == root_j) {
            return false;
        }

        // Union by rank
        if (rank_[root_i] < rank_[root_j]) {
            parent_[root_i] = root_j;
        } else if (rank_[root_i] > rank_[root_j]) {
            parent_[root_j] = root_i;
        } else {
            parent_[root_j] = root_i;
            rank_[root_i]++;
        }

        --component_count_;
        return true;
    }

    bool connected(int i, int j) {
        return find(i) == find(j);
    }

    int component_count() const noexcept {
        return component_count_;
    }

    size_t size() const noexcept {
        return parent_.size();
    }
};

} // namespace ds
