/*
 * Module: src/apu/apu.c
 * Responsibility: APU channel implementations and audio mixing.
 * Key invariants:
 *  - Pulse timers tick at CPU/2 rate, triangle at CPU rate
 *  - Noise shift register: 15-bit LFSR, feedback from bits 0 and 1 (or 6 in mode 1)
 *  - Mixer: pulse_out = 95.88/(8128/(p1+p2)+100), tnd uses similar nonlinear formula
 * Notes:
 *  - length_table[] indexed by upper 5 bits of $4003/$4007/$400B/$400F
 *  - Frame counter mode 1 (5-step) triggers extra quarter/half frame on write
 */
#include "apu.h"
#include <string.h>
#include <math.h>

static const uint8_t length_table[32] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

static const uint8_t duty_cycles[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 0, 0, 0},
    {1, 0, 0, 1, 1, 1, 1, 1}
};

static const uint16_t noise_period[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

static const uint16_t dmc_period[16] = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
};

void apu_init(APU* apu) {
    memset(apu, 0, sizeof(APU));
    apu->noise.shift_register = 1;
    apu->pulse1.enabled = false;
    apu->pulse2.enabled = false;
    apu->triangle.enabled = false;
    apu->noise.enabled = false;
    apu->dmc.enabled = false;
    
    apu->audio_time_per_sample = 1.0f / 44100.0f;
    apu->audio_time = 0.0f;
}

void apu_reset(APU* apu) {
    apu_init(apu);
}

static void pulse_tick_timer(PulseChannel* pulse) {
    if (pulse->timer == 0) {
        pulse->timer = pulse->timer_load;
        pulse->duty_sequence_step = (pulse->duty_sequence_step + 1) & 7;
    } else {
        pulse->timer--;
    }
}

static void triangle_tick_timer(TriangleChannel* triangle) {
    if (triangle->timer == 0) {
        triangle->timer = triangle->timer_load;
        if (triangle->length_counter > 0 && triangle->linear_counter > 0) {
            triangle->sequencer_step = (triangle->sequencer_step + 1) & 31;
        }
    } else {
        triangle->timer--;
    }
}

static void noise_tick_timer(NoiseChannel* noise) {
    if (noise->timer == 0) {
        noise->timer = noise->timer_load;
        uint16_t shift = noise->mode_flag ? 6 : 1;
        uint16_t feedback = (noise->shift_register & 1) ^ ((noise->shift_register >> shift) & 1);
        noise->shift_register = (noise->shift_register >> 1) | (feedback << 14);
    } else {
        noise->timer--;
    }
}

static void tick_length_counter(PulseChannel* pulse) {
    if (!pulse->envelope_loop && pulse->length_counter > 0) {
        pulse->length_counter--;
    }
}

static void tick_triangle_length(TriangleChannel* triangle) {
    if (!triangle->length_halt && triangle->length_counter > 0) {
        triangle->length_counter--;
    }
}

static void tick_noise_length(NoiseChannel* noise) {
    if (!noise->length_halt && noise->length_counter > 0) {
        noise->length_counter--;
    }
}

static void tick_envelope(PulseChannel* pulse) {
    if (pulse->envelope_start) {
        pulse->envelope_start = false;
        pulse->envelope_counter = pulse->envelope_period;
        pulse->envelope_value = 15;
    } else {
        if (pulse->envelope_counter > 0) {
            pulse->envelope_counter--;
        } else {
            pulse->envelope_counter = pulse->envelope_period;
            if (pulse->envelope_value > 0) {
                pulse->envelope_value--;
            } else if (pulse->envelope_loop) {
                pulse->envelope_value = 15;
            }
        }
    }
}

static void tick_noise_envelope(NoiseChannel* noise) {
    if (noise->envelope_start) {
        noise->envelope_start = false;
        noise->envelope_counter = noise->envelope_period;
        noise->envelope_value = 15;
    } else {
        if (noise->envelope_counter > 0) {
            noise->envelope_counter--;
        } else {
            noise->envelope_counter = noise->envelope_period;
            if (noise->envelope_value > 0) {
                noise->envelope_value--;
            } else if (noise->length_halt) {
                noise->envelope_value = 15;
            }
        }
    }
}

static void tick_linear_counter(TriangleChannel* triangle) {
    if (triangle->linear_reload) {
        triangle->linear_counter = triangle->linear_counter_reload_value;
    } else if (triangle->linear_counter > 0) {
        triangle->linear_counter--;
    }
    if (!triangle->length_halt) {
        triangle->linear_reload = false;
    }
}

static void tick_sweep(PulseChannel* pulse, int channel) {
    uint16_t current_period = pulse->timer_load;
    uint16_t change = current_period >> pulse->sweep_shift;
    
    if (pulse->sweep_negate) {
        change = (uint16_t)(-change);
        if (channel == 1) change--;
    }
    
    uint16_t target_period = current_period + change;
    
    bool mute = (current_period < 8) || (target_period > 0x7FF);
    
    if (pulse->sweep_counter == 0 && pulse->sweep_enabled && !mute && pulse->sweep_shift > 0) {
        pulse->timer_load = target_period;
    }
    
    if (pulse->sweep_counter == 0 || pulse->sweep_reload) {
        pulse->sweep_counter = pulse->sweep_period;
        pulse->sweep_reload = false;
    } else {
        pulse->sweep_counter--;
    }
}

