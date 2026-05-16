#ifdef UNIT_TEST
#include "test_mock.h"
#endif
#include "engine_test.h"
#include "grain_test.h"
#include "mapper_test.h"
#include "math_test.h"
#include "mod_test.h"
#include "quantizer_test.h"
#include "router_test.h"
#include "scheduler_test.h"
#include "tone_test.h"
#include "ui_test.h"
#include "window_test.h"

int main() {
    test_engine_immediate_updates();
    test_engine_parameter_smoothing();
    test_engine_tone_mapping();

    test_grain_lifecycle();
    test_grain_manager_polyphony();
    test_grain_manager_randomization();
    test_grain_manager_reset();
    test_grain_manager_safety_clamping();
    test_grain_reset();

    test_mapper_density();
    test_mapper_effective_knob();
    test_mapper_expression_assignment();
    test_mapper_initialization();
    test_mapper_switches();

    test_math_fonepole_deadband();

    test_mod_depth_more();
    test_mod_depth_none();
    test_mod_depth_some();

    test_quantizer_free_mode();
    test_quantizer_octaves_fifths_mode();
    test_quantizer_semitones_mode();
    test_quantizer_zero_pitch();

    test_router_drive_saturation();
    test_router_freeze();
    test_router_mixer();
    test_router_reset();
    test_router_routing_logic();

    test_scheduler_burst_mode();
    test_scheduler_free_mode();
    test_scheduler_reset();
    test_scheduler_synced_mode();

    test_tone_filter_stateful();
    test_tone_mapping();

    test_ui_manager_priorities();
    test_ui_manager_tempo_color();
    test_ui_manager_timers();

    test_window_interpolation();
    test_window_tables();

    std::cout << "All Tests Passed Successfully." << std::endl;
    return 0;
}
