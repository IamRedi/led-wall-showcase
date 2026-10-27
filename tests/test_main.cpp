#include "test_framework.h"

// LED-L002 host test entry point (deterministic, no external framework).

void test_matrix_mapping_expected_indices();
void test_matrix_mapping_bijection();
void test_matrix_mapping_no_flip();
void test_matrix_mapping_flip_y();
void test_matrix_mapping_out_of_bounds();

void test_mode_machine_boot_and_persistent();
void test_mode_machine_clear();
void test_mode_machine_name_truncation();
void test_mode_machine_invalid_kind_and_name();

void test_transient_basic_lifecycle();
void test_transient_expiry_boundary();
void test_transient_replace_no_queue();
void test_transient_persistent_cancels();
void test_transient_zero_duration_rejected();

void test_transient_creation_near_uint32_max();
void test_transient_tick_before_rollover();
void test_transient_tick_after_rollover();
void test_transient_expiry_exactly_at_wrapped_deadline();
void test_transient_snapshot_purity_across_rollover();
void test_transient_replacement_across_rollover();
void test_transient_invalid_durations_unchanged_semantics();

void test_throttle_not_dirty_never_frames();
void test_throttle_initial_dirty_before_floor();
void test_throttle_exactly_at_floor();
void test_throttle_after_show_exact_and_over_floor();
void test_throttle_wrap_around();
void test_throttle_did_show_clears_dirty();

void test_public_state_remaining_independent_of_boot_offset();
void test_public_state_remaining_independent_of_absolute_wall();
void test_public_state_after_expiry();
void test_public_state_rollover();
void test_public_state_frozen_spec_fields();

void test_brightness_defaults_and_clamp();
void test_brightness_ceiling_changes();

void test_config_defaults();
void test_config_valid_entries();
void test_config_unknown_key_warning();
void test_config_invalid_values_fallback();
void test_config_wifi_and_api_keys();

void test_registry_expression_vocabulary();
void test_registry_animation_keys();
void test_registry_scene_usable_animations();
void test_registry_available_list();
void test_registry_games_and_inputs();

void test_dispatcher_persistent_programs();
void test_dispatcher_unknown_names_typed_errors_no_mutation();
void test_dispatcher_text_show_persistent_and_transient();
void test_dispatcher_clear_semantics();
void test_dispatcher_clear2_leaves_unrelated_transient();
void test_dispatcher_brightness_semantics();
void test_dispatcher_events_and_rollover();
void test_dispatcher_state_read_and_max_duration();

void test_identity_payload_fields();
void test_identity_core_capabilities_always_advertised();
void test_identity_conditional_spectrum_gate();

void test_public_state_text_reporting_by_mode();

void test_ui_route_matching();
void test_ui_content_bounds_and_type();
void test_ui_content_ascii_only();
void test_ui_content_endpoints();
void test_ui_content_event_names();
void test_ui_content_vocabularies();
void test_ui_content_error_vocabulary();
void test_ui_content_contract_markers();
void test_ui_content_forbidden_constructs();
void test_ui_v3_progressive_disclosure_structure();
void test_ui_v3_wire_vocabulary();

// LED-L007 — visual renderers + text pipeline.
void test_font_heritage_glyphs_byte_identical();
void test_font_frozen_set_fully_supported();
void test_font_unsupported_characters();
void test_font_unsupported_renders_as_space();
void test_font_extended_charset_supported();
void test_font_utf8_decode();
void test_font_d_extension_glyph_corrected();
void test_font_geometry();

void test_text_width_and_skipped();
void test_text_static_centered_golden();
void test_text_static_exactly_fitting();
void test_text_static_one_px_too_wide_is_rejected_upstream();
void test_text_scroll_initial_and_first_step_golden();
void test_text_scroll_full_traversal_wraps();
void test_text_scroll_rollover_timing();

