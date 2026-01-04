/*
 * Module: src/apu/apu.h
 * Responsibility: Audio Processing Unit - sound channels, frame counter, sample generation.
 * Key invariants:
 *  - 5 channels: 2 pulse, 1 triangle, 1 noise, 1 DMC (delta modulation)
 *  - Frame counter ticks envelope/length every ~7457 CPU cycles (240Hz)
 *  - Sample rate: 44100Hz; audio buffer holds up to 1024 samples
 * Notes:
 *  - Pulse sweep differs between channels (channel 1 uses ones' complement)
 *  - Length counter halt also serves as envelope loop flag for pulse/noise
 */
#ifndef APU_H
#define APU_H

#include <stdint.h>
#include <stdbool.h>

#define APU_PULSE1_VOL       0x4000
#define APU_PULSE1_SWEEP     0x4001
#define APU_PULSE1_LO        0x4002
#define APU_PULSE1_HI        0x4003

#define APU_PULSE2_VOL       0x4004
#define APU_PULSE2_SWEEP     0x4005
#define APU_PULSE2_LO        0x4006
#define APU_PULSE2_HI        0x4007

#define APU_TRIANGLE_LINEAR  0x4008
#define APU_TRIANGLE_LO      0x400A
#define APU_TRIANGLE_HI      0x400B

#define APU_NOISE_VOL        0x400C
#define APU_NOISE_LO         0x400E
#define APU_NOISE_HI         0x400F

#define APU_DMC_FREQ         0x4010
#define APU_DMC_RAW          0x4011
#define APU_DMC_START        0x4012
#define APU_DMC_LEN          0x4013

#define APU_STATUS           0x4015
#define APU_FRAME_COUNTER    0x4017

typedef struct {
    bool enabled;
    uint8_t duty_mode;
    uint8_t volume;
    bool constant_volume;
    bool envelope_start;
    bool envelope_loop;
    uint8_t envelope_period;
    uint8_t envelope_value;
    uint8_t envelope_counter;
    
    bool sweep_enabled;
    uint8_t sweep_period;
    bool sweep_negate;
    uint8_t sweep_shift;
    bool sweep_reload;
    uint8_t sweep_counter;
    
    uint16_t timer;
    uint16_t timer_load;
    uint16_t length_counter;
    
    uint8_t duty_sequence_step;
} PulseChannel;

typedef struct {
    bool enabled;
    bool length_halt;
    uint8_t linear_counter_reload_value;
    uint16_t timer;
    uint16_t timer_load;
    uint16_t length_counter;
    uint8_t linear_counter;
    bool linear_reload;
    
    uint8_t sequencer_step;
} TriangleChannel;

typedef struct {
    bool enabled;
    bool length_halt;
    bool constant_volume;
    uint8_t volume;
    bool envelope_start;
    uint8_t envelope_period;
    uint8_t envelope_value;
    uint8_t envelope_counter;
    
    uint16_t timer;
    uint16_t timer_load;
    uint16_t length_counter;
    
    uint16_t shift_register;
    bool mode_flag;
} NoiseChannel;

typedef struct {
    bool enabled;
    bool loop;
    bool irq_enabled;
    uint16_t sample_address;
    uint16_t sample_length;
    uint16_t current_address;
    uint16_t bytes_remaining;
    
    uint8_t output_level;
    uint8_t buffer;
    bool buffer_empty;
    uint8_t bits_remaining;
    uint16_t shift_register;
    
    uint16_t timer;
    uint16_t timer_load;
} DMCChannel;

typedef struct APU {
    PulseChannel pulse1;
    PulseChannel pulse2;
    TriangleChannel triangle;
    NoiseChannel noise;
    DMCChannel dmc;
    
    uint64_t frame_count;
    uint8_t frame_counter_mode;
    bool irq_inhibit;
    bool frame_irq;
    
    float audio_time;
    float audio_time_per_sample;
    float sample_buffer[1024];
    int sample_count;
    
} APU;

/* Initialize APU state (all channels disabled, noise shift = 1). */
void apu_init(APU* apu);

/* Reset APU to power-on state. */
void apu_reset(APU* apu);

/* Advance APU by one CPU cycle. Updates timers, frame counter, samples. */
void apu_tick(APU* apu);

/* Write to APU register ($4000-$4017). */
void apu_write(APU* apu, uint16_t addr, uint8_t val);

/* Read from APU status ($4015). Clears frame IRQ. */
uint8_t apu_read(APU* apu, uint16_t addr);

/* Get current mixed audio sample (pulse + TND). */
float apu_get_sample(APU* apu);

/* Copy samples to buffer, clear internal buffer. Returns count copied. */
int apu_get_buffer(APU* apu, float* buffer, int max_samples);

#endif
