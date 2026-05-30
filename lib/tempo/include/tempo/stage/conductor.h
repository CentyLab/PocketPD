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
     * and the navigation ops (push/pop/replace/reset_root/reset_path) to move between stages.
     *
     * Up to 32 stages (mirrors StageMask).
     *
     * @tparam Event The application's event type. Forwarded into Stage so the active stage can
     * receive `on_event` callbacks via `dispatch_event`.
     * @tparam Stages The compile-time stage type list.
     */
    template <typename Event, typename... Stages>
    class Conductor {
    public:
        using StageType = Stage<Event, Stages...>;
        using StageMaskType = StageMask<Stages...>;

        static constexpr size_t STAGE_COUNT = sizeof...(Stages);
        static_assert(STAGE_COUNT > 0, "Conductor needs at least one stage type");
        static_assert(STAGE_COUNT <= 32, "Conductor supports up to 32 stage types");

        template <typename S>
        static constexpr size_t index_of() {
            return type_index_v<S, Stages...>;
        }

    private:
        static inline NullStage<Event, Stages...> s_null_stage{};

        std::array<StageType*, STAGE_COUNT> m_slots{};
        StageType* m_current = &s_null_stage;
        size_t m_current_idx = STAGE_COUNT;

        bool m_has_pending = false;
        size_t m_pending_idx = 0;

        std::array<size_t, 8> m_nav{};
        size_t m_nav_depth = 0;

        // —— Low-level transition primitives (navigation goes through the public ops)

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

        template <size_t... Is>
        void request_index_impl(size_t idx, std::index_sequence<Is...>) {
            ((idx == Is ? request<type_at_t<Is, Stages...>>() : void()), ...);
        }

        void request_index(size_t idx) {
            request_index_impl(idx, std::make_index_sequence<STAGE_COUNT>{});
        }

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
        void start(uint32_t now_ms) {
            constexpr size_t idx = index_of<S>();
            m_nav[0] = idx;
            m_nav_depth = 1;
            m_current_idx = idx;
            m_current = lookup(idx);
            m_current->on_enter(*this, now_ms);
        }

        /** @brief Push S onto the stack. No-op if S is already the top. */
        template <typename S, typename... Args>
        void push(Args&&... args) {
            if (m_nav_depth && m_nav[m_nav_depth - 1] == index_of<S>()) {
                return;
            }
            if (m_nav_depth < m_nav.size()) {
                m_nav[m_nav_depth++] = index_of<S>();
            }
            request<S>(std::forward<Args>(args)...);
        }

        /** @brief Pop the top, returning to the stage below. No-op at the root. */
        void pop() {
            if (m_nav_depth > 1) {
                request_index(m_nav[--m_nav_depth - 1]);
            }
        }

        /** @brief Replace the top with S. */
        template <typename S, typename... Args>
        void replace(Args&&... args) {
            if (m_nav_depth == 0) {
                return; // no current top to replace
            }
            m_nav[m_nav_depth - 1] = index_of<S>();
            request<S>(std::forward<Args>(args)...);
        }

        /** @brief Reset the stack to `[S]`. */
        template <typename S, typename... Args>
        void reset_root(Args&&... args) {
            m_nav[0] = index_of<S>();
            m_nav_depth = 1;
            request<S>(std::forward<Args>(args)...);
        }

        /** @brief Reset the stack to `Path...` and enter its top (lower stages get no on_enter). */
        template <typename... Path>
        void reset_path() {
            static_assert(sizeof...(Path) > 0, "reset_path needs at least one stage");
            static_assert(sizeof...(Path) <= 8, "path deeper than the nav stack");
            m_nav_depth = 0;
            ((m_nav[m_nav_depth++] = index_of<Path>()), ...);
            request_index(m_nav[m_nav_depth - 1]);
        }

        bool has_pending() const {
            return m_has_pending;
        }

        bool apply_pending_transition(uint32_t now_ms) {
            if (!m_has_pending) {
                return false;
            }

            const size_t next_idx = m_pending_idx;
            m_has_pending = false;

            StageType* next = lookup(next_idx);

            m_current->on_exit(*this, now_ms);
            m_current_idx = next_idx;
            m_current = next;
            m_current->on_enter(*this, now_ms);
            return true;
        }

        void tick(uint32_t now_ms) {
            m_current->on_tick(*this, now_ms);
        }

        /**
         * @brief Deliver `event` to the currently-active stage's `on_event`.
         *
         * Called by `Application::tick` for every event drained from the ISR and task queues,
         * after the scheduler has dispatched the event to all tasks whose stage filter matches.
         */
        void dispatch_event(const Event& event, uint32_t now_ms) {
            m_current->on_event(*this, event, now_ms);
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