void test_corners_golden();
void test_checker_golden();
void test_border_golden();
void test_row_sweep_golden();
void test_bounce_golden();
void test_wave_golden();
void test_rain_golden();
void test_plasma_golden();
void test_animations_registry_binding_and_bounds();

void test_expression_table_covers_frozen_vocabulary_exactly();
void test_expression_frozen_examples_mapping();

void test_renderer_idle_is_blank();
void test_renderer_scene_and_animation_identical();
void test_renderer_unknown_names_fail_closed();
void test_renderer_conditional_kinds_fail_closed();
void test_renderer_text_dispatch();
void test_renderer_expression_dispatch_and_restore();
void test_renderer_time_varying_classification();
void test_text_program_completion_semantics();

void test_text_too_wide_static_rejected_no_mutation();
void test_text_width_boundary_static();
void test_text_skipped_characters_reported();
void test_program_start_stamps_feed_renderer();
void test_text_program_stamps_independent_per_slot();
void test_dispatcher_never_emits_text_done();
void test_framebuffer_bounds_and_mapping();

void test_adversarial_scroll_speed_zero_guard();
void test_adversarial_multibyte_skipped_accounting();
void test_adversarial_animation_time_wrap();
void test_adversarial_buffer_reuse_leaves_no_residue();
void test_adversarial_all_orientation_combinations();
void test_adversarial_expression_unknown_restores_nothing();

// LED-L009 — experience reconstruction.
void test_countdown_arm_and_idle_view();
void test_countdown_run_pause_resume_exact_ms();
void test_countdown_zero_finishes_stable();
void test_countdown_reset_and_stop_distinction();
void test_countdown_rollover_wrap_safety();
void test_countdown_arm_validation_and_bounds();
void test_countdown_display_formats();
void test_countdown_state_names();
void test_input_event_wire_roundtrip();
void test_input_queue_bounded_fifo_and_drop_count();
void test_button_decoder_debounce_and_edge();
void test_button_decoder_repeat_schedule();
void test_button_decoder_start_state();
void test_input_provider_independence();
void test_menu_open_close_and_root_navigation();
void test_menu_timer_and_standby_and_exit_actions();
void test_menu_lists_select_and_back();
void test_menu_brightness_editing();
void test_menu_closed_ignores_input();
void test_games_registry_bindings_complete();
void test_game_host_lifecycle_all_games();
void test_game_pong_point_and_over();
void test_game_snake_growth_and_wall_over();
void test_game_reaction_stop_at_zero_precision();
void test_game_breakout_and_dodger_bounds();
void test_game_host_unknown_start_fails_closed();
void test_game_over_cue_fires_once_per_run();

void test_game_difficulty_host_default_and_persistence();
void test_game_snake_difficulty_moves_at_documented_intervals();
void test_game_pong_difficulty_ball_speed();
void test_game_breakout_difficulty_paddle_and_speed();
void test_game_reaction_difficulty_window_keeps_ms_precision();
void test_game_dodger_difficulty_cadence_by_score();
void test_game_difficulty_change_applies_at_restart_not_live();
void test_game_difficulty_inputs_not_dropped();

void test_scroll_golden_full_frames_match_independent_reference();
void test_scroll_progression_moves_glyphs_in_rtl_direction();
void test_scroll_ltr_legacy_direction_preserved();
void test_scroll_no_block_for_dark_background_styles();
void test_scroll_loop_wraps_seamlessly();
void test_scroll_no_stale_pixels_across_swaps();
void test_scroll_extreme_offsets_stay_bounded();
void test_scroll_repeated_activation_survives();
void test_scroll_text_frame_content_changed_cadence_helper();

