#ifdef UNIT_TEST
#include "test_mock.h"
#endif
#include "dsp/engine_test.h"
#include "dsp/grain_test.h"
#include "dsp/math_test.h"
#include "dsp/modulator_test.h"
#include "dsp/quantizer_test.h"
#include "dsp/router_test.h"
#include "dsp/scheduler_test.h"
#include "dsp/tone_filter_test.h"
#include "dsp/window_test.h"

int main() {
    test_engine_end_to_end_signal_flow();
    test_engine_reset();

    test_grain_lifecycle();
    test_grain_manager_polyphony();
    test_grain_manager_randomization();
    test_grain_manager_reset();
    test_grain_reset();
    test_clamp_offset();
    test_grain_param_helpers();
    test_grain_manager_trigger_smoke();

    test_math_fonepole_deadband();

    test_modulator_depth_more();
    test_modulator_depth_none();
    test_modulator_depth_some();

    test_quantizer_free_mode();
    test_quantizer_octaves_fifths_mode();
    test_quantizer_semitones_mode();
    test_quantizer_zero_pitch();
    test_quantizer_values();

    test_router_drive_saturation();
    test_router_freeze();
    test_router_mixer();
    test_router_reset();
    test_router_routing_logic();

    test_scheduler_burst_mode();
    test_scheduler_free_mode();
    test_scheduler_reset();
    test_scheduler_synced_mode();
    test_scheduler_modulation();

    test_tone_filter_stateful();
    test_tone_filter_stereo_independence();

    test_window_interpolation();
    test_window_tables();

    std::cout << "All Tests Passed Successfully." << std::endl;
    return 0;
}
