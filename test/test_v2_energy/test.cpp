/**
 * GoogleTest suite for EnergyTask + EnergyStage.
 *
 * EnergyTask: SensorEvent ingest, output-on/off transitions, Wh / Ah /
 * total-seconds bookkeeping, EnergyEvent publication on tick.
 * EnergyStage: entry from NormalStage on R LONG, render driven by EnergyEvent,
 * R SHORT toggles output, R LONG returns to NormalStage with original profile,
 * encoder + L gestures ignored.
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
#include "v2/images.h"
#include "v2/stages/energy/energy_view.h"
#include "v2/stages/energy_stage.h"
#include "v2/stages/normal_stage.h"
#include "v2/tasks/energy_task.h"

using namespace pocketpd;
using ::testing::NiceMock;
using ::testing::Return;

using TestQueue = tempo::EventQueue<Event, 16>;
using TestPublisher = tempo::QueuePublisher<Event, 16>;
using TestConductor = App::Conductor;

namespace {

    SensorEvent make_sensor(uint32_t ts_ms, uint32_t mv, uint32_t ma) {
        return SensorEvent{LoadReading{ts_ms, mv, ma}, SupplyReading{}};
    }

    const EnergyEvent* drain_last_energy(TestQueue& q) {
        static Event last;
        const EnergyEvent* found = nullptr;
        Event e;
        while (q.pop(e)) {
            if (std::get_if<EnergyEvent>(&e) != nullptr) {
                last = e;
                found = std::get_if<EnergyEvent>(&last);
            }
        }
        return found;
    }

} // namespace

// ——— EnergyTask ——————————————————————————————————————————————————————

TEST(EnergyTask, OutputOffYieldsNoAccumulation) {
    FakeOutputGate gate;
    EnergyTask task(gate);
    task.on_event(make_sensor(0, 5000, 1000), 0);
    task.on_event(make_sensor(100, 5000, 1000), 100);
    EXPECT_EQ(task.accumulated_wh(), 0.0);
    EXPECT_EQ(task.accumulated_ah(), 0.0);
    EXPECT_EQ(task.total_seconds(), 0u);
}

TEST(EnergyTask, FirstSampleArmsSessionWithoutAccumulating) {
    FakeOutputGate gate;
    gate.enable();
    EnergyTask task(gate);
    task.on_event(make_sensor(1000, 5000, 1000), 1000);
    EXPECT_EQ(task.accumulated_wh(), 0.0);
    EXPECT_EQ(task.accumulated_ah(), 0.0);
}

TEST(EnergyTask, SubsequentSampleAccumulatesWhAndAh) {
    FakeOutputGate gate;
    gate.enable();
    EnergyTask task(gate);
    task.on_event(make_sensor(0, 5000, 1000), 0);
    task.on_event(make_sensor(100, 5000, 1000), 100);

    const double expected_wh = 5.0 * 100.0 / 3600000.0;
    const double expected_ah = 1.0 * 100.0 / 3600000.0;
    EXPECT_NEAR(task.accumulated_wh(), expected_wh, 1e-9);
    EXPECT_NEAR(task.accumulated_ah(), expected_ah, 1e-9);
}

TEST(EnergyTask, LargeDeltaSkippedToAvoidStartupAndJumps) {
    FakeOutputGate gate;
    gate.enable();
    EnergyTask task(gate);
    task.on_event(make_sensor(0, 5000, 1000), 0);
    task.on_event(make_sensor(2000, 5000, 1000), 2000);
    EXPECT_EQ(task.accumulated_wh(), 0.0);
    EXPECT_EQ(task.accumulated_ah(), 0.0);
}

TEST(EnergyTask, ClosedSessionRetainsAccumulatedSeconds) {
    FakeOutputGate gate;
    EnergyTask task(gate);

    gate.enable();
    // 7 events 500 ms apart → 6 deltas of 500 ms = 3000 ms accumulated.
    for (uint32_t t = 1000; t <= 4000; t += 500) {
        task.on_event(make_sensor(t, 5000, 1000), t);
    }

    gate.disable();
    task.on_event(make_sensor(4500, 5000, 1000), 4500);

    EXPECT_EQ(task.total_seconds(), 3u);
}

TEST(EnergyTask, RestartedSessionAccumulatesAcrossClosure) {
    FakeOutputGate gate;
    EnergyTask task(gate);

    // First session: 6 deltas of 500 ms = 3000 ms.
    gate.enable();
    for (uint32_t t = 0; t <= 3000; t += 500) {
        task.on_event(make_sensor(t, 5000, 1000), t);
    }

    gate.disable();
    task.on_event(make_sensor(3500, 5000, 1000), 3500);

    // Second session: 4 deltas of 500 ms = 2000 ms. Joined with prior 3000 ms.
    gate.enable();
    for (uint32_t t = 10000; t <= 12000; t += 500) {
        task.on_event(make_sensor(t, 5000, 1000), t);
    }

    EXPECT_EQ(task.total_seconds(), 5u);
}

TEST(EnergyTask, OnTickPublishesEnergyEvent) {
    FakeOutputGate gate;
    gate.enable();
    EnergyTask task(gate);
    TestQueue q;
    TestPublisher pub(q);
    task.attach_publisher_INTERNAL_DO_NOT_USE(pub);

    task.on_event(make_sensor(0, 5000, 1000), 0);
    task.on_event(make_sensor(100, 5000, 1000), 100);
    task.on_tick(2500);

    const auto* evt = drain_last_energy(q);
    ASSERT_NE(evt, nullptr);
    const double expected_wh = 5.0 * 100.0 / 3600000.0;
    EXPECT_NEAR(evt->accumulated_wh, expected_wh, 1e-9);
    EXPECT_EQ(evt->total_seconds, 0u);
}

// ——— NormalStage → EnergyStage transition ————————————————————————————————

TEST(NormalStageEnergy, RLongRequestsEnergyStageWithSamePdoIndex) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    EnergyStage energy(display, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    conductor.register_stage(energy);

    normal.prepare(2);
    conductor.start<NormalStage>(0);
    normal.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::LONG}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition(0));
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<EnergyStage>());
    EXPECT_EQ(energy.active_pdo_index(), 2);
}

// ——— EnergyStage ————————————————————————————————————————————————————————

namespace {

    struct EnergyStageHarness {
        NiceMock<MockDisplay> display;
        FakeOutputGate gate;
        EnergyStage energy{display, gate};
        TestConductor conductor;

        EnergyStageHarness() {
            conductor.register_stage(energy);
            energy.prepare(1);
            conductor.start<EnergyStage>(0);
        }
    };

} // namespace

TEST(EnergyStage, RShortTogglesOutput) {
    EnergyStageHarness h;
    EXPECT_FALSE(h.gate.is_enabled());

    h.energy.on_event(h.conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);
    EXPECT_TRUE(h.gate.is_enabled());

    h.energy.on_event(h.conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);
    EXPECT_FALSE(h.gate.is_enabled());
}

TEST(EnergyStage, RLongReturnsToNormalStageWithStoredProfile) {
    NiceMock<MockDisplay> display;
    NiceMock<MockPdSink> sink;
    FakeOutputGate gate;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    NormalStage normal(display, sink, gate);
    EnergyStage energy(display, gate);
    TestConductor conductor;
    conductor.register_stage(normal);
    conductor.register_stage(energy);

    energy.prepare(3);
    conductor.start<EnergyStage>(0);
    energy.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::LONG}, 0);

    EXPECT_TRUE(conductor.has_pending());
    EXPECT_TRUE(conductor.apply_pending_transition(0));
    EXPECT_EQ(conductor.current_index(), TestConductor::index_of<NormalStage>());
    EXPECT_EQ(normal.active_pdo_index(), 3);
}

TEST(EnergyStage, EncoderAndLGesturesAreIgnored) {
    EnergyStageHarness h;

    h.energy.on_event(h.conductor, EncoderEvent{5}, 0);
    EXPECT_FALSE(h.conductor.has_pending());

    h.energy.on_event(h.conductor, ButtonEvent{ButtonId::L, Gesture::LONG}, 0);
    EXPECT_FALSE(h.conductor.has_pending());

    h.energy.on_event(h.conductor, ButtonEvent{ButtonId::ENCODER, Gesture::SHORT}, 0);
    EXPECT_FALSE(h.conductor.has_pending());
}

TEST(EnergyStage, EnergyEventDriveDisplayedAccumulators) {
    using ::testing::AtLeast;
    using ::testing::HasSubstr;
    using ::testing::StrEq;

    NiceMock<MockDisplay> display;
    FakeOutputGate gate;

    EnergyStage energy(display, gate);
    TestConductor conductor;
    conductor.register_stage(energy);

    energy.prepare(0);
    conductor.start<EnergyStage>(0);

    EnergyEvent ee{};
    ee.accumulated_wh = 1.23;
    ee.accumulated_ah = 0.45;
    ee.total_seconds = 65;
    energy.on_event(conductor, ee, 0);

    // First on_tick arms the IntervalTimer; second fires the render.
    energy.on_tick(conductor, 0);

    EXPECT_CALL(display, draw_text(::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber());
    EXPECT_CALL(display, draw_text(::testing::_, ::testing::_, StrEq("Wh"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(::testing::_, ::testing::_, StrEq("Ah"))).Times(AtLeast(1));
    EXPECT_CALL(display, draw_text(::testing::_, ::testing::_, HasSubstr("01:05")))
        .Times(AtLeast(1));
    EXPECT_CALL(display, flush()).Times(AtLeast(1));

    energy.on_tick(conductor, 100);
}

// ——— EnergyStage lock ————————————————————————————————————————————————————————

TEST(EnergyStage, ComboLongTogglesLock) {
    NiceMock<MockDisplay> display;
    NiceMock<MockOutputGate> gate;

    EnergyStage stage(display, gate);
    TestConductor conductor;
    conductor.register_stage(stage);
    stage.prepare(0);
    conductor.start<EnergyStage>(0);

    EXPECT_FALSE(stage.locked());
    stage.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    EXPECT_TRUE(stage.locked());
    stage.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    EXPECT_FALSE(stage.locked());
}

TEST(EnergyStage, LockedIgnoresRShort) {
    NiceMock<MockDisplay> display;
    NiceMock<MockOutputGate> gate;
    EXPECT_CALL(gate, disable()).Times(::testing::AnyNumber());
    EXPECT_CALL(gate, enable()).Times(0);

    EnergyStage stage(display, gate);
    TestConductor conductor;
    conductor.register_stage(stage);
    stage.prepare(0);
    conductor.start<EnergyStage>(0);

    stage.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    ASSERT_TRUE(stage.locked());

    stage.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::SHORT}, 0);
}

TEST(EnergyStage, OnEnterResetsLocked) {
    NiceMock<MockDisplay> display;
    NiceMock<MockOutputGate> gate;

    EnergyStage stage(display, gate);
    TestConductor conductor;
    conductor.register_stage(stage);
    stage.prepare(0);
    conductor.start<EnergyStage>(0);

    stage.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    ASSERT_TRUE(stage.locked());

    stage.prepare(0);
    stage.on_enter(conductor, 0);
    EXPECT_FALSE(stage.locked());
}

// ——— EnergyView render ——————————————————————————————————————————————————————

TEST(EnergyView, LockedRendersPadlock) {
    using ::testing::_;
    NiceMock<MockDisplay> display;

    EXPECT_CALL(
        display,
        draw_xbm(
            EnergyView::PADLOCK_X, EnergyView::PADLOCK_Y,
            EnergyView::PADLOCK_W, EnergyView::PADLOCK_H,
            bitmap::PADLOCK.data()
        )
    ).Times(1);

    EnergyViewModel vm{};
    vm.output_enabled = false;
    vm.locked = true;

    EnergyView::render(display, vm);
}

TEST(EnergyView, UnlockedDoesNotDrawPadlock) {
    using ::testing::_;
    NiceMock<MockDisplay> display;

    EXPECT_CALL(
        display,
        draw_xbm(_, _, EnergyView::PADLOCK_W, EnergyView::PADLOCK_H, bitmap::PADLOCK.data())
    ).Times(0);

    EnergyViewModel vm{};
    vm.output_enabled = false;
    vm.locked = false;

    EnergyView::render(display, vm);
}

TEST(EnergyStage, LockedIgnoresRLong) {
    NiceMock<MockDisplay> display;
    NiceMock<MockOutputGate> gate;
    NiceMock<MockPdSink> sink;
    EXPECT_CALL(sink, is_index_pps(::testing::_)).WillRepeatedly(Return(false));
    EXPECT_CALL(sink, set_pdo).WillRepeatedly(Return(true));

    EnergyStage energy(display, gate);
    NormalStage normal(display, sink, gate);
    TestConductor conductor;
    conductor.register_stage(energy);
    conductor.register_stage(normal);
    energy.prepare(0);
    conductor.start<EnergyStage>(0);

    energy.on_event(conductor, ButtonEvent{ButtonId::L_R, Gesture::LONG}, 0);
    ASSERT_TRUE(energy.locked());

    energy.on_event(conductor, ButtonEvent{ButtonId::R, Gesture::LONG}, 0);
    EXPECT_TRUE(energy.locked());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
