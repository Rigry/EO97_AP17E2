#define STM32F405xx
#define F_OSC   8000000UL
#define F_CPU   168000000UL

#include "init_clock.h"
#include "periph_rcc.h"
#include "periph_flash.h"
#include "pin.h"
#include "literals.h"
#include "button.h"
#include "flash.h"
#include "timers.h"
#include "menu.h"
#include "modbus_slave.h"
#include "adc.h"
#include "pwm_.h"
#include "encoder.h"
// #include "tune_up.h"
#include "generator.h"

using TX  = mcu::PA9;
using RX  = mcu::PA10;
using RTS = mcu::PA12;
using PWM_pin = mcu::PC9;
using LED_red = mcu::PA15;
using LED_green = mcu::PC10;
using Enter = mcu::PA8;
using Left = mcu::PB13;
using Right = mcu::PB14;
using E   = mcu::PC14;       
using RW  = mcu::PC15;       
using RS  = mcu::PC13;      
using DB4 = mcu::PC2;       
using DB5 = mcu::PC3;
using DB6 = mcu::PC0;    
using DB7 = mcu::PC1;

extern "C" void init_clock () { init_clock<F_OSC, F_CPU>(); }

int main()
{
   struct Flash_data {
      uint16_t factory_number = 0;
      UART::Settings uart_set = {
         .parity_enable  = false,
         .parity         = USART::Parity::even,
         .data_bits      = USART::DataBits::_8,
         .stop_bits      = USART::StopBits::_1,
         .baudrate       = USART::Baudrate::BR9600,
         .res            = 0
      };
      uint8_t  modbus_address = 1;
      uint16_t model_number   = 0;
      uint16_t work_frequency = 18_kHz;
      uint16_t current        = 0;
      uint16_t m_resonance    = 1_kHz;
      uint16_t a_resonance    = 1_kHz;
      uint8_t  power          = 100_percent;
      uint8_t  temperatura    = 65;
      uint8_t  recovery       = 45;
      bool     m_search       = false;
      bool     m_control      = false;
      bool     search         = true;
   } flash;
   
   [[maybe_unused]] auto _ = Flash_updater<
        mcu::FLASH::Sector::_9
      , mcu::FLASH::Sector::_8
   >::make (&flash);

   
   decltype(auto) led_green = Pin::make<LED_green, mcu::PinMode::Output>();
   decltype(auto) led_red   = Pin::make<LED_red, mcu::PinMode::Output>();

   auto us_on = Button<Enter>();

   volatile decltype(auto) modbus = Modbus_slave<In_regs, Out_regs>
                 ::make<mcu::Periph::USART1, TX, RX, RTS>
                       (flash.modbus_address, flash.uart_set);

   #define ADR(reg) GET_ADR(In_regs, reg)
   modbus.outRegs.device_code       = 9;
   modbus.outRegs.factory_number    = flash.factory_number;
   modbus.outRegs.modbus_address    = flash.modbus_address;
   modbus.outRegs.uart_set          = flash.uart_set;
   // modbus.outRegs.operation.manual  = bool(flash.mode);       
   modbus.arInRegsMax[ADR(uart_set)]= 0b11111111;
   modbus.inRegsMin.modbus_address  = 1;
   modbus.inRegsMax.modbus_address  = 255;
   
   volatile decltype(auto) pwm = PWM::make<mcu::Periph::TIM3, PWM_pin>(490);
   volatile decltype(auto) encoder = Encoder::make<mcu::Periph::TIM8, mcu::PC6, mcu::PC7, true>();

   ADC_1 adc_current;
   // ADC_2 adc_temp;

   using Flash  = decltype(flash);
   using Modbus = Modbus_slave<In_regs, Out_regs>;

   Generator <Flash, Modbus> work {adc_current, pwm, led_green, led_red, state, flash, modbus, encoder};

   auto up    = Button<Right>();
   auto down  = Button<Left>();
   auto enter = Button<Enter>();
   // enter.set_down_callback([&]{flash.factory_number = 10;});
   constexpr auto hd44780_pins = HD44780_pins<RS, RW, E, DB4, DB5, DB6, DB7>{};
   [[maybe_unused]] auto menu = Menu (
      hd44780_pins, up, down, enter
      , flash
      , modbus.outRegs
      , state
   );
   
   while(1){
      work();
      __WFI();
   }
}
