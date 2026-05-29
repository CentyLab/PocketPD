/**
 * @file table_view.h
 * @brief Reusable list/table view that owns its cursor and scroll window.
 */
#pragma once

#include <algorithm>
#include <cstdint>

#include <tempo/hardware/display.h>

namespace pocketpd {

    struct TableRow {
        const char* text = nullptr;
    };

    struct TableModel {
        const TableRow* rows = nullptr;
        uint8_t count = 0;
    };

    struct TableStyle {
        uint8_t row_height = 12;
        uint8_t first_y = 12;
        uint8_t text_x = 10;
        uint8_t cursor_x = 0;
        uint8_t visible_rows = 5;
        const char* cursor_glyph = ">";
    };

    class TableView {
    private:
        TableStyle m_style;
        uint8_t m_cursor = 0;
        uint8_t m_scroll_top = 0;

        /**
         * @brief Index of the top visible row. Sticky — moves only when the cursor leaves view.
         */
        uint8_t next_scroll_top(uint8_t count) const {
            const uint8_t cap = m_style.visible_rows;
            if (count <= cap) {
                return 0;
            }

            uint8_t top = m_scroll_top;
            if (m_cursor < top) {
                top = m_cursor;
            } else if (m_cursor >= top + cap) {
                top = m_cursor - cap + 1;
            }

            const auto max_top = count - cap;
            return top > max_top ? max_top : top;
        }

    public:
        explicit TableView(TableStyle style = {}) : m_style(style) {}

        uint8_t cursor() const {
            return m_cursor;
        }

        uint8_t capacity() const {
            return m_style.visible_rows;
        }

        void reset() {
            m_cursor = 0;
            m_scroll_top = 0;
        }

        bool move(int delta, uint8_t count) {
            if (count == 0) {
                return false;
            }

            const int next = std::clamp(m_cursor + delta, 0, count - 1);
            if (next == m_cursor) {
                return false;
            }

            m_cursor = next;
            return true;
        }

        void render(tempo::Display& display, const TableModel& model) {
            m_scroll_top = next_scroll_top(model.count);

            display.clear();

            const auto last = std::min<uint8_t>(model.count, m_scroll_top + m_style.visible_rows);
            for (auto i = m_scroll_top; i < last; ++i) {
                const auto y = m_style.first_y + (i - m_scroll_top) * m_style.row_height;
                if (i == m_cursor) {
                    display.draw_text(m_style.cursor_x, y, m_style.cursor_glyph);
                }

                display.draw_text(m_style.text_x, y, model.rows[i].text);
            }

            display.flush();
        }
    };

} // namespace pocketpd
