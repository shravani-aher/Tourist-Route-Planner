#pragma once

/**
 * @file Queue.h
 * @brief Hand-written FIFO (First-In-First-Out) Queue data structure.
 *
 * DATA STRUCTURE CONCEPT:
 * - Queue ADT implementing FIFO order via a dynamic circular ring buffer.
 * - Time Complexity:
 *     - push(): O(1) amortized.
 *     - pop():  O(1) worst-case.
 *     - front(): O(1) worst-case.
 * - Application in Project:
 *     1. Breadth-First Search (BFS) reachability validation: Explores the graph layer-by-layer
 *        after any road closure to ensure that all mandatory/must-visit stops remain reachable.
 *     2. Simulation Event Queue: Queues periodic simulated environmental changes (traffic rushes,
 *        rain blocks, crowd spikes) to be processed in order.
 */

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <new>

namespace ds {

template <typename T>
class Queue {
private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t size_ = 0;

    void grow(size_t new_cap) {
        if (new_cap < size_) new_cap = size_;
        if (new_cap == 0) new_cap = 4;

        T* new_buf = reinterpret_cast<T*>(new char[new_cap * sizeof(T)]);
        for (size_t i = 0; i < size_; ++i) {
            size_t old_idx = (head_ + i) % capacity_;
            new (&new_buf[i]) T(std::move_if_noexcept(buffer_[old_idx]));
            buffer_[old_idx].~T();
        }

        delete[] reinterpret_cast<char*>(buffer_);
        buffer_ = new_buf;
        capacity_ = new_cap;
        head_ = 0;
        tail_ = size_;
    }

public:
    Queue() = default;

    explicit Queue(size_t initial_cap) {
        grow(initial_cap < 4 ? 4 : initial_cap);
    }

    Queue(const Queue& other) {
        if (other.capacity_ > 0) {
            grow(other.capacity_);
            for (size_t i = 0; i < other.size_; ++i) {
                size_t idx = (other.head_ + i) % other.capacity_;
                new (&buffer_[i]) T(other.buffer_[idx]);
            }
            size_ = other.size_;
            head_ = 0;
            tail_ = size_;
        }
    }

    Queue(Queue&& other) noexcept
        : buffer_(other.buffer_), capacity_(other.capacity_),
          head_(other.head_), tail_(other.tail_), size_(other.size_) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
        other.head_ = 0;
        other.tail_ = 0;
        other.size_ = 0;
    }

    Queue& operator=(const Queue& other) {
        if (this != &other) {
            clear();
            if (other.capacity_ > 0) {
                grow(other.capacity_);
                for (size_t i = 0; i < other.size_; ++i) {
                    size_t idx = (other.head_ + i) % other.capacity_;
                    new (&buffer_[i]) T(other.buffer_[idx]);
                }
                size_ = other.size_;
                head_ = 0;
                tail_ = size_;
            }
        }
        return *this;
    }

    Queue& operator=(Queue&& other) noexcept {
        if (this != &other) {
            clear();
            delete[] reinterpret_cast<char*>(buffer_);
            buffer_ = other.buffer_;
            capacity_ = other.capacity_;
            head_ = other.head_;
            tail_ = other.tail_;
            size_ = other.size_;

            other.buffer_ = nullptr;
            other.capacity_ = 0;
            other.head_ = 0;
            other.tail_ = 0;
            other.size_ = 0;
        }
        return *this;
    }

    ~Queue() {
        clear();
        delete[] reinterpret_cast<char*>(buffer_);
    }

    bool empty() const noexcept {
        return size_ == 0;
    }

    size_t size() const noexcept {
        return size_;
    }

    void push(const T& val) {
        if (size_ == capacity_) {
            grow(capacity_ == 0 ? 4 : capacity_ * 2);
        }
        new (&buffer_[tail_]) T(val);
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
    }

    void push(T&& val) {
        if (size_ == capacity_) {
            grow(capacity_ == 0 ? 4 : capacity_ * 2);
        }
        new (&buffer_[tail_]) T(std::move(val));
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
    }

    T pop() {
        if (size_ == 0) {
            throw std::underflow_error("Queue::pop called on empty queue");
        }
        T front_val = std::move(buffer_[head_]);
        buffer_[head_].~T();
        head_ = (head_ + 1) % capacity_;
        --size_;
        return front_val;
    }

    T& front() {
        if (size_ == 0) {
            throw std::underflow_error("Queue::front called on empty queue");
        }
        return buffer_[head_];
    }

    const T& front() const {
        if (size_ == 0) {
            throw std::underflow_error("Queue::front called on empty queue");
        }
        return buffer_[head_];
    }

    void clear() {
        for (size_t i = 0; i < size_; ++i) {
            size_t idx = (head_ + i) % capacity_;
            buffer_[idx].~T();
        }
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }
};

} // namespace ds