void apu_tick(APU* apu) {
    if (apu->frame_count % 2 == 0) {
        pulse_tick_timer(&apu->pulse1);
        pulse_tick_timer(&apu->pulse2);
        noise_tick_timer(&apu->noise);
    }
    triangle_tick_timer(&apu->triangle);
    
    if (apu->frame_count % 7457 == 0) {
        tick_envelope(&apu->pulse1);
        tick_envelope(&apu->pulse2);
        tick_noise_envelope(&apu->noise);
        tick_linear_counter(&apu->triangle);
        
        static int step = 0;
        step++;
        if (step == 2 || step == 4) {
            tick_length_counter(&apu->pulse1);
            tick_length_counter(&apu->pulse2);
            tick_triangle_length(&apu->triangle);
            tick_noise_length(&apu->noise);
            tick_sweep(&apu->pulse1, 0);
            tick_sweep(&apu->pulse2, 1);
        }
        if (step == 4) step = 0;
    }
    
    apu->frame_count++;
    
    apu->audio_time += (1.0f / 1789773.0f);
    if (apu->audio_time >= apu->audio_time_per_sample) {
        apu->audio_time -= apu->audio_time_per_sample;
        float sample = apu_get_sample(apu);
        if (apu->sample_count < 1024) {
            apu->sample_buffer[apu->sample_count++] = sample;
        }
    }
}

uint8_t apu_read(APU* apu, uint16_t addr) {
    if (addr == APU_STATUS) {
        uint8_t status = 0;
        if (apu->pulse1.length_counter > 0) status |= 0x01;
        if (apu->pulse2.length_counter > 0) status |= 0x02;
        if (apu->triangle.length_counter > 0) status |= 0x04;
        if (apu->noise.length_counter > 0) status |= 0x08;
        if (apu->dmc.bytes_remaining > 0) status |= 0x10;
        if (apu->frame_irq) status |= 0x40;
        if (apu->dmc.irq_enabled) status |= 0x80;
        
        apu->frame_irq = false;
        return status;
    }
    return 0;
}

