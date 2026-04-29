// === src/bus/EventQueue.h ===
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace tempo {

    /**
     * @brief A ring buffer. We could have used MASK = CAPACITY - 1 and enforce power of two sizing
     * but since CAPACITY is a compiled time constant, compiler won't generate __aeabi_uidivmod
     * subroutine. __aeabi_uidivmod is slow on Cortex-M0+ (RP2040 main core, no HW divide). ref:
     * https://godbolt.org/z/a4aMjTxhM
     *
     * @tparam TEvent The event type.
     * @tparam Capacity The capacity of the event queue. Must be a power of two.
     */
    template <typename Event, size_t Capacity>
    class EventQueue {
        static constexpr bool m_is_at_least_two = Capacity >= 2;
        static_assert(m_is_at_least_two, "Capacity must be >= 2");

        static constexpr bool m_is_copyable = std::is_trivially_copyable_v<Event>;
        static_assert(m_is_copyable, "TEvent variant must be trivially-copyable or POD");

        std::array<Event, Capacity> m_storage{};
        std::atomic<size_t> m_head{0};
        std::atomic<size_t> m_tail{0};

    public:
        static constexpr size_t CAPACITY = Capacity;

        constexpr size_t mod(size_t x) {
            return x % CAPACITY;
        }

        /**
         * @brief Push an event onto the queue.
         *
         * @param e The event to push.
         * @return true if the event was pushed successfully, false if the queue is full.
         */
        [[nodiscard("Please check if operation was successful")]]
        bool push(const Event& e) {
            const size_t head = m_head.load(std::memory_order_relaxed);
            const size_t next = mod(head + 1);
            if (next == m_tail.load(std::memory_order_acquire)) {
                return false; // next == tail -> buffer is full
            }

            m_storage[head] = e;
            m_head.store(next, std::memory_order_release);
            return true;
        }

        [[nodiscard("Please check if operation was successful")]]
        bool pop(Event& out) {
            const size_t tail = m_tail.load(std::memory_order_relaxed);
            if (tail == m_head.load(std::memory_order_acquire)) {
                return false; // tail == head -> buffer is empty
            }

            out = m_storage[tail];
            m_tail.store(mod(tail + 1), std::memory_order_release);
            return true;
        }

        bool empty() const {
            return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed);
        }
    };

} // namespace tempo
