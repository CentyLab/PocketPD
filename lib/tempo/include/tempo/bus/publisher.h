//
// Concrete IPublisherT implementations. Two flavors:
//
//   QueuePublisherT  — wraps a single EventQueue. Used by Application
//                      (single-core) where there's exactly one queue.

#pragma once

#include "event_queue.h"

namespace tempo {

    template <typename Event>
    class Publisher {
    public:
        virtual ~Publisher() = default;
        /**
         * @brief Publish an event. Returns true iff every backing queue accepted the event. False
         * indicates at least one queue was full; the producer should decide whether to retry, drop,
         * or panic. In dual-core fan-out, partial success is possible: the event may have landed in
         * some queues but not others. The implementation should not deliberately roll back.
         *
         * @param e The event to publish.
         * @return true if the event was published to all queues successfully, false otherwise.
         */
        virtual bool publish(const Event& e) = 0;

    protected:
        Publisher() = default;
    };

    template <typename Event, size_t Capacity>
    class QueuePublisher : public Publisher<Event> {

    private:
        EventQueue<Event, Capacity>& m_queue;

    public:
        explicit QueuePublisher(EventQueue<Event, Capacity>& q) : m_queue(q) {}

        bool publish(const Event& event) override {
            return m_queue.push(event);
        }
    };

    template <typename Event, typename... Stages>
    class Application;

    template <typename Derived, typename Event>
    class UsePublisher;

    namespace detail {

        /**
         * @brief Storage layer for UsePublisher. Holds the PublisherSlot. Split out so that
         * Derived (friend of UsePublisher for CRTP) does not gain access to the slot. Derived
         * reaches the publish path through `publish()` exposed by UsePublisher.
         */
        template <typename Derived, typename Event>
        class UsePublisherStorage {
            class PublisherSlot {
                Publisher<Event>* m_publisher = nullptr;

                void attach(Publisher<Event>& publisher) {
                    m_publisher = &publisher;
                }

                template <typename E, typename... Stages>
                friend class tempo::Application;
                friend class UsePublisherStorage<Derived, Event>;
            };

            PublisherSlot m_publisher_slot;

            template <typename E, typename... Stages>
            friend class tempo::Application;
            friend class tempo::UsePublisher<Derived, Event>;

        protected:
            UsePublisherStorage() = default;

            bool publish(const Event& event) {
                if (m_publisher_slot.m_publisher == nullptr) {
                    return false;
                }
                return m_publisher_slot.m_publisher->publish(event);
            }

            void attach_publisher(Publisher<Event>& publisher) {
                m_publisher_slot.attach(publisher);
            }
        };

    } // namespace detail

    /**
     * @brief CRTP mixin that injects a Publisher<Event> into Tasks and Stages registered with an
     * Application.
     *
     * Derived classes call `publish(event)` to emit.
     *
     * @tparam Derived  The host class (CRTP).
     * @tparam Event    The Application's event variant type.
     */
    template <typename Derived, typename Event>
    class UsePublisher : public detail::UsePublisherStorage<Derived, Event> {
        UsePublisher() = default;

        template <typename E, typename... Stages>
        friend class Application;
        friend Derived;

    public:
        /**
         * @brief DO NOT USE in production. Test-only seam for unit tests that construct a
         * Stage or Task directly, without an Application. Calling this method manually is undefined
         * behavior.
         *
         * @param publisher The publisher to attach.
         */
        void attach_publisher_INTERNAL_DO_NOT_USE(Publisher<Event>& publisher) {
            this->attach_publisher(publisher);
        }
    };

} // namespace tempo
