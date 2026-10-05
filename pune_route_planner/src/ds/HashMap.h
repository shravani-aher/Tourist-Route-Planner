#pragma once

/**
 * @file HashMap.h
 * @brief Hand-written Hash Map with Separate Chaining collision resolution.
 *
 * DATA STRUCTURE CONCEPT:
 * - Hash Table with array of buckets and singly linked lists for collision chains.
 * - Hash Function:
 *     - FNV-1a (Fowler-Noll-Vo) 64-bit hash algorithm for std::string.
 *     - Multiplicative/Splitmix64 mixing for integral keys.
 * - Load Factor (\lambda = N / M):
 *     When load factor exceeds threshold (default 0.75), rehashing is triggered.
 *     Rehashing allocates a bucket array of 2x capacity and re-inserts all existing
 *     nodes into their new bucket indices (hash % new_capacity).
 * - Time Complexity:
 *     - Average Case: O(1) lookup, insertion, and deletion under Uniform Hashing Assumption (SUHA).
 *     - Worst Case: O(N) when all keys hash to the same bucket (forming an N-length chain).
 * - Used for mapping place IDs ("SW") -> node indices (0..9) and road IDs ("e01") -> Road structs.
 */

#include "DynArray.h"
#include <string>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <stdexcept>

namespace ds {

// Default Hash functors
template <typename Key>
struct Hash;

// Specialization for std::string using FNV-1a 64-bit
template <>
struct Hash<std::string> {
    uint64_t operator()(const std::string& s) const noexcept {
        uint64_t hash = 14695981039346656037ULL; // FNV offset basis
        for (char c : s) {
            hash ^= static_cast<unsigned char>(c);
            hash *= 1099511628211ULL;            // FNV prime
        }
        return hash;
    }
};

// Specialization for 32/64-bit integers using multiplicative hashing
template <typename T>
struct IntHash {
    uint64_t operator()(T val) const noexcept {
        uint64_t x = static_cast<uint64_t>(val);
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }
};

template <> struct Hash<int> : IntHash<int> {};
template <> struct Hash<uint32_t> : IntHash<uint32_t> {};
template <> struct Hash<int64_t> : IntHash<int64_t> {};
template <> struct Hash<uint64_t> : IntHash<uint64_t> {};

template <typename Key, typename Value, typename Hasher = Hash<Key>>
class HashMap {
public:
    struct Node {
        Key key;
        Value value;
        Node* next;

        Node(const Key& k, const Value& v, Node* n = nullptr)
            : key(k), value(v), next(n) {}
        Node(Key&& k, Value&& v, Node* n = nullptr)
            : key(std::move(k)), value(std::move(v)), next(n) {}
    };

private:
    Node** buckets_ = nullptr;
    size_t bucket_count_ = 0;
    size_t size_ = 0;
    float max_load_factor_ = 0.75f;
    Hasher hasher_;

    size_t get_bucket_index(const Key& key, size_t cap) const {
        return static_cast<size_t>(hasher_(key) % cap);
    }

    void init_buckets(size_t cap) {
        bucket_count_ = cap;
        buckets_ = new Node*[bucket_count_](); // zero-initialized
        size_ = 0;
    }

    void destroy_all() {
        if (buckets_) {
            for (size_t i = 0; i < bucket_count_; ++i) {
                Node* curr = buckets_[i];
                while (curr) {
                    Node* next = curr->next;
                    delete curr;
                    curr = next;
                }
            }
            delete[] buckets_;
            buckets_ = nullptr;
        }
        size_ = 0;
        bucket_count_ = 0;
    }

    void rehash(size_t new_cap) {
        Node** old_buckets = buckets_;
        size_t old_cap = bucket_count_;

        buckets_ = new Node*[new_cap]();
        bucket_count_ = new_cap;

        // Re-distribute nodes into new buckets without allocating new nodes
        for (size_t i = 0; i < old_cap; ++i) {
            Node* curr = old_buckets[i];
            while (curr) {
                Node* next = curr->next;
                size_t new_idx = get_bucket_index(curr->key, new_cap);
                curr->next = buckets_[new_idx];
                buckets_[new_idx] = curr;
                curr = next;
            }
        }
        delete[] old_buckets;
    }

public:
    explicit HashMap(size_t initial_buckets = 16, float max_lf = 0.75f)
        : max_load_factor_(max_lf) {
        init_buckets(initial_buckets < 4 ? 4 : initial_buckets);
    }

    // Copy Constructor
    HashMap(const HashMap& other)
        : max_load_factor_(other.max_load_factor_), hasher_(other.hasher_) {
        init_buckets(other.bucket_count_);
        for (size_t i = 0; i < other.bucket_count_; ++i) {
            Node* curr = other.buckets_[i];
            while (curr) {
                insert(curr->key, curr->value);
                curr = curr->next;
            }
        }
    }

