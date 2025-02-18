#include "fm.h"

#include "hardware/pio.h"
#include <hardware/clocks.h>
#include "hardware/dma.h"

// --------- //
// audio_i2s //
// --------- //

// #define audio_i2s_wrap_target 0
// #define audio_i2s_wrap 7

#define audio_i2s_offset_entry_point 0u

static const uint16_t audio_i2s_program_instructions[] = {
            //     .wrap_target
    0xa042, //  0: nop                    side 0     
    0xb042, //  1: nop                    side 1     
            //     .wrap
};

static const struct pio_program audio_i2s_program = {
    .instructions = audio_i2s_program_instructions,
    .length = 2,
    .origin = -1,
};

static inline pio_sm_config audio_i2s_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset , offset + 1);
    sm_config_set_sideset(&c, 1, false, false);
    return c;
};

static uint sm_i2s=-1;

static inline void set_rf_freq(int32_t freq)
{
    sm_config_set_clkdiv(PIO_I2S->sm, clock_get_hz(clk_sys) / (10000000 + freq / 1.0));
};
static inline void audio_i2s_program_init(PIO pio,  uint offset, uint data_pin, uint clock_pin_base) {
    sm_i2s  = pio_claim_unused_sm(pio, true);
    uint sm = sm_i2s;

    uint8_t func=(pio==pio0)?GPIO_FUNC_PIO0:GPIO_FUNC_PIO1;    // TODO: GPIO_FUNC_PIO0 for pio0 or GPIO_FUNC_PIO1 for pio1
    gpio_set_function(data_pin, func);
    gpio_set_function(clock_pin_base, func);
    gpio_set_function(clock_pin_base+1, func);

    pio_sm_config sm_config = audio_i2s_program_get_default_config(offset);
    sm_config_set_out_pins(&sm_config, data_pin, 1);
    sm_config_set_sideset_pins(&sm_config, clock_pin_base);
    sm_config_set_out_shift(&sm_config, false, true, 32);
    pio_sm_init(pio, sm, offset, &sm_config);
    uint pin_mask = (1u << data_pin) | (1u << clock_pin_base);
    pio_sm_set_pindirs_with_mask(pio, sm, pin_mask, pin_mask);
    pio_sm_set_pins(pio, sm, 0); // clear pins
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + audio_i2s_offset_entry_point));


    uint32_t sample_freq=44100;//44100;
    sample_freq*=4;
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    uint32_t divider = system_clock_frequency * 4 / sample_freq; // avoid arithmetic overflow

    pio_sm_set_clkdiv_int_frac(pio, sm , divider >> 8u, divider & 0xffu);

    pio_sm_set_enabled(pio, sm, true);
}

static uint32_t i2s_data;
static uint32_t trans_count_DMA=1<<30;

void fm_init(){
    uint offset = pio_add_program(PIO_I2S, &audio_i2s_program);
    audio_i2s_program_init(PIO_I2S, offset, I2S_DATA_PIN , I2S_CLK_BASE_PIN);

    int dma_i2s=dma_claim_unused_channel(true);
	int dma_i2s_ctrl=dma_claim_unused_channel(true);

    //основной рабочий канал
	dma_channel_config cfg_dma = dma_channel_get_default_config(dma_i2s);
	channel_config_set_transfer_data_size(&cfg_dma, DMA_SIZE_32);
	channel_config_set_chain_to(&cfg_dma, dma_i2s_ctrl);// chain to other channel

	channel_config_set_read_increment(&cfg_dma, false);
	channel_config_set_write_increment(&cfg_dma, false);



	uint dreq=DREQ_PIO1_TX0+sm_i2s;
	if (PIO_I2S==pio0) dreq=DREQ_PIO0_TX0+sm_i2s;
	channel_config_set_dreq(&cfg_dma, dreq);

	dma_channel_configure(
		dma_i2s,
		&cfg_dma,
		&PIO_I2S->txf[sm_i2s],		// Write address
		&i2s_data,					// read address
		1<<10,					//
		false			 				// Don't start yet
	);


    //контрольный канал для основного(перезапуск)
	cfg_dma = dma_channel_get_default_config(dma_i2s_ctrl);
	channel_config_set_transfer_data_size(&cfg_dma, DMA_SIZE_32);
	//channel_config_set_chain_to(&cfg_dma, dma_i2s);// chain to other channel
	
	channel_config_set_read_increment(&cfg_dma, false);
	channel_config_set_write_increment(&cfg_dma, false);



	dma_channel_configure(
		dma_i2s_ctrl,
		&cfg_dma,
		&dma_hw->ch[dma_i2s].al1_transfer_count_trig,	// Write address
		// &dma_hw->ch[dma_chan].al2_transfer_count,
		&trans_count_DMA,					// read address
		1,									//
		false								// Don't start yet
	);
    // dma_start_channel_mask((1u << dma_i2s)) ;
};

void i2s_deinit(){

}

inline void fm_out(int16_t l_out,int16_t r_out){
    uint16_t corrected_sample = (uint16_t)((int32_t)l_out + 0x8000L) >> 1;

        set_rf_freq((int32_t )(r_out + l_out));
        
        
        };