/**
 * GoogleTest suite for NormalStage (PDO + PPS branches).
 *
 * Drives App::Conductor with MockDisplay, MockPdSink, MockOutputGate and
 * asserts entry RDO call, R-SHORT output toggle, L-LONG → ProfilePicker
 * SELECT, snapshot-driven render, PPS encoder edits + bounds, increment
 * cycling, and AdjustMode flip.
 */
#define VERSION "\"test\""

#include <MockDisplay.h>
#include <MockOutputGate.h>
#include <MockPdSink.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tempo/bus/event_queue.h>
#include <tempo/bus/publisher.h>
#include <tempo/stage/conductor.h>

#include "v2/app.h"
#include "v2/stages/energy_stage.h"
#include "v2/stages/normal_stage.h"
#include "v2/stages/profile_picker_stage.h"

using namespace pocketpd;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

using TestConductor = App::Conductor;

using TestQueue = tempo::EventQueue<Event, 32>;
using TestPublisher = tempo::QueuePublisher<Event, 32>;

namespace {
    template <typename T>
    const T* pop_as(TestQueue& q) {
        static Event last;
        if (!q.pop(last)) {
            return nullptr;
        }
        return std::get_if<T>(&last);
    }
}

TEST(NormalStage, OnEnterPdoProfileRequestsFixedPdo) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;

    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo(2)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(sink, set_pps_pdo).Times(0);

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);

    normal.prepare(2);
    conductor.start<NormalStage>(0);
}

TEST(NormalStage, OnEnterPpsProfileResetsTargetsToDefaults) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;

    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(1)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, set_pdo).Times(0);
    EXPECT_CALL(sink, set_pps_pdo).Times(0);  // PpsControlTask owns sink writes

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);

    normal.prepare(1);
    conductor.start<NormalStage>(0);
    EXPECT_EQ(normal.target_mv(), 5000);
    EXPECT_EQ(normal.target_ma(), 1000);
}

TEST(NormalStage, ReEnteringSamePpsProfilePreservesEdits) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(1)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, set_pps_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(1);
    conductor.start<NormalStage>(0);

    normal.on_event(conductor, EncoderEvent{-2}, 0);
    ASSERT_EQ(normal.target_mv(), 7000);

    normal.prepare(1);
    normal.on_enter(conductor, 0);
    EXPECT_EQ(normal.target_mv(), 7000);
    EXPECT_EQ(normal.target_ma(), 1000);
}

TEST(NormalStage, SwitchingPpsProfilesResetsTargetsToNewMinimum) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, is_index_pps(2)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(1)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, pdo_min_voltage_mv(2)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, pdo_max_voltage_mv(2)).WillRepeatedly(Return(20000));
    EXPECT_CALL(sink, pdo_max_current_ma(2)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, set_pps_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(1);
    conductor.start<NormalStage>(0);
    normal.on_event(conductor, EncoderEvent{-100}, 0);
    ASSERT_NE(normal.target_mv(), 3300);

    normal.prepare(2);
    normal.on_enter(conductor, 0);
    EXPECT_EQ(normal.target_mv(), 5000);
    EXPECT_EQ(normal.target_ma(), 1000);
}

TEST(NormalStage, OnEnterWithNegativeIndexRendersPassthrough) {
    using ::testing::_;
    using ::testing::AnyNumber;
    using ::testing::HasSubstr;

    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));
    EXPECT_CALL(sink, set_pdo).Times(0);
    EXPECT_CALL(sink, set_pps_pdo).Times(0);
    EXPECT_CALL(display, clear()).Times(::testing::AtLeast(1));
    EXPECT_CALL(display, draw_text(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(display, draw_text(_, _, HasSubstr("Passthrough")))
        .Times(::testing::AtLeast(1));
    EXPECT_CALL(display, flush()).Times(::testing::AtLeast(1));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(-1);
    conductor.start<NormalStage>(0);
}

TEST(NormalStage, RShortToggleEnablesAndDisablesOutput) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    bool enabled = false;
    EXPECT_CALL(gate, is_enabled()).WillRepeatedly(::testing::ReturnPointee(&enabled));
    EXPECT_CALL(gate, enable()).WillRepeatedly([&] { enabled = true; });
    EXPECT_CALL(gate, disable()).WillRepeatedly([&] { enabled = false; });
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(0);
    conductor.start<NormalStage>(0);

    normal.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);
    EXPECT_TRUE(enabled);

    normal.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);
    EXPECT_FALSE(enabled);
}

