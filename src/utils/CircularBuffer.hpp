/*
 CircularBuffer.hpp - Circular buffer library for Arduino.
 Copyright (c) 2017 Roberto Lo Giacco.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as 
 published by the Free Software Foundation, either version 3 of the 
 License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CIRCULAR_BUFFER_H_
#define CIRCULAR_BUFFER_H_
#include <stdint.h>
#include <stddef.h>

namespace Helper {
/** @private */
template<bool FITS8, bool FITS16> struct Index {
    using Type = uint32_t;
};

/** @private */
template<> struct Index<false, true> {
    using Type = uint16_t;
};

/** @private */
template<> struct Index<true, true> {
    using Type = uint8_t;
};
}

/**
 * @brief Implements a circular buffer that supports LIFO and FIFO operations.
 *
 * @tparam T The type of the data to store in the buffer.
 * @tparam S The maximum number of elements that can be stored in the buffer.
 * @tparam IT The data type of the index. Typically should be left as default.
 */
template<typename T, size_t S, typename IT = typename Helper::Index<
        (S <= UINT8_MAX), (S <= UINT16_MAX)>::Type> class CircularBuffer {
public:
    /**
     * @brief The buffer capacity.
     *
     * Read only as it cannot ever change.
     */
    static constexpr IT capacity = static_cast<IT>(S);

    /**
     * @brief Aliases the index type.
     *
     * Can be used to obtain the right index type with `decltype(buffer)::index_t`.
     */
    using index_t = IT;

    /**
     * @brief Create an empty circular buffer.
     */
    constexpr CircularBuffer();

    // disable the copy constructor
    /** @private */
    CircularBuffer(const CircularBuffer&) = delete;
    /** @private */
    CircularBuffer(CircularBuffer&&) = delete;

    // disable the assignment operator
    /** @private */
    CircularBuffer& operator=(const CircularBuffer&) = delete;
    /** @private */
    CircularBuffer& operator=(CircularBuffer&&) = delete;

    /**
     * @brief Adds an element to the end of buffer.
     *
     * @return `false` if the addition caused overwriting to an existing element.
     */
    bool push(T value);

    /**
     * @brief Removes an element from the beginning of the buffer.
     *
     * @warning Calling this operation on an empty buffer has an unpredictable behaviour.
     */
    T shift();

    /**
     * @brief Array-like access to buffer.
     *
     * Calling this operation using and index value greater than `size - 1` returns the tail element.
     *
     * @warning Calling this operation on an empty buffer has an unpredictable behaviour.
     */
    T operator [](IT index) const;

    /**
     * @brief Returns how many elements are actually stored in the buffer.
     *
     * @return The number of elements stored in the buffer.
     */
    IT inline size() const;

    /**
     * @brief Resets the buffer to a clean status, making all buffer positions available.
     *
     * @note This does not clean up any dynamically allocated memory stored in the buffer.
     * Clearing a buffer that points to heap-allocated memory may cause a memory leak, if it's not properly cleaned up.
     */
    void inline clear();

private:
    T buffer[S];
    T *head;
    T *tail;
    IT count;
};

template<typename T, size_t S, typename IT>
constexpr CircularBuffer<T, S, IT>::CircularBuffer() :
        head(buffer), tail(buffer), count(0) {
}

template<typename T, size_t S, typename IT>
bool CircularBuffer<T, S, IT>::push(T value) {
    if (++tail == buffer + capacity) {
        tail = buffer;
    }
    *tail = value;
    if (count == capacity) {
        if (++head == buffer + capacity) {
            head = buffer;
        }
        return false;
    } else {
        if (count++ == 0) {
            head = tail;
        }
        return true;
    }
}

template<typename T, size_t S, typename IT>
T CircularBuffer<T, S, IT>::shift() {
    if (count == 0)
        return *head;
    T result = *head++;
    if (head >= buffer + capacity) {
        head = buffer;
    }
    count--;
    return result;
}

template<typename T, size_t S, typename IT>
T CircularBuffer<T,S,IT>::operator [](IT index) const {
    if (index >= count) return *tail;
    return *(buffer + ((head - buffer + index) % capacity));
}

template<typename T, size_t S, typename IT>
IT inline CircularBuffer<T, S, IT>::size() const {
    return count;
}

template<typename T, size_t S, typename IT>
void inline CircularBuffer<T, S, IT>::clear() {
    head = tail = buffer;
    count = 0;
}

#endif
