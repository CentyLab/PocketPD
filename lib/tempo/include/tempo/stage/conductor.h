#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "tempo/core/time.h"
#include "tempo/stage/null_stage.h"
#include "tempo/stage/stage.h"

namespace tempo {

    inline constexpr size_t MAX_STAGES = 32;

    template <typename StageId>
    class Conductor {
    private:
        using stage_t = tempo::Stage<StageId>;
        const Clock& m_clock;

        static inline NullStage<StageId> s_null_stage{};

        stage_t* lookup_stage(StageId id) const {
            const auto idx = static_cast<size_t>(id);
            return idx < MAX_STAGES ? m_stages[idx] : &s_null_stage;
        }

        std::array<stage_t*, MAX_STAGES> m_stages{};
        stage_t* m_current = &s_null_stage;
        StageId m_current_id{};
        StageId m_pending_id{};
        bool m_has_pending = false;

    public:
        Conductor(const Clock& clock) : m_clock(clock) {
            for (auto& s : m_stages) {
                s = &s_null_stage;
            }

            static_assert(
                static_cast<std::size_t>(StageId::COUNT_) <= MAX_STAGES,
                "Too many TStageIds; raise MAX_STAGES or shrink the enum."
            );
        }

        void register_stage(StageId id, stage_t& stage) {
            const auto idx = static_cast<size_t>(id);
            m_stages[idx] = &stage;
            stage.set_clock(m_clock);
        }

        void start(StageId initial) {
            m_current_id = initial;
            m_current = lookup_stage(initial);
            m_current->on_enter(*this);
        }

        void request(StageId next) {
            m_pending_id = next;
            m_has_pending = true;
        }

        [[nodiscard]]
        bool has_pending() const {
            return m_has_pending;
        }

        bool apply_pending_transition() {
            if (!m_has_pending) {
                return false;
            }

            const StageId next_id = m_pending_id;
            m_has_pending = false;

            if (next_id == m_current_id) {
                return false;
            }

            stage_t* next = lookup_stage(next_id);

            m_current->on_exit(*this);
            m_current_id = next_id;
            m_current = next;
            m_current->on_enter(*this);
            return true;
        }

        void tick(uint32_t now_ms) {
            m_current->on_tick(*this, now_ms);
        }

        StageId current_id() const {
            return m_current_id;
        }

        const stage_t* current_stage() const {
            return m_current;
        }

        [[nodiscard]]
        const char* current_name() const {
            return m_current->name();
        }

        const stage_t* stage_for(StageId id) const {
            const auto idx = static_cast<size_t>(id);
            return idx < MAX_STAGES ? m_stages[idx] : &s_null_stage;
        }
    };

} // namespace tempo