TEST(NormalStage, LLongRequestsMenu) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    MenuStage menu(display);
    TestConductor conductor;
    conductor.register_stage(normal);
    conductor.register_stage(menu);
    normal.prepare(0);
    conductor.start<NormalStage>(0);

    normal.on_event(conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition(0));
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<MenuStage>());
}

TEST(NormalStage, EncoderEventsIgnoredInPdoBranch) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, set_pps_pdo).Times(0);

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(0);
    conductor.start<NormalStage>(0);

    ::testing::Mock::VerifyAndClearExpectations(&gate);
    EXPECT_CALL(gate, enable).Times(0);
    EXPECT_CALL(gate, disable).Times(0);

    normal.on_event(conductor, EncoderEvent{3}, 0);
    EXPECT_FALSE(conductor.has_pending());
}

namespace {

    struct PpsHarness {
        NiceMock<MockDisplay> display;
        NiceMock<MockPdSink> sink;
        NiceMock<MockOutputGate> gate;
        NormalStage normal{display, sink, gate};
        TestConductor conductor;

        explicit PpsHarness(int pdo_index = 1, int v_lo = 3300, int v_hi = 11000, int a_hi = 3000) {
            EXPECT_CALL(sink, is_index_pps(pdo_index)).WillRepeatedly(Return(true));
            EXPECT_CALL(sink, pdo_min_voltage_mv(pdo_index)).WillRepeatedly(Return(v_lo));
            EXPECT_CALL(sink, pdo_max_voltage_mv(pdo_index)).WillRepeatedly(Return(v_hi));
            EXPECT_CALL(sink, pdo_max_current_ma(pdo_index)).WillRepeatedly(Return(a_hi));
            EXPECT_CALL(sink, set_pps_pdo).WillRepeatedly(Return(true));
            conductor.register_stage(normal);
            normal.prepare(static_cast<int8_t>(pdo_index));
            conductor.start<NormalStage>(0);
        }
    };

} // namespace

TEST(NormalStage, EncoderDeltaInVoltageModeAdjustsTargetMv) {
    PpsHarness h;
    h.normal.on_event(h.conductor, EncoderEvent{-3}, 0);
    EXPECT_EQ(h.normal.target_mv(), 8000);
    EXPECT_EQ(h.normal.adjust_mode(), AdjustMode::VOLTAGE);
}

TEST(NormalStage, EncoderDeltaInCurrentModeAdjustsTargetMa) {
    PpsHarness h;
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::SHORT}, 0);
    ASSERT_EQ(h.normal.adjust_mode(), AdjustMode::CURRENT);

    h.normal.on_event(h.conductor, EncoderEvent{-4}, 0);
    EXPECT_EQ(h.normal.target_ma(), 3000);
}

TEST(NormalStage, EncoderShortPressCyclesIncrementIndex) {
    PpsHarness h;
    EXPECT_EQ(h.normal.voltage_increment_index(), 0);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    EXPECT_EQ(h.normal.voltage_increment_index(), 1);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    EXPECT_EQ(h.normal.voltage_increment_index(), 2);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    EXPECT_EQ(h.normal.voltage_increment_index(), 0);

    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::SHORT}, 0);
    ASSERT_EQ(h.normal.adjust_mode(), AdjustMode::CURRENT);
    EXPECT_EQ(h.normal.current_increment_index(), 0);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    EXPECT_EQ(h.normal.current_increment_index(), 1);
    EXPECT_EQ(h.normal.voltage_increment_index(), 0);
}

TEST(NormalStage, EncoderEditUsesActiveIncrementMagnitude) {
    PpsHarness h;
    ASSERT_EQ(h.normal.voltage_increment_index(), 0);

    h.normal.on_event(h.conductor, EncoderEvent{-1}, 0);
    EXPECT_EQ(h.normal.target_mv(), 6000);
}

TEST(NormalStage, EncoderEditClampsTargetVoltageToPdoBounds) {
    PpsHarness h(1, 3300, 11000, 3000);

    EXPECT_CALL(h.sink, set_pps_pdo(1, 11000, ::testing::_)).WillRepeatedly(Return(true));
    h.normal.on_event(h.conductor, EncoderEvent{-20}, 0);
    EXPECT_EQ(h.normal.target_mv(), 11000);

    h.normal.on_event(h.conductor, EncoderEvent{20}, 0);
    EXPECT_EQ(h.normal.target_mv(), 3300);
}

