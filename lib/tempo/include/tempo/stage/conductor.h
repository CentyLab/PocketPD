#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "tempo/core/type_list.h"
#include "tempo/stage/null_stage.h"
#include "tempo/stage/stage.h"
#include "tempo/stage/stage_mask.h"

namespace tempo {

    /**
     * @brief Owns a set of Stages identified by type, dispatches lifecycle callbacks, and
     * applies pending transitions on demand.
     *
     * Stage identity = type. Each Stage S has a slot index equal to its position in the
     * Stages... type list. Use register_stage<S>(stage) to bind a slot to a Stage instance,
     * and request<S>() to schedule a transition.
     *
     * Up to 32 stages (mirrors StageMask).
     *
     * @tparam Stages The compile-time stage type list.
     */
    template <typename... Stages>
    class Conductor {
    public:
        using StageType = Stage<Stages...>;
        using StageMaskType = StageMask<Stages...>;

        static constexpr size_t STAGE_COUNT = sizeof...(Stages);
        static_assert(STAGE_COUNT > 0, "Conductor needs at least one stage type");
        static_assert(STAGE_COUNT <= 32, "Conductor supports up to 32 stage types");

        template <typename S>
        static constexpr size_t index_of() {
            return type_index_v<S, Stages...>;
        }

    private:
        static inline NullStage<Stages...> s_null_stage{};

        std::array<StageType*, STAGE_COUNT> m_slots{};
        StageType* m_current = &s_null_stage;
        size_t m_current_idx = STAGE_COUNT;

        bool m_has_pending = false;
        size_t m_pending_idx = 0;

        StageType* lookup(size_t idx) const {
            if (idx >= STAGE_COUNT) {
                return &s_null_stage;
            }

            StageType* s = m_slots[idx];
            return s ? s : &s_null_stage;
        }

    public:
        Conductor() {
            for (auto& s : m_slots) {
                s = nullptr;
            }
        }

        ~Conductor() = default;

        Conductor(const Conductor&) = delete;
        Conductor& operator=(const Conductor&) = delete;
        Conductor(Conductor&&) = delete;
        Conductor& operator=(Conductor&&) = delete;

        template <typename S>
        void register_stage(S& stage) {
            constexpr size_t idx = index_of<S>();
            m_slots[idx] = &stage;
        }

        template <typename S>
        void start() {
            constexpr size_t idx = index_of<S>();
            m_current_idx = idx;
            m_current = lookup(idx);
            m_current->on_enter(*this);
        }

        /**
         * @brief Request a transition to stage S. on_enter(Conductor&) is called when the
         * transition is applied.
         *
         * Optional payload-passing form: `request<S>(args...)` calls `S::prepare(args...)`
         * on the target slot immediately, then schedules the transition. The target stage
         * uses `prepare` to stash entry context that its subsequent `on_enter` consumes.
         * Stages without a `prepare` overload accept only the zero-arg `request<S>()` form.
         *
         * No allocation: arguments are forwarded directly to `prepare`. The configured
         * state persists on the stage instance until `on_enter` runs on the next tick.
         */
        template <typename S, typename... Args>
        void request(Args&&... args) {
            constexpr size_t idx = index_of<S>();
            m_pending_idx = idx;
            m_has_pending = true;

            if constexpr (sizeof...(Args) > 0) {
                if (auto* slot = m_slots[idx]) {
                    static_cast<S*>(slot)->prepare(std::forward<Args>(args)...);
                }
            }
        }

        bool has_pending() const {
            return m_has_pending;
        }

        /**
         * @brief Apply a pending transition. Same-stage requests are honored: the active
         * stage's `on_exit` runs, then `on_enter` runs again on the same instance. Pair with
         * `request<S>(payload)` to re-enter a stage with new entry context (e.g. switching
         * an internal mode) without inventing a separate code path for in-stage updates.
         */
        bool apply_pending_transition() {
            if (!m_has_pending) {
                return false;
            }

            const size_t next_idx = m_pending_idx;
            m_has_pending = false;

            StageType* next = lookup(next_idx);

            m_current->on_exit(*this);
            m_current_idx = next_idx;
            m_current = next;
            m_current->on_enter(*this);
            return true;
        }

        void tick(uint32_t now_ms) {
            m_current->on_tick(*this, now_ms);
        }

        size_t current_index() const {
            return m_current_idx;
        }

        const StageType* current_stage() const {
            return m_current;
        }

        const char* current_name() const {
            return m_current->name();
        }

        const StageType* stage_at(size_t idx) const {
            return lookup(idx);
        }

        const char* name_at(size_t idx) const {
            return lookup(idx)->name();
        }
    };

} // namespace tempo
