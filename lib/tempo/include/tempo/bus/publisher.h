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

        // Publish an event. Returns true iff every backing queue accepted
        // the event. False indicates at least one queue was full; the
        // producer should decide whether to retry, drop, or panic. In
        // dual-core fan-out, partial success is possible: the event may
        // have landed in some queues but not others. The framework deliberately
        // does not roll back; the consumer side filters via std::get_if anyway.
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

} // namespace tempo