TEST(NormalStage, EncoderEditClampsCurrentAtPpsMinimum) {
    PpsHarness h(1, 3300, 11000, 3000);
    h.normal.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::SHORT}, 0);
    ASSERT_EQ(h.normal.adjust_mode(), AdjustMode::CURRENT);

    EXPECT_CALL(h.sink, set_pps_pdo).WillRepeatedly(Return(true));
    h.normal.on_event(h.conductor, EncoderEvent{-20}, 0);
    ASSERT_EQ(h.normal.target_ma(), 3000);
    h.normal.on_event(h.conductor, EncoderEvent{50}, 0);
    EXPECT_EQ(h.normal.target_ma(), PPSMode::MIN_CURRENT_MA);
}

TEST(NormalStage, EncoderEditSnapsTargetVoltageToPpsStepGrid) {
    PpsHarness h(1, 3300, 11000, 3000);
    EXPECT_CALL(h.sink, set_pps_pdo).WillRepeatedly(Return(true));
    h.normal.on_event(h.conductor, EncoderEvent{-1}, 0);
    EXPECT_EQ(h.normal.target_mv() % PPS_VOLTAGE_STEP_MV, 0);
    h.normal.on_event(h.conductor, EncoderEvent{-1}, 0);
    EXPECT_EQ(h.normal.target_mv() % PPS_VOLTAGE_STEP_MV, 0);
}

TEST(NormalStage, LShortDoesNothingInPdoBranch) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, set_pps_pdo).Times(0);

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(0);
    conductor.start<NormalStage>(0);

    const auto mode_before = normal.adjust_mode();
    normal.on_event(conductor, ButtonEvent{ButtonId::L, Gesture::SHORT}, 0);
    EXPECT_EQ(normal.adjust_mode(), mode_before);
}

