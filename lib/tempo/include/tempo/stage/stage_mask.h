#pragma once

#include <cstddef>
#include <cstdint>

#include "tempo/core/type_list.h"

namespace tempo {

    /**
     * @brief Bitset over a compile-time stage type list.
     *
     * StageMask<Idle, Run, Error>::of<Idle, Run>() yields a mask with the bits matching
     * those types' positions in the type list. Indices map 1:1 to Conductor slot indices.
     *
     * Up to 32 stages.
     *
     * @tparam Stages The compile-time stage type list (same order as Conductor's).
     */
    template <typename... Stages>
    class StageMask {
    private:
        uint32_t m_bits = 0;

    public:
        static constexpr size_t STAGE_COUNT = sizeof...(Stages);
        static_assert(STAGE_COUNT <= 32, "StageMask supports up to 32 stages");

        constexpr StageMask() = default;

        /**
         * @brief Build a mask containing the given stage types.
         */
        template <typename... Selected>
        static constexpr StageMask of() {
            StageMask mask;
            ((mask.m_bits |= (uint32_t{1} << type_index_v<Selected, Stages...>) ), ...);
            return mask;
        }

        static constexpr StageMask none() {
            return StageMask{};
        }

        static constexpr StageMask all() {
            StageMask mask;
            mask.m_bits = 0xFFFFFFFFu;
            return mask;
        }

        template <typename S>
        constexpr bool contains() const {
            return (m_bits & (uint32_t{1} << type_index_v<S, Stages...>) ) != 0;
        }

        constexpr bool contains_index(size_t idx) const {
            return idx < 32 && (m_bits & (uint32_t{1} << idx)) != 0;
        }

        constexpr bool empty() const {
            return m_bits == 0;
        }
        constexpr uint32_t raw() const {
            return m_bits;
        }

        friend constexpr StageMask operator|(StageMask a, StageMask b) {
            StageMask r;
            r.m_bits = a.m_bits | b.m_bits;
            return r;
        }

        friend constexpr StageMask operator&(StageMask a, StageMask b) {
            StageMask r;
            r.m_bits = a.m_bits & b.m_bits;
            return r;
        }

        friend constexpr StageMask operator-(StageMask a, StageMask b) {
            StageMask r;
            r.m_bits = a.m_bits & ~b.m_bits;
            return r;
        }

        constexpr StageMask& operator|=(StageMask o) {
            m_bits |= o.m_bits;
            return *this;
        }
        constexpr StageMask& operator&=(StageMask o) {
            m_bits &= o.m_bits;
            return *this;
        }
        constexpr StageMask& operator-=(StageMask o) {
            m_bits &= ~o.m_bits;
            return *this;
        }

        friend constexpr bool operator==(StageMask a, StageMask b) {
            return a.m_bits == b.m_bits;
        }
        friend constexpr bool operator!=(StageMask a, StageMask b) {
            return a.m_bits != b.m_bits;
        }
    };

} // namespace tempo
