#define STM32F405xx
#define F_OSC   8000000UL
#define F_CPU   168000000UL

#include "init_clock.h"
#include "periph_rcc.h"
#include "periph_flash.h"
#include "pin.h"
#include "literals.h"
// #include "main.h"
#include "tune_up.h"

extern "C" void init_clock () { init_clock<F_OSC, F_CPU>(); }

using TX  = mcu::PA9;
using RX  = mcu::PA10;
using RTS = mcu::PA12;
using PWM_pin = mcu::PC9;
using LED = mcu::PA15;
using Enter = mcu::PA8;

int main()
{
   Flash<Flash_data, mcu::FLASH::Sector::_10> flash{};

   decltype(auto) enter = mcu::Button::make<Enter>(); 
   decltype(auto) led = Pin::make<LED, mcu::PinMode::Output>();

   decltype(auto) modbus = Modbus_slave<In_regs, Out_regs>
                 ::make<mcu::Periph::USART1, TX, RX, RTS>
                       (flash.modbus_address, flash.uart_set);

   volatile decltype(auto) pwm = PWM::make<mcu::Periph::TIM3, PWM_pin>(490);
   volatile decltype(auto) encoder = Encoder::make<mcu::Periph::TIM8, mcu::PC6, mcu::PC7, true>();
   // encoder = 18000;
   ADC_ adc;

   modbus.outRegs.device_code       = 9;
   modbus.outRegs.factory_number    = flash.factory_number;
   modbus.outRegs.modbus_address    = flash.modbus_address;
   modbus.outRegs.uart_set          = flash.uart_set;
   modbus.arInRegsMax[ADR(uart_set)]= 0b11111111;
   modbus.inRegsMin.modbus_address  = 1;
   modbus.inRegsMax.modbus_address  = 255;


   using Flash  = decltype(flash);
   using Modbus = Modbus_slave<In_regs, Out_regs>;
   // using Button = mcu::Button;

   Task<Flash, Modbus> task {adc, pwm, led, flash, enter, modbus, encoder};
   // Task<Flash, Modbus> task {adc, pwm, flash, modbus};  

   // adc.control.set_callback ([&]{
   //      led = adc.voltage < _2V;
   // });
      
   
   
   while(1){
      task();
      // __WFI();
   }
}