void test_font_u_w_letterforms_owner_corrected();
void test_api_game_difficulty_semantics();
void test_v5_clear_exits_running_game();
void test_api_game_difficulty_without_services_fails_closed();
void test_text_style_registry();
void test_text_palette_semantic_names();
void test_text_readable_foreground();
void test_text_codepoint_width_and_skip();
void test_text_lowercase_and_punctuation_supported();
void test_text_rtl_marquee_enters_from_right();
void test_text_rtl_full_traversal_and_wrap();
void test_text_word_by_word_sequence();
void test_text_loop_marquee_never_freezes();
void test_text_styles_render();
void test_text_studio_program_complete_semantics();
void test_visualizer_accepts_wellformed_feed();
void test_visualizer_rejects_malformed_and_out_of_rate();
void test_visualizer_expiry_and_decay();
void test_visualizer_clear();
void test_api_timer_semantics();
void test_api_input_menu_games();
void test_api_visualizer_notify_standby();
void test_api_text_studio_command_semantics();
void test_api_extended_without_services_fails_closed();

int g_checks = 0;
int g_failures = 0;

// LED-L010 — matrix + tail + DS4 + P1/P2 expansion.
void test_zone_target_wire_roundtrip();
void test_l010_tail_setup_point_gate_and_off();
void test_zone_target_supported_mask();
void test_tail_framebuffer_bounds_and_config();
void test_tail_effects_deterministic_and_bounded();
void test_tail_scanner_moves_exactly_one_pixel();
void test_tail_cores_progress_and_chase();
void test_tail_pulse_core_extremes();
void test_tail_games_registry_and_labels();
void test_tail_game_host_lifecycle_all_six();
void test_light_race_2p_routing();
void test_tug_of_war_2p_pulls_opposite();
void test_reaction_stop_hit_and_miss();
void test_color_chain_insert_clears_and_loses();
void test_color_catch_life_loss_game_over();
void test_memory_wrong_press_ends_round();
void test_tail_game_difficulty_and_lane_injection();
void test_players_api_player_attribution();
void test_players_game_mode_validation();
void test_players_pong_2p_no_cross_talk();
void test_ds4_face_button_mapping_and_edges();
void test_ds4_dpad_directions_and_autorepeat();
void test_ds4_slots_lifecycle_and_state();
void test_ds4_disabled_core_is_inert();
void test_ds4_stick_share_and_player_ownership();
void test_wired_codec_roundtrip_and_rejection();
void test_l010_animation_target_defaults_and_validation();
void test_l010_target_carry_and_tail_enable_gate();
void test_l010_config_tail_keys();
void test_l010_public_state_zone_views();

// LED-L010 repair cycle 1 (2026-10-13): B-L010-R1 / M-L010-1..4,6 regressions.
void test_rc1_config_load_applies_all_documented_keys();
void test_rc1_config_load_key_list_complete();
void test_rc1_config_load_absent_keys_keep_defaults();
void test_rc1_config_load_corrupt_values_fail_safe();
void test_rc1_stale_tail_cleared_for_matrix_games();
void test_rc1_every_mode_renders_deterministically_from_prefilled_tail();
void test_rc1_tug_of_war_lane_centered_start_and_fair_play();
void test_rc1_reaction_stop_zone_winnable_at_every_lane();
void test_rc1_tail_game_min_lane_metadata();
void test_rc1_game_set_rejects_short_lane_typed();
void test_rc1_identity_worst_case_capacity();
void test_rc1_reported_zone_target_any_slot();
void test_v41_fireworks_crosses_anchor_on_one_clock();
void test_v41_fireworks_is_deterministic_and_bounded();
void test_v5_body_topology_crosses_configured_attachment();
void test_v41_output_limiter_combines_both_zones();
void test_v41_output_limiter_preserves_dark_and_zero_brightness();
void test_v41_all_ten_effects_have_coordinated_real_frames();
void test_v41_rainbow_anchor_color_continuity();
void test_v41_pixel_canvas_atomic_session_revision_and_tail_end();
void test_v41_pixel_canvas_renderer_uses_device_snapshot();
void test_v41_games_bind_and_terminate();
void test_v41_scene_bounded_timeline_and_presets();
void test_v41_input_sessions_two_players_edges_and_replay();
void test_v41_sound_scheduler_priority_mute_and_bounds();

