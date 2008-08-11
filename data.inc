
; Declarations for DATA.ASM- global data

; These first three are from IDPUT.ASM
global C id_chars:byte
global C id_dmg:byte
global C def_id_chars:byte
global C player_color:byte
global C player_char:byte         ;Array 4 in size
global C bullet_color:byte        ;Array 3 in size
global C missile_color:byte
global C bullet_char:byte         ;Array 12 in size

global C board_where:byte         ;Array NUM_BOARDS in size
global C keys:byte                ;Array NUM_KEYS in size
global C score:dword
global C mod_playing:byte         ;Array FILENAME_SIZE in size
global C curr_board:byte
global C curr_file:byte           ;Array FILENAME_SIZE in size
global C curr_sav:byte				 ;Array FILENAME_SIZE in size
global C help_file:byte           ;Array PATHNAME_SIZE in size
global C MSE_file:byte  		    ;Array PATHNAME_SIZE in size
global C mzx_blank_mod_file:byte  ;Array PATHNAME_SIZE in size
global C mzx_convert_mod_file:byte;Array PATHNAME_SIZE in size
global C config_file:byte         ;Array PATHNAME_SIZE in size
global C megazeux_dir:byte        ;Array PATHNAME_SIZE in size
global C current_dir:byte         ;Array PATHNAME_SIZE in size
global C megazeux_drive:byte
global C current_drive:byte
global C quicksave_file:byte      ;Array FILENAME_SIZE in size
global C viewport_x:byte
global C viewport_y:byte
global C viewport_xsiz:byte
global C viewport_ysiz:byte
global C board_xsiz:word
global C board_ysiz:word
global C can_shoot:byte
global C can_bomb:byte
global C fire_burn_brown:byte
global C fire_burn_space:byte
global C fire_burn_fakes:byte
global C fire_burn_trees:byte
global C explosions_leave:byte
global C save_mode:byte
global C forest_becomes:byte
global C collect_bombs:byte
global C fire_burns:byte
global C board_dir:byte           ;Array 4 in size
global C restart_if_zapped:byte
global C time_limit:word
global C first_board:byte
global C clear_on_exit:byte
global C endgame_board:byte
global C endgame_x:word
global C endgame_y:word
global C game_over_sfx:byte
global C death_board:byte
global C death_x:word
global C death_y:word
global C only_from_swap:byte
global C starting_lives:word
global C lives_limit:word
global C starting_health:word
global C health_limit:word
global C last_key:byte
global C num_input:word
global C input_size:byte
global C volume:byte
global C volume_inc:byte
global C volume_target:byte
global C player_ns_locked:byte
global C player_ew_locked:byte
global C player_attack_locked:byte
global C input_string:byte        ;Array 81 in size
global C blind_dur:byte
global C firewalker_dur:byte
global C freeze_time_dur:byte
global C slow_time_dur:byte
global C wind_dur:byte
global C player_last_dir:byte
global C current_page:byte
global C current_pg_seg:word
global C status_shown_counters:byte;Array 4*COUNTER_NAME_SIZE in size
global C music_on:byte
global C sfx_on:byte
global C music_device:byte
global C mixing_rate:word
global C sfx_channels:byte
global C music_gvol:byte
global C sound_gvol:byte
global C overall_speed:byte
global C bottom_mesg:byte
global C b_mesg_timer:byte
global C b_mesg_row:byte
global C b_mesg_col:byte
global C player_x:word
global C player_y:word
global C pl_saved_x:word          ;Array 8 in size
global C pl_saved_y:word          ;Array 8 in size
global C pl_saved_board:byte      ;Array 8 in size
global C edge_color:byte
global C scroll_color:byte
global C lazwall_start:byte
global C scroll_x:word
global C scroll_y:word
global C locked_x:word
global C locked_y:word
global C protection_method:byte
global C password:byte            ;Array 16 in size
global C enemy_hurt_enemy:byte
global C cheats_active:byte
global C current_help_sec:byte
global C saved_pl_color:byte
global C flags:word               ;Array 128 in size
global C board_list:dword         ;Pointer
global C board_offsets:dword      ;Pointer
global C board_sizes:dword        ;Pointer
global C board_filenames:dword    ;Pointer
global C level_id:dword           ;Pointer
global C level_color:dword        ;Pointer
global C level_param:dword        ;Pointer
global C level_under_id:dword     ;Pointer
global C level_under_color:dword  ;Pointer
global C level_under_param:dword  ;Pointer
global C overlay:dword            ;Pointer
global C overlay_color:dword      ;Pointer
global C update_done:dword			 ;Pointer
global C overlay_mode:byte

global C under_player_id:byte
global C under_player_color:byte
global C under_player_param:byte
global C max_bsiz_mode:byte
global C max_bxsiz:word
global C max_bysiz:word
global C mesg_edges:byte
; New values for s2

global C saved_mouse_x:word
global C saved_mouse_y:word
global C saved_mouse_buttons:word
global C real_mod_playing:byte

; These are from DATA2.CPP

global C robots:dword
global C scrolls:dword
global C sensors:dword
global C counters:dword
