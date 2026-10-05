#pragma once

/**
 * @file DynArray.h
 * @brief Hand-written resizable dynamic array template.
 *
 * DATA STRUCTURE CONCEPT:
 * - Dynamic Array (Vector) with contiguous memory allocation.
 * - Amortized O(1) push_back() achieved via geometric expansion (doubling factor = 2).
 *   Proof: Appending N elements requires doubling at 1, 2, 4, 8, ..., N. Total copy cost is
 *   1 + 2 + 4 + ... + N = 2N - 1 < 2N operations. Thus, average cost per append is O(1).
 * - Cache locality: Sequential elements reside contiguously in RAM, maximizing L1/L2 CPU
 *   cache line hits compared to pointer-based structures like linked lists.
 * - Random access in O(1) via base pointer arithmetic: *(buffer + index).
 */

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <initializer_list>
#include <new>

namespace ds {

template <typename T>
class DynArray {
private:
    T* data_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;

    void reallocate(size_t new_cap) {
        if (new_cap < size_) new_cap = size_;
        if (new_cap == 0) {
            clear();
            delete[] reinterpret_cast<char*>(data_);
            data_ = nullptr;
            capacity_ = 0;
            return;
        }

        // Allocate raw memory to avoid default-constructing unused slots
        T* new_buffer = reinterpret_cast<T*>(new char[new_cap * sizeof(T)]);

        // Move or copy existing elements to the new buffer
        for (size_t i = 0; i < size_; ++i) {
            new (&new_buffer[i]) T(std::move_if_noexcept(data_[i]));
            data_[i].~T();
        }

        delete[] reinterpret_cast<char*>(data_);
        data_ = new_buffer;
        capacity_ = new_cap;
    }

public:
    // Iterators for standard range-based for loops
    using iterator = T*;
    using const_iterator = const T*;

    DynArray() = default;

    explicit DynArray(size_t initial_cap) {
        reserve(initial_cap);
    }

    DynArray(size_t count, const T& value) {
        reserve(count);
        for (size_t i = 0; i < count; ++i) {
            new (&data_[i]) T(value);
        }
        size_ = count;
    }

    DynArray(std::initializer_list<T> init) {
        reserve(init.size());
        for (const auto& item : init) {
            push_back(item);
        }
    }

    // Copy constructor (Deep Copy)
    DynArray(const DynArray& other) {
        reserve(other.size_);
        for (size_t i = 0; i < other.size_; ++i) {
            new (&data_[i]) T(other.data_[i]);
        }
        size_ = other.size_;
    }

    // Move constructor (O(1) transfer of ownership)
    DynArray(DynArray&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    // Copy assignment
    DynArray& operator=(const DynArray& other) {
        if (this != &other) {
            clear();
            reserve(other.size_);
            for (size_t i = 0; i < other.size_; ++i) {
                new (&data_[i]) T(other.data_[i]);
            }
            size_ = other.size_;
        }
        return *this;
    }

    // Move assignment
    DynArray& operator=(DynArray&& other) noexcept {
        if (this != &other) {
            clear();
            delete[] reinterpret_cast<char*>(data_);
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    ~DynArray() {
        clear();
        delete[] reinterpret_cast<char*>(data_);
    }

    size_t size() const noexcept { return size_; }
    size_t capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }

    void reserve(size_t new_cap) {
        if (new_cap > capacity_) {
            reallocate(new_cap);
        }
    }

    void clear() noexcept {
        for (size_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }
        size_ = 0;
    }

    void push_back(const T& val) {
        if (size_ == capacity_) {
            reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        }
        new (&data_[size_]) T(val);
        ++size_;
    }

    void push_back(T&& val) {
        if (size_ == capacity_) {
            reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        }
        new (&data_[size_]) T(std::move(val));
        ++size_;
    }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        }
        new (&data_[size_]) T(std::forward<Args>(args)...);
        ++size_;
    }

    void pop_back() {
        if (size_ > 0) {
            --size_;
            data_[size_].~T();
        }
    }

    T& operator[](size_t idx) noexcept {
        return data_[idx];
    }

    const T& operator[](size_t idx) const noexcept {
        return data_[idx];
    }

    T& at(size_t idx) {
        if (idx >= size_) {
            throw std::out_of_range("DynArray::at out of range");
        }
        return data_[idx];
    }

    const T& at(size_t idx) const {
        if (idx >= size_) {
            throw std::out_of_range("DynArray::at out of range");
        }
        return data_[idx];
    }

    T& front() { return data_[0]; }
    const T& front() const { return data_[0]; }
    T& back() { return data_[size_ - 1]; }
    const T& back() const { return data_[size_ - 1]; }

    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }

    iterator begin() noexcept { return data_; }
    iterator end() noexcept { return data_ + size_; }
    const_iterator begin() const noexcept { return data_; }
    const_iterator end() const noexcept { return data_ + size_; }
    const_iterator cbegin() const noexcept { return data_; }
    const_iterator cend() const noexcept { return data_ + size_; }
};

} // namespace ds
