#pragma once

/**
 * @file Stack.h
 * @brief Hand-written LIFO (Last-In-First-Out) Stack data structure.
 *
 * DATA STRUCTURE CONCEPT:
 * - Stack ADT adhering to LIFO policy.
 * - Backed by ds::DynArray for O(1) amortized push and O(1) pop/peek.
 * - Application in Project:
 *     Used as an Undo History Stack for dynamic road network mutations (blocking roads,
 *     crowd spikes, traffic changes). Each mutation pushes an inverse action record onto
 *     the stack. When "Undo" is invoked, the top record is popped and reversed, restoring
 *     the graph state to its prior configuration in O(1) time.
 */

#include "DynArray.h"
#include <stdexcept>
#include <utility>

namespace ds {

template <typename T>
class Stack {
private:
    DynArray<T> data_;

public:
    Stack() = default;

    bool empty() const noexcept {
        return data_.empty();
    }

    size_t size() const noexcept {
        return data_.size();
    }

    void push(const T& val) {
        data_.push_back(val);
    }

    void push(T&& val) {
        data_.push_back(std::move(val));
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        data_.emplace_back(std::forward<Args>(args)...);
    }

    T pop() {
        if (data_.empty()) {
            throw std::underflow_error("Stack::pop called on empty stack");
        }
        T top_elem = std::move(data_.back());
        data_.pop_back();
        return top_elem;
    }

    T& top() {
        if (data_.empty()) {
            throw std::underflow_error("Stack::top called on empty stack");
        }
        return data_.back();
    }

    const T& top() const {
        if (data_.empty()) {
            throw std::underflow_error("Stack::top called on empty stack");
        }
        return data_.back();
    }

    T& peek() { return top(); }
    const T& peek() const { return top(); }

    void clear() {
        data_.clear();
    }
};

} // namespace ds