TEST(NormalStage, OnEnterPdoBranchRendersVAReadoutAndPdoIndex) {
    using ::testing::AtLeast;
    using ::testing::HasSubstr;
    using ::testing::StrEq;

    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(2)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, pdo_max_voltage_mv(2)).WillRepeatedly(Return(20000));
    EXPECT_CALL(sink, pdo_max_current_ma(2)).WillRepeatedly(Return(5000));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    EXPECT_CALL(gate, is_enabled()).WillRepeatedly(Return(false));

    EXPECT_CALL(display, draw_text(::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber());
    EXPECT_CALL(display, clear()).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(1, 14, StrEq("V"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(1, 48, StrEq("A"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(::testing::_, 28, HasSubstr("20000 mV"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(::testing::_, 62, HasSubstr("5000 mA"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(::testing::_, 63, HasSubstr("[2]"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(110, 64, StrEq("PDO"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(::testing::_, ::testing::_, StrEq("OFF"))).Times(AtLeast(1));
    EXPECT_CALL(display, flush()).Times(AtLeast(1));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(2);
    normal.on_event(
        conductor,
        SensorEvent{LoadReading{0, 5000, 1234}, SupplyReading{0, 20000, true}},
        0
    );
    conductor.start<NormalStage>(0);
}

TEST(NormalView, LockedRendersPadlock) {
    using ::testing::_;
    NiceMock<MockDisplay> display;

    EXPECT_CALL(
        display,
        draw_xbm(
            NormalView::PADLOCK_X, NormalView::PADLOCK_Y,
            NormalView::PADLOCK_W, NormalView::PADLOCK_H,
            bitmap::PADLOCK.data()
        )
    ).Times(1);

    NormalViewModel vm{};
    vm.mode = FixedMode{};
    vm.output_enabled = false;
    vm.locked = true;
    vm.active_pdo_index = 0;

    NormalView::render(display, vm);
}

TEST(NormalView, UnlockedDoesNotDrawPadlock) {
    using ::testing::_;
    NiceMock<MockDisplay> display;

    EXPECT_CALL(
        display,
        draw_xbm(_, _, NormalView::PADLOCK_W, NormalView::PADLOCK_H, bitmap::PADLOCK.data())
    ).Times(0);

    NormalViewModel vm{};
    vm.mode = FixedMode{};
    vm.output_enabled = false;
    vm.locked = false;
    vm.active_pdo_index = 0;

    NormalView::render(display, vm);
}

TEST(NormalView, ReadoutHiddenKeepsLabelsHidesValues) {
    using ::testing::_;
    using ::testing::AtLeast;
    using ::testing::HasSubstr;
    using ::testing::StrEq;
    NiceMock<MockDisplay> display;

    EXPECT_CALL(display, draw_text(_, _, _)).Times(::testing::AnyNumber());
    EXPECT_CALL(display, draw_text(1, 14, StrEq("V"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(1, 48, StrEq("A"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(_, 14, HasSubstr("5."))).Times(0);
    EXPECT_CALL(display, draw_text(_, 48, HasSubstr("1."))).Times(0);

    NormalViewModel vm{};
    vm.mode = FixedMode{};
    vm.output_enabled = false;
    vm.readout_visible = false;
    vm.active_pdo_index = 0;
    vm.load_reading = LoadReading{0, 5000, 1234};
    vm.supply_reading = SupplyReading{0, 20000, true};

    NormalView::render(display, vm);
}

TEST(NormalStage, ComboLongTogglesLock) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(0);
    conductor.start<NormalStage>(0);

    EXPECT_FALSE(normal.locked());
    normal.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    EXPECT_TRUE(normal.locked());
    normal.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    EXPECT_FALSE(normal.locked());
}

TEST(NormalStage, LockedIgnoresRShort) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));
    EXPECT_CALL(gate, disable()).Times(::testing::AnyNumber());
    EXPECT_CALL(gate, enable()).Times(0);

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(0);
    conductor.start<NormalStage>(0);

    normal.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    ASSERT_TRUE(normal.locked());

    normal.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);
}

TEST(NormalStage, LockedIgnoresEncoder) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(1)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(1)).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma(1)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, set_pps_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(1);
    conductor.start<NormalStage>(0);

    const int32_t before = normal.target_mv();
    normal.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    normal.on_event(conductor, EncoderEvent{2}, 0);
    EXPECT_EQ(normal.target_mv(), before);
}

TEST(NormalStage, OnEnterResetsLocked) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(0);
    conductor.start<NormalStage>(0);

    normal.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    ASSERT_TRUE(normal.locked());

    normal.prepare(0);
    normal.on_enter(conductor, 0);
    EXPECT_FALSE(normal.locked());
}

TEST(NormalStage, LockedIgnoresLLong) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    ProfilePickerStage picker(display, sink);
    TestConductor conductor;
    conductor.register_stage(normal);
    conductor.register_stage(picker);
    normal.prepare(0);
    conductor.start<NormalStage>(0);

    normal.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    ASSERT_TRUE(normal.locked());

    normal.on_event(conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);
    EXPECT_TRUE(normal.locked());
}

TEST(NormalStage, LockedIgnoresRLong) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    EnergyStage energy(display, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    conductor.register_stage(energy);
    normal.prepare(0);
    conductor.start<NormalStage>(0);

    normal.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    ASSERT_TRUE(normal.locked());

    normal.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::LONG}, 0);
    EXPECT_TRUE(normal.locked());
}

namespace {
    // Pops events from the queue and returns the first PpsTargetEvent matching `match`,
    // or nullptr if the queue is exhausted. `pop_as` already consumes one event per call.
    template <typename Pred>
    bool find_pps_event(TestQueue& q, Pred match) {
        while (true) {
            const PpsTargetEvent* evt = pop_as<PpsTargetEvent>(q);
            if (evt == nullptr) return false;     // queue empty
            if (match(*evt)) return true;
        }
    }
}

TEST(NormalStagePublishing, EmitsPpsTargetOnPpsEntry) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;

    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(::testing::_)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(::testing::_)).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma(::testing::_)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, set_pps_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestQueue queue;
    TestPublisher publisher(queue);
    normal.attach_publisher_INTERNAL_DO_NOT_USE(publisher);

    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(1);
    conductor.start<NormalStage>(0);

    EXPECT_TRUE(find_pps_event(queue, [](const PpsTargetEvent& e) {
        return e.pdo_index == 1 && e.target_mv == 5000 && e.target_ma == 1000;
    }));
}

TEST(NormalStagePublishing, EmitsInactivePpsOnFixedEntry) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;

    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo(2)).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestQueue queue;
    TestPublisher publisher(queue);
    normal.attach_publisher_INTERNAL_DO_NOT_USE(publisher);

    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(2);
    conductor.start<NormalStage>(0);

    EXPECT_TRUE(find_pps_event(queue, [](const PpsTargetEvent& e) {
        return e.pdo_index == -1 && e.target_mv == 0 && e.target_ma == 0;
    }));
}

TEST(NormalStagePublishing, EmitsInactivePpsOnPassthroughEntry) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, pdo_count()).WillRepeatedly(Return(0));

    NormalStage normal(display, sink, gate);
    TestQueue queue;
    TestPublisher publisher(queue);
    normal.attach_publisher_INTERNAL_DO_NOT_USE(publisher);

    TestConductor conductor;
    conductor.register_stage(normal);
    conductor.start<NormalStage>(0);

    EXPECT_TRUE(find_pps_event(queue, [](const PpsTargetEvent& e) {
        return e.pdo_index == -1;
    }));
}

