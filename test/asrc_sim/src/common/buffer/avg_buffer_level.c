#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "avg_buffer_level.h"

void init_calc_buffer_level_state(buffer_calc_state_t *p_calc_state, int32_t window_len_log2, int32_t buffer_level_stable_threshold)
{
    memset(p_calc_state, 0, sizeof(buffer_calc_state_t));
    p_calc_state->window_len_log2 = window_len_log2;
    p_calc_state->buffer_level_stable_threshold = buffer_level_stable_threshold;
}

void calc_avg_buffer_level(buffer_calc_state_t *state, int current_level, bool reset)
{

    if(reset == true)
    {
        init_calc_buffer_level_state(state, state->window_len_log2, state->buffer_level_stable_threshold); // Reinitialise state
        printf("Reset avg I2S send buffer level\n");
    }

    state->error_accum += current_level;
    state->count += 1;

    if(state->count == (1<<state->window_len_log2))
    {
        if(state->flag_first_done == true)
        {
            state->flag_prev_avg_valid = true;
        }
        state->prev_avg_buffer_level = state->avg_buffer_level;
        state->avg_buffer_level = state->error_accum >> state->window_len_log2;
        if(state->flag_prev_avg_valid)
        {
            state->avg_buffer_level = (state->avg_buffer_level + state->prev_avg_buffer_level) / 2;
            if(state->flag_stable_avg == false)
            {
                state->buffer_level_stable_count += 1;

                if(state->buffer_level_stable_count > state->buffer_level_stable_threshold)
                {
                    state->stable_avg_level = state->avg_buffer_level;
                    printf("stable average calculated as %d\n", state->stable_avg_level);
                    state->flag_stable_avg = true;
                }
            }
        }
        state->count = 0;
        state->error_accum = 0;
        if(state->flag_first_done == false)
        {
            state->flag_first_done = true;
        }
    }
}
