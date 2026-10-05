#pragma once

/**
 * @file Sort.h
 * @brief Hand-written Sorting Algorithms: Merge Sort (Stable, O(N log N)) and Insertion Sort (Adaptive, O(N^2)).
 *
 * DATA STRUCTURE / ALGORITHM CONCEPTS:
 * 1. Merge Sort:
 *    - Divide and Conquer paradigm.
 *    - Divides input array into two halves, recursively sorts each half, and merges them.
 *    - STABILITY: Preserves the relative order of elements with equal keys.
 *    - Time Complexity:
 *        Best, Average, Worst: \Theta(N \log N) comparisons.
 *    - Space Complexity: O(N) auxiliary buffer for merging.
 *    - Application in Project: Used to rank candidate routes by multi-criterion ordering:
 *      (1) balanced cost -> (2) travel time -> (3) distance -> (4) road ID sequence.
 *
 * 2. Insertion Sort:
 *    - Incremental algorithm.
 *    - Time Complexity: O(N^2) worst-case, O(N) best-case (nearly sorted).
 *    - Ideal demonstration for tiny arrays (N <= 16) with zero allocation overhead.
 */

#include "DynArray.h"
#include <utility>

namespace ds {

// Hand-written Insertion Sort
template <typename T, typename Compare>
void insertion_sort(DynArray<T>& arr, size_t left, size_t right, Compare comp) {
    if (right <= left) return;
    for (size_t i = left + 1; i <= right; ++i) {
        T key = std::move(arr[i]);
        size_t j = i;
        while (j > left && comp(key, arr[j - 1])) {
            arr[j] = std::move(arr[j - 1]);
            --j;
        }
        arr[j] = std::move(key);
    }
}

template <typename T, typename Compare>
void insertion_sort(DynArray<T>& arr, Compare comp) {
    if (arr.size() > 1) {
        insertion_sort(arr, 0, arr.size() - 1, comp);
    }
}

// Hand-written Merge Sort
namespace internal {

template <typename T, typename Compare>
void merge(DynArray<T>& arr, DynArray<T>& aux, size_t left, size_t mid, size_t right, Compare comp) {
    for (size_t k = left; k <= right; ++k) {
        aux[k] = std::move(arr[k]);
    }

    size_t i = left;
    size_t j = mid + 1;
    size_t k = left;

    while (i <= mid && j <= right) {
        // Use !comp(aux[j], aux[i]) to ensure STABILITY (left element favored on tie)
        if (!comp(aux[j], aux[i])) {
            arr[k++] = std::move(aux[i++]);
        } else {
            arr[k++] = std::move(aux[j++]);
        }
    }

    while (i <= mid) {
        arr[k++] = std::move(aux[i++]);
    }
    while (j <= right) {
        arr[k++] = std::move(aux[j++]);
    }
}

template <typename T, typename Compare>
void merge_sort_recursive(DynArray<T>& arr, DynArray<T>& aux, size_t left, size_t right, Compare comp) {
    if (left >= right) return;

    // Small-size optimization: use insertion sort for small partitions (cutoff = 8)
    if (right - left <= 8) {
        insertion_sort(arr, left, right, comp);
        return;
    }

    size_t mid = left + (right - left) / 2;
    merge_sort_recursive(arr, aux, left, mid, comp);
    merge_sort_recursive(arr, aux, mid + 1, right, comp);

    // If already sorted across boundary, skip merge (O(N) best case optimization)
    if (!comp(arr[mid + 1], arr[mid])) {
        return;
    }

    merge(arr, aux, left, mid, right, comp);
}

} // namespace internal

template <typename T, typename Compare>
void merge_sort(DynArray<T>& arr, Compare comp) {
    if (arr.size() <= 1) return;
    DynArray<T> aux(arr.size(), arr[0]); // allocate auxiliary buffer once
    internal::merge_sort_recursive(arr, aux, 0, arr.size() - 1, comp);
}

} // namespace ds
