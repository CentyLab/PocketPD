#pragma once

#include <cstdint>
#include <initializer_list>
#include <type_traits>

namespace tempo {

    template <typename TStageId>
    class StageMask {
        static_assert(std::is_enum_v<TStageId>, "TStageId must be a scoped enum");
        using underlying_t = std::underlying_type_t<TStageId>;

        static constexpr uint32_t mask_bit(TStageId id) {
            return uint32_t{1} << static_cast<underlying_t>(id);
        }

    public:
        constexpr StageMask() = default;

        constexpr StageMask(std::initializer_list<TStageId> ids) {
            for (const auto& id : ids) {
                m_bits |= mask_bit(id);
            }
        }

        static constexpr StageMask none() {
            return StageMask{};
        }

        static constexpr StageMask all() {
            const auto count = static_cast<underlying_t>(TStageId::COUNT_);
            static_assert(
                static_cast<std::size_t>(TStageId::COUNT_) <= 32,
                "Too many TStageIds; StageMask supports up to 32."
            );
            StageMask m;
            m.m_bits = (count >= 32) ? 0xFFFFFFFFu : ((uint32_t{1} << count) - 1u);
            return m;
        }

        constexpr bool contains(TStageId id) const {
            return (m_bits & mask_bit(id)) != 0;
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

        constexpr TStageId first() const {
            for (underlying_t i = 0; i < static_cast<underlying_t>(TStageId::COUNT_); ++i) {
                if (m_bits & (uint32_t{1} << i)) {
                    return static_cast<TStageId>(i);
                }
            }
            return TStageId::COUNT_;
        }

    private:
        uint32_t m_bits = 0;
    };

} // namespace tempo