int run_all_tests() {
  test_matrix_mapping_expected_indices();
  test_matrix_mapping_bijection();
  test_matrix_mapping_no_flip();
  test_matrix_mapping_flip_y();
  test_matrix_mapping_out_of_bounds();

  test_mode_machine_boot_and_persistent();
  test_mode_machine_clear();
  test_mode_machine_name_truncation();
  test_mode_machine_invalid_kind_and_name();

  test_transient_basic_lifecycle();
  test_transient_expiry_boundary();
  test_transient_replace_no_queue();
  test_transient_persistent_cancels();
  test_transient_zero_duration_rejected();

  test_transient_creation_near_uint32_max();
  test_transient_tick_before_rollover();
  test_transient_tick_after_rollover();
  test_transient_expiry_exactly_at_wrapped_deadline();
  test_transient_snapshot_purity_across_rollover();
  test_transient_replacement_across_rollover();
  test_transient_invalid_durations_unchanged_semantics();

  test_throttle_not_dirty_never_frames();
  test_throttle_initial_dirty_before_floor();
  test_throttle_exactly_at_floor();
  test_throttle_after_show_exact_and_over_floor();
  test_throttle_wrap_around();
  test_throttle_did_show_clears_dirty();

  test_public_state_remaining_independent_of_boot_offset();
  test_public_state_remaining_independent_of_absolute_wall();
  test_public_state_after_expiry();
  test_public_state_rollover();
  test_public_state_frozen_spec_fields();

  test_brightness_defaults_and_clamp();
  test_brightness_ceiling_changes();

  test_config_defaults();
  test_config_valid_entries();
  test_config_unknown_key_warning();
  test_config_invalid_values_fallback();
  test_config_wifi_and_api_keys();

  test_registry_expression_vocabulary();
  test_registry_animation_keys();
  test_registry_scene_usable_animations();
  test_registry_available_list();
  test_registry_games_and_inputs();

  test_dispatcher_persistent_programs();
  test_dispatcher_unknown_names_typed_errors_no_mutation();
  test_dispatcher_text_show_persistent_and_transient();
  test_dispatcher_clear_semantics();
  test_dispatcher_clear2_leaves_unrelated_transient();
  test_dispatcher_brightness_semantics();
  test_dispatcher_events_and_rollover();
  test_dispatcher_state_read_and_max_duration();

  test_identity_payload_fields();
  test_identity_core_capabilities_always_advertised();
  test_identity_conditional_spectrum_gate();

  test_public_state_text_reporting_by_mode();

  test_ui_route_matching();
  test_ui_content_bounds_and_type();
  test_ui_content_ascii_only();
  test_ui_content_endpoints();
  test_ui_content_event_names();
  test_ui_content_vocabularies();
  test_ui_content_error_vocabulary();
  test_ui_content_contract_markers();
  test_ui_content_forbidden_constructs();
  test_ui_v3_progressive_disclosure_structure();
  test_ui_v3_wire_vocabulary();

  test_font_heritage_glyphs_byte_identical();
  test_font_frozen_set_fully_supported();
  test_font_unsupported_characters();
  test_font_unsupported_renders_as_space();
  test_font_extended_charset_supported();
  test_font_utf8_decode();
  test_font_d_extension_glyph_corrected();
  test_font_geometry();

  test_text_width_and_skipped();
  test_text_static_centered_golden();
  test_text_static_exactly_fitting();
  test_text_static_one_px_too_wide_is_rejected_upstream();
  test_text_scroll_initial_and_first_step_golden();
  test_text_scroll_full_traversal_wraps();
  test_text_scroll_rollover_timing();

  test_corners_golden();
  test_checker_golden();
  test_border_golden();
  test_row_sweep_golden();
  test_bounce_golden();
  test_wave_golden();
  test_rain_golden();
  test_plasma_golden();
  test_animations_registry_binding_and_bounds();

  test_expression_table_covers_frozen_vocabulary_exactly();
  test_expression_frozen_examples_mapping();

  test_renderer_idle_is_blank();
  test_renderer_scene_and_animation_identical();
  test_renderer_unknown_names_fail_closed();
  test_renderer_conditional_kinds_fail_closed();
  test_renderer_text_dispatch();
  test_renderer_expression_dispatch_and_restore();
  test_renderer_time_varying_classification();
  test_text_program_completion_semantics();

  test_text_too_wide_static_rejected_no_mutation();
  test_text_width_boundary_static();
  test_text_skipped_characters_reported();
  test_program_start_stamps_feed_renderer();
  test_text_program_stamps_independent_per_slot();
  test_dispatcher_never_emits_text_done();
  test_framebuffer_bounds_and_mapping();

  test_adversarial_scroll_speed_zero_guard();
  test_adversarial_multibyte_skipped_accounting();
  test_adversarial_animation_time_wrap();
  test_adversarial_buffer_reuse_leaves_no_residue();
  test_adversarial_all_orientation_combinations();
  test_adversarial_expression_unknown_restores_nothing();

  // LED-L009 — experience reconstruction.
  test_countdown_arm_and_idle_view();
  test_countdown_run_pause_resume_exact_ms();
  test_countdown_zero_finishes_stable();
  test_countdown_reset_and_stop_distinction();
  test_countdown_rollover_wrap_safety();
  test_countdown_arm_validation_and_bounds();
  test_countdown_display_formats();
  test_countdown_state_names();
  test_input_event_wire_roundtrip();
  test_input_queue_bounded_fifo_and_drop_count();
  test_button_decoder_debounce_and_edge();
  test_button_decoder_repeat_schedule();
  test_button_decoder_start_state();
  test_input_provider_independence();
  test_menu_open_close_and_root_navigation();
  test_menu_timer_and_standby_and_exit_actions();
  test_menu_lists_select_and_back();
  test_menu_brightness_editing();
  test_menu_closed_ignores_input();
  test_games_registry_bindings_complete();
  test_game_host_lifecycle_all_games();
  test_game_pong_point_and_over();
  test_game_snake_growth_and_wall_over();
  test_game_reaction_stop_at_zero_precision();
  test_game_breakout_and_dodger_bounds();
  test_game_host_unknown_start_fails_closed();
  test_game_over_cue_fires_once_per_run();

  test_game_difficulty_host_default_and_persistence();
  test_game_snake_difficulty_moves_at_documented_intervals();
  test_game_pong_difficulty_ball_speed();
  test_game_breakout_difficulty_paddle_and_speed();
  test_game_reaction_difficulty_window_keeps_ms_precision();
  test_game_dodger_difficulty_cadence_by_score();
  test_game_difficulty_change_applies_at_restart_not_live();
  test_game_difficulty_inputs_not_dropped();

  test_scroll_golden_full_frames_match_independent_reference();
  test_scroll_progression_moves_glyphs_in_rtl_direction();
  test_scroll_ltr_legacy_direction_preserved();
  test_scroll_no_block_for_dark_background_styles();
  test_scroll_loop_wraps_seamlessly();
  test_scroll_no_stale_pixels_across_swaps();
  test_scroll_extreme_offsets_stay_bounded();
  test_scroll_repeated_activation_survives();
  test_scroll_text_frame_content_changed_cadence_helper();

  test_font_u_w_letterforms_owner_corrected();
  test_api_game_difficulty_semantics();
  test_v5_clear_exits_running_game();
  test_api_game_difficulty_without_services_fails_closed();
  test_text_style_registry();
  test_text_palette_semantic_names();
  test_text_readable_foreground();
  test_text_codepoint_width_and_skip();
  test_text_lowercase_and_punctuation_supported();
  test_text_rtl_marquee_enters_from_right();
  test_text_rtl_full_traversal_and_wrap();
  test_text_word_by_word_sequence();
  test_text_loop_marquee_never_freezes();
  test_text_styles_render();
  test_text_studio_program_complete_semantics();
  test_visualizer_accepts_wellformed_feed();
  test_visualizer_rejects_malformed_and_out_of_rate();
  test_visualizer_expiry_and_decay();
  test_visualizer_clear();
  test_api_timer_semantics();
  test_api_input_menu_games();
  test_api_visualizer_notify_standby();
  test_api_text_studio_command_semantics();
  test_api_extended_without_services_fails_closed();

  // LED-L010 — matrix + tail + DS4 + P1/P2 expansion.
  test_zone_target_wire_roundtrip();
  test_l010_tail_setup_point_gate_and_off();
  test_zone_target_supported_mask();
  test_tail_framebuffer_bounds_and_config();
  test_tail_effects_deterministic_and_bounded();
  test_tail_scanner_moves_exactly_one_pixel();
  test_tail_cores_progress_and_chase();
  test_tail_pulse_core_extremes();
  test_tail_games_registry_and_labels();
  test_tail_game_host_lifecycle_all_six();
  test_light_race_2p_routing();
  test_tug_of_war_2p_pulls_opposite();
  test_reaction_stop_hit_and_miss();
  test_color_chain_insert_clears_and_loses();
  test_color_catch_life_loss_game_over();
  test_memory_wrong_press_ends_round();
  test_tail_game_difficulty_and_lane_injection();
  test_players_api_player_attribution();
  test_players_game_mode_validation();
  test_players_pong_2p_no_cross_talk();
  test_ds4_face_button_mapping_and_edges();
  test_ds4_dpad_directions_and_autorepeat();
  test_ds4_slots_lifecycle_and_state();
  test_ds4_disabled_core_is_inert();
  test_ds4_stick_share_and_player_ownership();
  test_wired_codec_roundtrip_and_rejection();
  test_l010_animation_target_defaults_and_validation();
  test_l010_target_carry_and_tail_enable_gate();
  test_l010_config_tail_keys();
  test_l010_public_state_zone_views();
  test_l010_public_state_zone_views();

  // LED-L010 repair cycle 1 regression suite.
  test_rc1_config_load_applies_all_documented_keys();
  test_rc1_config_load_key_list_complete();
  test_rc1_config_load_absent_keys_keep_defaults();
  test_rc1_config_load_corrupt_values_fail_safe();
  test_rc1_stale_tail_cleared_for_matrix_games();
  test_rc1_every_mode_renders_deterministically_from_prefilled_tail();
  test_rc1_tug_of_war_lane_centered_start_and_fair_play();
  test_rc1_reaction_stop_zone_winnable_at_every_lane();
  test_rc1_tail_game_min_lane_metadata();
  test_rc1_game_set_rejects_short_lane_typed();
  test_rc1_identity_worst_case_capacity();
  test_rc1_reported_zone_target_any_slot();
  test_v41_fireworks_crosses_anchor_on_one_clock();
  test_v41_fireworks_is_deterministic_and_bounded();
  test_v5_body_topology_crosses_configured_attachment();
  test_v41_output_limiter_combines_both_zones();
  test_v41_output_limiter_preserves_dark_and_zero_brightness();
  test_v41_all_ten_effects_have_coordinated_real_frames();
  test_v41_rainbow_anchor_color_continuity();
  test_v41_pixel_canvas_atomic_session_revision_and_tail_end();
  test_v41_pixel_canvas_renderer_uses_device_snapshot();
  test_v41_games_bind_and_terminate();
  test_v41_scene_bounded_timeline_and_presets();
  test_v41_input_sessions_two_players_edges_and_replay();
  test_v41_sound_scheduler_priority_mute_and_bounds();

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return (g_failures == 0) ? 0 : 1;
}

int main() { return run_all_tests(); }
