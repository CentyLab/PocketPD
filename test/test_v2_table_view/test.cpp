/**
 * @file test.cpp
 * @brief TableView render + cursor/scroll-window unit tests against MockDisplay.
 */
#define VERSION "\"test\""

#include <array>

#include <MockDisplay.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "v2/ui/table_view.h"

using namespace pocketpd;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::StrEq;

TEST(TableView, RendersRowsWithCursorGlyph) {
    NiceMock<MockDisplay> display;
    std::array<TableRow, 2> rows{};
    rows[0].text = "Alpha";
    rows[1].text = "Beta";

    TableModel model;
    model.rows = rows.data();
    model.count = 2;

    EXPECT_CALL(display, draw_text(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(display, clear()).Times(1);
    EXPECT_CALL(display, draw_text(0, 12, StrEq(">"))).Times(1);
    EXPECT_CALL(display, draw_text(10, 12, StrEq("Alpha"))).Times(1);
    EXPECT_CALL(display, draw_text(10, 24, StrEq("Beta"))).Times(1);
    EXPECT_CALL(display, draw_text(0, 24, StrEq(">"))).Times(0);
    EXPECT_CALL(display, flush()).Times(1);

    TableView view;
    view.render(display, model);
}

TEST(TableView, DrawsRowTextVerbatim) {
    // The view is content-agnostic: it draws whatever the stage formatted into text,
    // including leading markers like a checkbox. It does not know what a row means.
    NiceMock<MockDisplay> display;
    std::array<TableRow, 2> rows{};
    rows[0].text = "[X] On";
    rows[1].text = "[ ] Off";

    TableModel model;
    model.rows = rows.data();
    model.count = 2;

    EXPECT_CALL(display, draw_text(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(display, draw_text(10, 12, StrEq("[X] On"))).Times(1);
    EXPECT_CALL(display, draw_text(10, 24, StrEq("[ ] Off"))).Times(1);

    TableView view;
    view.render(display, model);
}

TEST(TableView, MoveClampsAndReportsChange) {
    TableView view;
    EXPECT_FALSE(view.move(-1, 3)); // already at top
    EXPECT_EQ(view.cursor(), 0);
    EXPECT_TRUE(view.move(2, 3)); // 0 -> 2
    EXPECT_EQ(view.cursor(), 2);
    EXPECT_FALSE(view.move(5, 3)); // clamped at bottom, no change
    EXPECT_EQ(view.cursor(), 2);
    EXPECT_TRUE(view.move(-2, 3)); // 2 -> 0
    EXPECT_EQ(view.cursor(), 0);
    EXPECT_FALSE(view.move(1, 0)); // empty list
}

TEST(TableView, ScrollsWindowToKeepCursorVisible) {
    // A view with capacity 2, cursor moved to row 2, derives scroll_top=1 on its own.
    NiceMock<MockDisplay> display;
    std::array<TableRow, 4> rows{};
    rows[0].text = "r0";
    rows[1].text = "r1";
    rows[2].text = "r2";
    rows[3].text = "r3";

    TableModel model;
    model.rows = rows.data();
    model.count = 4;

    TableStyle style;
    style.visible_rows = 2;
    TableView view{style};
    view.move(2, 4); // cursor -> 2, below the initial window

    EXPECT_CALL(display, draw_text(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(display, draw_text(10, 12, StrEq("r1"))).Times(1);
    EXPECT_CALL(display, draw_text(10, 24, StrEq("r2"))).Times(1);
    EXPECT_CALL(display, draw_text(0, 24, StrEq(">"))).Times(1);
    EXPECT_CALL(display, draw_text(10, _, StrEq("r0"))).Times(0);
    EXPECT_CALL(display, draw_text(10, _, StrEq("r3"))).Times(0);

    view.render(display, model);
}

TEST(TableView, StickyScrollPersistsAcrossRenders) {
    // Scroll state lives in the view across frames: paging down then back up one
    // keeps the window sticky without the consumer tracking anything.
    NiceMock<MockDisplay> display;
    std::array<TableRow, 4> rows{};
    rows[0].text = "r0";
    rows[1].text = "r1";
    rows[2].text = "r2";
    rows[3].text = "r3";

    TableModel model;
    model.rows = rows.data();
    model.count = 4;

    TableStyle style;
    style.visible_rows = 2;
    TableView view{style};

    view.move(3, 4); // cursor -> 3, window becomes r2/r3
    view.render(display, model);

    EXPECT_CALL(display, draw_text(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(display, draw_text(10, 12, StrEq("r2"))).Times(1);
    EXPECT_CALL(display, draw_text(10, 24, StrEq("r3"))).Times(1);
    EXPECT_CALL(display, draw_text(10, _, StrEq("r0"))).Times(0);
    EXPECT_CALL(display, draw_text(10, _, StrEq("r1"))).Times(0);

    view.move(-1, 4); // cursor -> 2, still inside window: stays r2/r3
    view.render(display, model);
}

TEST(TableView, ScrollsBackUpWhenCursorMovesAboveWindow) {
    NiceMock<MockDisplay> display;
    std::array<TableRow, 4> rows{};
    rows[0].text = "r0";
    rows[1].text = "r1";
    rows[2].text = "r2";
    rows[3].text = "r3";

    TableModel model;
    model.rows = rows.data();
    model.count = 4;

    TableStyle style;
    style.visible_rows = 2;
    TableView view{style};

    view.move(3, 4); // window r2/r3
    view.render(display, model);

    EXPECT_CALL(display, draw_text(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(display, draw_text(0, 12, StrEq(">"))).Times(1);
    EXPECT_CALL(display, draw_text(10, 12, StrEq("r0"))).Times(1);
    EXPECT_CALL(display, draw_text(10, 24, StrEq("r1"))).Times(1);
    EXPECT_CALL(display, draw_text(10, _, StrEq("r2"))).Times(0);
    EXPECT_CALL(display, draw_text(10, _, StrEq("r3"))).Times(0);

    view.move(-3, 4); // cursor -> 0, window follows up to r0/r1
    view.render(display, model);
}

TEST(TableView, NoScrollWhenAllRowsFit) {
    // count <= capacity: scroll_top stays 0, every row drawn top-down.
    NiceMock<MockDisplay> display;
    std::array<TableRow, 3> rows{};
    rows[0].text = "a";
    rows[1].text = "b";
    rows[2].text = "c";

    TableModel model;
    model.rows = rows.data();
    model.count = 3;

    TableStyle style;
    style.visible_rows = 5;
    TableView view{style};
    view.move(2, 3); // cursor -> 2

    EXPECT_CALL(display, draw_text(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(display, draw_text(10, 12, StrEq("a"))).Times(1);
    EXPECT_CALL(display, draw_text(10, 24, StrEq("b"))).Times(1);
    EXPECT_CALL(display, draw_text(10, 36, StrEq("c"))).Times(1);
    EXPECT_CALL(display, draw_text(0, 36, StrEq(">"))).Times(1);

    view.render(display, model);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
