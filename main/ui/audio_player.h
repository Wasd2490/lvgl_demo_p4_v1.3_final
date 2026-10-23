/* Multi-track PCM player using BSP ES8311 codec. */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void audio_player_init(void);
void audio_player_play(void);   /* restart current track */
void audio_player_play_track(int idx);
void audio_player_next(void);
void audio_player_prev(void);
void audio_player_pause(void);
void audio_player_stop(void);
bool audio_player_is_playing(void);
int  audio_player_track_count(void);
int  audio_player_track_index(void);
const char *audio_player_track_name(int i);
void audio_player_set_volume(int vol); /* 0..100 */
int  audio_player_track_duration_sec(void);
int  audio_player_position_sec(void);

#ifdef __cplusplus
}
#endif
