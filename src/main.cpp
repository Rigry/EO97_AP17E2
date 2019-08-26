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
      bool mode           = 0;
   } flash;
   
   [[maybe_unused]] auto _ = Flash_updater<
        mcu::FLASH::Sector::_9
      , mcu::FLASH::Sector::_10
   >::make (&flash);

   decltype(auto) led_green = Pin::make<LED_green, mcu::PinMode::Output>();
   decltype(auto) led_red   = Pin::make<LED_red, mcu::PinMode::Output>();

   // struct Operation {
   //    bool manual :1; //Bit 0-1 manual (0), auto(1): 
   //    uint16_t    :15; //Bits 15:2 res: Reserved, must be kept cleared
   // };//__attribute__((packed));

   struct In_regs {
   
      UART::Settings uart_set;         // 0
      uint16_t modbus_address;         // 1
      uint16_t password;               // 2
      uint16_t factory_number;         // 3
      uint16_t frequency;              // 4
      uint16_t power;                  // 5
      // Operation operation;             // 6

   }__attribute__((packed));

   struct Out_regs {

      uint16_t device_code;            // 0
      uint16_t factory_number;         // 1
      UART::Settings uart_set;         // 2
      uint16_t modbus_address;         // 3
      uint16_t duty_cycle;             // 4
      uint16_t frequency;              // 5
      uint16_t resonanse;              // 6
      uint16_t current;                // 7
      uint16_t current_resonance;      // 8
      uint16_t temperatura;            // 9
      // Operation operation;             // 10

   };//__attribute__((packed));

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

   constexpr auto conversion_on_channel {16};
   struct ADC_{
      ADC_average& control = ADC_average::make<mcu::Periph::ADC1>(conversion_on_channel);
      ADC_channel& current = control.add_channel<mcu::PB0>();
   };
   
   volatile decltype(auto) pwm = PWM::make<mcu::Periph::TIM3, PWM_pin>(490);
   pwm.out_enable();
   volatile decltype(auto) encoder = Encoder::make<mcu::Periph::TIM8, mcu::PC6, mcu::PC7, true>();
   encoder = 18000;
   ADC_ adc;

   auto up    = Button<Right>();
   auto down  = Button<Left>();
   auto enter = Button<Enter>();
   constexpr auto hd44780_pins = HD44780_pins<RS, RW, E, DB4, DB5, DB6, DB7>{};
   [[maybe_unused]] auto menu = Menu (
      hd44780_pins, up, down, enter
      , flash
      , modbus.outRegs
   );

   
   while(1){
      
      pwm.frequency = encoder;
      modbus.outRegs.frequency = pwm.frequency ;
      led_green = true;
      led_red = true;
      // __WFI();
   }
}