    // Move Constructor
    HashMap(HashMap&& other) noexcept
        : buckets_(other.buckets_), bucket_count_(other.bucket_count_),
          size_(other.size_), max_load_factor_(other.max_load_factor_),
          hasher_(std::move(other.hasher_)) {
        other.buckets_ = nullptr;
        other.bucket_count_ = 0;
        other.size_ = 0;
    }

    // Copy Assignment
    HashMap& operator=(const HashMap& other) {
        if (this != &other) {
            destroy_all();
            max_load_factor_ = other.max_load_factor_;
            hasher_ = other.hasher_;
            init_buckets(other.bucket_count_);
            for (size_t i = 0; i < other.bucket_count_; ++i) {
                Node* curr = other.buckets_[i];
                while (curr) {
                    insert(curr->key, curr->value);
                    curr = curr->next;
                }
            }
        }
        return *this;
    }

    // Move Assignment
    HashMap& operator=(HashMap&& other) noexcept {
        if (this != &other) {
            destroy_all();
            buckets_ = other.buckets_;
            bucket_count_ = other.bucket_count_;
            size_ = other.size_;
            max_load_factor_ = other.max_load_factor_;
            hasher_ = std::move(other.hasher_);

            other.buckets_ = nullptr;
            other.bucket_count_ = 0;
            other.size_ = 0;
        }
        return *this;
    }

    ~HashMap() {
        destroy_all();
    }

    size_t size() const noexcept { return size_; }
    size_t bucket_count() const noexcept { return bucket_count_; }
    bool empty() const noexcept { return size_ == 0; }
    float load_factor() const noexcept {
        return bucket_count_ > 0 ? static_cast<float>(size_) / bucket_count_ : 0.0f;
    }

    bool contains(const Key& key) const {
        return find(key) != nullptr;
    }

    Value* find(const Key& key) {
        if (bucket_count_ == 0) return nullptr;
        size_t idx = get_bucket_index(key, bucket_count_);
        Node* curr = buckets_[idx];
        while (curr) {
            if (curr->key == key) {
                return &(curr->value);
            }
            curr = curr->next;
        }
        return nullptr;
    }

    const Value* find(const Key& key) const {
        if (bucket_count_ == 0) return nullptr;
        size_t idx = get_bucket_index(key, bucket_count_);
        Node* curr = buckets_[idx];
        while (curr) {
            if (curr->key == key) {
                return &(curr->value);
            }
            curr = curr->next;
        }
        return nullptr;
    }

    bool insert(const Key& key, const Value& val) {
        if (load_factor() >= max_load_factor_) {
            rehash(bucket_count_ * 2);
        }

        size_t idx = get_bucket_index(key, bucket_count_);
        Node* curr = buckets_[idx];
        while (curr) {
            if (curr->key == key) {
                return false; // key already exists
            }
            curr = curr->next;
        }

        buckets_[idx] = new Node(key, val, buckets_[idx]);
        ++size_;
        return true;
    }

    void insert_or_assign(const Key& key, const Value& val) {
        if (load_factor() >= max_load_factor_) {
            rehash(bucket_count_ * 2);
        }

        size_t idx = get_bucket_index(key, bucket_count_);
        Node* curr = buckets_[idx];
        while (curr) {
            if (curr->key == key) {
                curr->value = val;
                return;
            }
            curr = curr->next;
        }

        buckets_[idx] = new Node(key, val, buckets_[idx]);
        ++size_;
    }

    Value& operator[](const Key& key) {
        Value* existing = find(key);
        if (existing) {
            return *existing;
        }
        insert(key, Value{});
        return *find(key);
    }

    bool erase(const Key& key) {
        if (bucket_count_ == 0) return false;
        size_t idx = get_bucket_index(key, bucket_count_);
        Node* curr = buckets_[idx];
        Node* prev = nullptr;

        while (curr) {
            if (curr->key == key) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    buckets_[idx] = curr->next;
                }
                delete curr;
                --size_;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    void clear() {
        for (size_t i = 0; i < bucket_count_; ++i) {
            Node* curr = buckets_[i];
            while (curr) {
                Node* next = curr->next;
                delete curr;
                curr = next;
            }
            buckets_[i] = nullptr;
        }
        size_ = 0;
    }

    // Helper: Collect all keys
    DynArray<Key> keys() const {
        DynArray<Key> k_list;
        k_list.reserve(size_);
        for (size_t i = 0; i < bucket_count_; ++i) {
            Node* curr = buckets_[i];
            while (curr) {
                k_list.push_back(curr->key);
                curr = curr->next;
            }
        }
        return k_list;
    }
};

} // namespace ds
