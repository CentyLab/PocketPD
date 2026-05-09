/**
 * @file output_gate.h
 * @brief Load-switch enable line abstraction.
 *
 */
#pragma once

namespace pocketpd {

    class OutputGate {
    public:
        virtual ~OutputGate() = default;

        virtual void enable() = 0;
        virtual void disable() = 0;
        virtual bool is_enabled() const = 0;

        void toggle() {
            if (is_enabled()) {
                disable();
            } else {
                enable();
            }
        }
    };

} // namespace pocketpd