TEST(NormalStagePublishing, EmitsRefreshedPpsTargetOnEncoderApply) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;

    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv(::testing::_)).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv(::testing::_)).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma(::testing::_)).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, set_pps_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestQueue queue;
    TestPublisher publisher(queue);
    normal.attach_publisher_INTERNAL_DO_NOT_USE(publisher);

    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(0);
    conductor.start<NormalStage>(0);

    // Drain entry events.
    while (pop_as<PpsTargetEvent>(queue) != nullptr) {}

    normal.on_event(conductor, EncoderEvent{-1}, 0);

    // The encoder delta is sign-flipped inside PPSMode::on_encoder; encoder -1 with
    // voltage_idx = 0 yields target += VOLTAGE_INCREMENTS_MV[0] = +1000 mV.
    const int32_t expected = 5000 + static_cast<int32_t>(pocketpd::VOLTAGE_INCREMENTS_MV[0]);
    EXPECT_TRUE(find_pps_event(queue, [&](const PpsTargetEvent& e) {
        return e.pdo_index == 0 && e.target_mv == expected;
    }));
}

TEST(NormalStageCompState, CachesCompStateEventForViewModel) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(true));
    EXPECT_CALL(sink, pdo_min_voltage_mv).WillRepeatedly(Return(3300));
    EXPECT_CALL(sink, pdo_max_voltage_mv).WillRepeatedly(Return(11000));
    EXPECT_CALL(sink, pdo_max_current_ma).WillRepeatedly(Return(3000));
    EXPECT_CALL(sink, set_pps_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    normal.prepare(0);
    conductor.start<NormalStage>(0);

    normal.on_event(conductor, CompStateEvent{60}, 0);
    EXPECT_EQ(normal.comp_offset_mv(), 60);

    normal.on_event(conductor, CompStateEvent{0}, 0);
    EXPECT_EQ(normal.comp_offset_mv(), 0);
}

namespace {
    // Helper that builds a PPSMode pinned to a stub sink. PPSMode has no default ctor;
    // it requires a PdSinkController&. The clamp call inside its ctor consults the sink
    // for min/max voltage and max current — stub those to wide-open values.
    PPSMode make_pps_mode(MockPdSink& sink, int32_t target_mv, int32_t target_ma) {
        ON_CALL(sink, pdo_min_voltage_mv(_)).WillByDefault(Return(3300));
        ON_CALL(sink, pdo_max_voltage_mv(_)).WillByDefault(Return(11000));
        ON_CALL(sink, pdo_max_current_ma(_)).WillByDefault(Return(3000));
        PPSMode mode{sink, 0};
        mode.target_mv = target_mv;
        mode.target_ma = target_ma;
        return mode;
    }
}

TEST(PpsViewRender, OffsetSuffixHiddenWhenZero) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(display, draw_text(_, _, ::testing::HasSubstr("+"))).Times(0);
    EXPECT_CALL(display, draw_text(_, _, _)).Times(::testing::AnyNumber());
    EXPECT_CALL(display, text_width(_)).WillRepeatedly(Return(20));
    EXPECT_CALL(display, set_font).Times(::testing::AnyNumber());
    EXPECT_CALL(display, clear).Times(1);
    EXPECT_CALL(display, draw_box).Times(::testing::AnyNumber());

    NormalViewModel vm{};
    vm.mode = make_pps_mode(sink, 5000, 1000);
    vm.active_pdo_index = 0;
    vm.comp_offset_mv = 0;
    vm.output_enabled = true;
    vm.readout_visible = true;

    NormalView::render(display, vm);
}

TEST(PpsViewRender, OffsetSuffixRenderedWhenNonZero) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(display, draw_text(_, _, _)).Times(::testing::AnyNumber());
    EXPECT_CALL(display, draw_text(_, _, StrEq("+60"))).Times(1);
    EXPECT_CALL(display, text_width(_)).WillRepeatedly(Return(20));
    EXPECT_CALL(display, set_font).Times(::testing::AnyNumber());
    EXPECT_CALL(display, clear).Times(1);
    EXPECT_CALL(display, draw_box).Times(::testing::AnyNumber());

    NormalViewModel vm{};
    vm.mode = make_pps_mode(sink, 5000, 1000);
    vm.active_pdo_index = 0;
    vm.comp_offset_mv = 60;
    vm.output_enabled = true;
    vm.readout_visible = true;

    NormalView::render(display, vm);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
