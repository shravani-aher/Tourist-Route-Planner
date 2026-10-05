#pragma once

/**
 * @file MinHeap.h
 * @brief Hand-written array-based Binary Min-Heap with decrease-key capability.
 *
 * DATA STRUCTURE CONCEPT:
 * - Complete Binary Tree stored in a contiguous array (level-order traversal).
 * - Heap Invariant: For any node at index i (0-based):
 *     Parent(i) = (i - 1) / 2
 *     LeftChild(i) = 2 * i + 1
 *     RightChild(i) = 2 * i + 2
 *     Key(Parent(i)) <= Key(i)
 * - Time Complexity:
 *     - peek(): O(1) accessing root at index 0.
 *     - push(): O(log N) append at leaf and sift-up.
 *     - pop():  O(log N) swap root with last leaf, shrink, and sift-down.
 *     - decrease_key(): O(log N) enabled by position-lookup array pos_[id], which gives
 *       the heap index in O(1), followed by sift-up in O(log N).
 *   Without the position tracking array, decrease-key would require O(N) linear search
 *   to locate the element in the heap array before sifting.
 * - Used directly in Dijkstra's single-source shortest path algorithm to achieve
 *   O((V + E) log V) optimal running time.
 */

#include "DynArray.h"
#include <stdexcept>
#include <utility>

namespace ds {

template <typename Priority = double, typename Id = int>
class MinHeap {
public:
    struct Element {
        Id id;
        Priority priority;
    };

private:
    DynArray<Element> heap_;
    // pos_[id] maps an external ID (e.g., node index in [0, max_id]) to its current index in heap_
    DynArray<int> pos_;

    void swap_nodes(size_t i, size_t j) {
        std::swap(heap_[i], heap_[j]);
        if (static_cast<size_t>(heap_[i].id) < pos_.size()) {
            pos_[heap_[i].id] = static_cast<int>(i);
        }
        if (static_cast<size_t>(heap_[j].id) < pos_.size()) {
            pos_[heap_[j].id] = static_cast<int>(j);
        }
    }

    void sift_up(size_t idx) {
        while (idx > 0) {
            size_t parent = (idx - 1) / 2;
            if (heap_[idx].priority < heap_[parent].priority) {
                swap_nodes(idx, parent);
                idx = parent;
            } else {
                break;
            }
        }
    }

    void sift_down(size_t idx) {
        size_t n = heap_.size();
        while (true) {
            size_t left = 2 * idx + 1;
            size_t right = 2 * idx + 2;
            size_t smallest = idx;

            if (left < n && heap_[left].priority < heap_[smallest].priority) {
                smallest = left;
            }
            if (right < n && heap_[right].priority < heap_[smallest].priority) {
                smallest = right;
            }

            if (smallest != idx) {
                swap_nodes(idx, smallest);
                idx = smallest;
            } else {
                break;
            }
        }
    }

public:
    MinHeap() = default;

    explicit MinHeap(size_t max_id_capacity) {
        ensure_id_capacity(max_id_capacity);
    }

    void ensure_id_capacity(size_t max_id) {
        if (max_id >= pos_.size()) {
            size_t old_size = pos_.size();
            size_t new_size = max_id + 1;
            pos_.reserve(new_size);
            for (size_t i = old_size; i < new_size; ++i) {
                pos_.push_back(-1);
            }
        }
    }

    bool empty() const noexcept {
        return heap_.empty();
    }

    size_t size() const noexcept {
        return heap_.size();
    }

    bool contains(Id id) const {
        if (static_cast<size_t>(id) >= pos_.size()) return false;
        return pos_[id] != -1;
    }

    const Element& peek() const {
        if (heap_.empty()) {
            throw std::underflow_error("MinHeap::peek on empty heap");
        }
        return heap_[0];
    }

    void push(Id id, Priority priority) {
        ensure_id_capacity(static_cast<size_t>(id));
        if (contains(id)) {
            decrease_key(id, priority);
            return;
        }

        size_t idx = heap_.size();
        heap_.push_back(Element{id, priority});
        pos_[id] = static_cast<int>(idx);
        sift_up(idx);
    }

    Element pop() {
        if (heap_.empty()) {
            throw std::underflow_error("MinHeap::pop on empty heap");
        }

        Element top = heap_[0];
        pos_[top.id] = -1;

        if (heap_.size() == 1) {
            heap_.pop_back();
            return top;
        }

        heap_[0] = heap_.back();
        pos_[heap_[0].id] = 0;
        heap_.pop_back();

        sift_down(0);
        return top;
    }

    bool decrease_key(Id id, Priority new_priority) {
        if (static_cast<size_t>(id) >= pos_.size()) return false;
        int idx = pos_[id];
        if (idx == -1) return false;

        // In a min-heap, decrease-key only applies if new_priority <= current priority
        if (new_priority < heap_[idx].priority) {
            heap_[idx].priority = new_priority;
            sift_up(static_cast<size_t>(idx));
            return true;
        }
        return false;
    }

    void clear() {
        for (const auto& elem : heap_) {
            if (static_cast<size_t>(elem.id) < pos_.size()) {
                pos_[elem.id] = -1;
            }
        }
        heap_.clear();
    }
};

} // namespace ds