void apu_write(APU* apu, uint16_t addr, uint8_t val) {
    switch (addr) {
        case APU_PULSE1_VOL:
            apu->pulse1.duty_mode = (val >> 6) & 3;
            apu->pulse1.envelope_loop = (val >> 5) & 1;
            apu->pulse1.constant_volume = (val >> 4) & 1;
            apu->pulse1.volume = val & 0xF;
            apu->pulse1.envelope_period = val & 0xF;
            break;
        case APU_PULSE1_SWEEP:
            apu->pulse1.sweep_enabled = (val >> 7) & 1;
            apu->pulse1.sweep_period = (val >> 4) & 7;
            apu->pulse1.sweep_negate = (val >> 3) & 1;
            apu->pulse1.sweep_shift = val & 7;
            apu->pulse1.sweep_reload = true;
            break;
        case APU_PULSE1_LO:
            apu->pulse1.timer_load = (apu->pulse1.timer_load & 0xFF00) | val;
            break;
        case APU_PULSE1_HI:
            apu->pulse1.timer_load = (apu->pulse1.timer_load & 0x00FF) | ((val & 7) << 8);
            if (apu->pulse1.enabled) {
                apu->pulse1.length_counter = length_table[val >> 3];
            }
            apu->pulse1.envelope_start = true;
            apu->pulse1.duty_sequence_step = 0;
            break;
            
        case APU_PULSE2_VOL:
            apu->pulse2.duty_mode = (val >> 6) & 3;
            apu->pulse2.envelope_loop = (val >> 5) & 1;
            apu->pulse2.constant_volume = (val >> 4) & 1;
            apu->pulse2.volume = val & 0xF;
            apu->pulse2.envelope_period = val & 0xF;
            break;
        case APU_PULSE2_SWEEP:
            apu->pulse2.sweep_enabled = (val >> 7) & 1;
            apu->pulse2.sweep_period = (val >> 4) & 7;
            apu->pulse2.sweep_negate = (val >> 3) & 1;
            apu->pulse2.sweep_shift = val & 7;
            apu->pulse2.sweep_reload = true;
            break;
        case APU_PULSE2_LO:
            apu->pulse2.timer_load = (apu->pulse2.timer_load & 0xFF00) | val;
            break;
        case APU_PULSE2_HI:
            apu->pulse2.timer_load = (apu->pulse2.timer_load & 0x00FF) | ((val & 7) << 8);
            if (apu->pulse2.enabled) {
                apu->pulse2.length_counter = length_table[val >> 3];
            }
            apu->pulse2.envelope_start = true;
            apu->pulse2.duty_sequence_step = 0;
            break;
            
        case APU_TRIANGLE_LINEAR:
            apu->triangle.length_halt = (val >> 7) & 1;
            apu->triangle.linear_counter_reload_value = val & 0x7F;
            break;
        case APU_TRIANGLE_LO:
            apu->triangle.timer_load = (apu->triangle.timer_load & 0xFF00) | val;
            break;
        case APU_TRIANGLE_HI:
            apu->triangle.timer_load = (apu->triangle.timer_load & 0x00FF) | ((val & 7) << 8);
            if (apu->triangle.enabled) {
                apu->triangle.length_counter = length_table[val >> 3];
            }
            apu->triangle.linear_reload = true;
            break;
            
        case APU_NOISE_VOL:
            apu->noise.length_halt = (val >> 5) & 1;
            apu->noise.constant_volume = (val >> 4) & 1;
            apu->noise.volume = val & 0xF;
            apu->noise.envelope_period = val & 0xF;
            break;
        case APU_NOISE_LO:
            apu->noise.mode_flag = (val >> 7) & 1;
            apu->noise.timer_load = noise_period[val & 0xF];
            break;
        case APU_NOISE_HI:
            if (apu->noise.enabled) {
                apu->noise.length_counter = length_table[val >> 3];
            }
            apu->noise.envelope_start = true;
            break;
            
        case APU_STATUS:
            apu->pulse1.enabled = (val & 1);
            if (!apu->pulse1.enabled) apu->pulse1.length_counter = 0;
            
            apu->pulse2.enabled = (val & 2);
            if (!apu->pulse2.enabled) apu->pulse2.length_counter = 0;
            
            apu->triangle.enabled = (val & 4);
            if (!apu->triangle.enabled) apu->triangle.length_counter = 0;
            
            apu->noise.enabled = (val & 8);
            if (!apu->noise.enabled) apu->noise.length_counter = 0;
            
            apu->dmc.enabled = (val & 16);
            if (!apu->dmc.enabled) apu->dmc.bytes_remaining = 0;
            
            apu->frame_irq = false;
            break;
            
        case APU_FRAME_COUNTER:
            apu->frame_counter_mode = (val >> 7) & 1;
            apu->irq_inhibit = (val >> 6) & 1;
            if (apu->irq_inhibit) apu->frame_irq = false;
            if (apu->frame_counter_mode) {
                tick_envelope(&apu->pulse1);
                tick_envelope(&apu->pulse2);
                tick_noise_envelope(&apu->noise);
                tick_linear_counter(&apu->triangle);
                tick_length_counter(&apu->pulse1);
                tick_length_counter(&apu->pulse2);
                tick_triangle_length(&apu->triangle);
                tick_noise_length(&apu->noise);
                tick_sweep(&apu->pulse1, 0);
                tick_sweep(&apu->pulse2, 1);
            }
            break;
    }
}

float apu_get_sample(APU* apu) {
    float pulse1_out = 0, pulse2_out = 0;
    
    if (apu->pulse1.length_counter > 0 && apu->pulse1.timer_load > 8) {
        if (duty_cycles[apu->pulse1.duty_mode][apu->pulse1.duty_sequence_step]) {
            pulse1_out = (apu->pulse1.constant_volume ? apu->pulse1.volume : apu->pulse1.envelope_value);
        }
    }
    
    if (apu->pulse2.length_counter > 0 && apu->pulse2.timer_load > 8) {
        if (duty_cycles[apu->pulse2.duty_mode][apu->pulse2.duty_sequence_step]) {
            pulse2_out = (apu->pulse2.constant_volume ? apu->pulse2.volume : apu->pulse2.envelope_value);
        }
    }
    
    float triangle_out = 0;
    if (apu->triangle.length_counter > 0 && apu->triangle.linear_counter > 0) {
        uint8_t seq = apu->triangle.sequencer_step;
        triangle_out = (seq < 16) ? (15 - seq) : (seq - 16);
    }
    
    float noise_out = 0;
    if (apu->noise.length_counter > 0 && !(apu->noise.shift_register & 1)) {
        noise_out = (apu->noise.constant_volume ? apu->noise.volume : apu->noise.envelope_value);
    }
    
    float dmc_out = apu->dmc.output_level;
    
    float pulse_mix = 0;
    if (pulse1_out > 0 || pulse2_out > 0) {
        pulse_mix = 95.88f / ((8128.0f / (pulse1_out + pulse2_out)) + 100.0f);
    }
    
    float tnd_mix = 0;
    if (triangle_out > 0 || noise_out > 0 || dmc_out > 0) {
        tnd_mix = 159.79f / ((1.0f / ((triangle_out / 8227.0f) + (noise_out / 12241.0f) + (dmc_out / 22638.0f))) + 100.0f);
    }
    
    return pulse_mix + tnd_mix;
}

int apu_get_buffer(APU* apu, float* buffer, int max_samples) {
    int count = 0;
    for (int i = 0; i < apu->sample_count && i < max_samples; i++) {
        buffer[i] = apu->sample_buffer[i];
        count++;
    }
    apu->sample_count = 0;
    return count;
}
