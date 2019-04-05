#define STM32F405xx
#define F_OSC   8000000UL
#define F_CPU   168000000UL

#include "init_clock.h"
#include "periph_rcc.h"
#include "periph_flash.h"
#include "pin.h"
#include "literals.h"
#include "main.h"

extern "C" void init_clock () { init_clock<F_OSC, F_CPU>(); }

using TX  = mcu::PA9;
using RX  = mcu::PA10;
using RTS = mcu::PA12;
using PWM_pin = mcu::PC9;

int main()
{
   Flash<Flash_data, mcu::FLASH::Sector::_8> flash{};

   decltype(auto) modbus = Modbus_slave<In_regs, Out_regs>
                 ::make<mcu::Periph::USART1, TX, RX, RTS>
                       (flash.modbus_address, flash.uart_set);

   decltype(auto) pwm = PWM::make<mcu::Periph::TIM3, PWM_pin>(990);
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

   Task<Flash, Modbus> task {adc, pwm, flash, modbus}; 

   // adc.control.set_callback ([&]{
   //      led = adc.voltage < _2V;
   // });
      
   
   
   while(1){
      task();
      __WFI();
   }
}
