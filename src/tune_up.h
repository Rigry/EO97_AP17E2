#pragma once

#include "pin.h"
#include "pwm_.h"
#include "adc.h"
#include "flash.h"
#include "timers.h"
#include "encoder.h"
#include "button_old.h"
#include "modbus_slave.h"

// #if defined(USE_MOCK_PWM)
// using PWM_t = mock::PWM;
// #else
// using PWM_t = ::PWM;
// #endif

constexpr auto conversion_on_channel {16};
struct ADC_{
   ADC_average& control = ADC_average::make<mcu::Periph::ADC1>(conversion_on_channel);
   ADC_channel& power = control.add_channel<mcu::PA4>();
};

struct In_regs {
   
   UART::Settings uart_set;         // 0
   uint16_t modbus_address;         // 1
   uint16_t password;               // 2
   uint16_t factory_number;         // 3
   uint16_t frequency;              // 4
   uint16_t power;                  // 5
   uint16_t temperature;            // 6
   uint16_t number_line;            // 7

}__attribute__((packed));

struct Out_regs {

   uint16_t device_code;            // 0
   uint16_t factory_number;         // 1
   UART::Settings uart_set;         // 2
   uint16_t modbus_address;         // 3
   uint16_t frequency;              // 4
   uint16_t duty_cycle;             // 5
   uint16_t power;                  // 6

}__attribute__((packed));

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
};

#define ADR(reg) GET_ADR(In_regs, reg)

template<class Flash, class Modbus>
class Task
{
   ADC_& adc;
   PWM& pwm;
   Pin& led;
   Flash& flash;
   mcu::Button& enter;
   Modbus& modbus;
   Encoder& encoder;
   Timer timer{10};

public:
   Task(ADC_& adc, PWM& pwm, Pin& led, Flash& flash, mcu::Button& enter, Modbus& modbus, Encoder& encoder) 
      : adc {adc}
      , pwm {pwm}
      , led {led}
      , flash {flash}
      , enter {enter}
      , modbus {modbus}
      , encoder {encoder}
   {
      // adc.control.set_callback ([&]{
      //    pwm.duty_cycle += adc.power > modbus.inRegs.power ? -1 : 1;
      // });
      adc.control.start();
      encoder = 1800;
      pwm.duty_cycle = 490;
      
   }

   void operator()() {

      modbus.outRegs.power = adc.power;
      modbus.outRegs.duty_cycle = pwm.duty_cycle;
      
      modbus([&](uint16_t registrAddress) {
            static bool unblock = false;
         switch (registrAddress) {
            case ADR(uart_set):
               flash.uart_set
                  = modbus.outRegs.uart_set
                  = modbus.inRegs.uart_set;
            break;
            case ADR(modbus_address):
               flash.modbus_address 
                  = modbus.outRegs.modbus_address
                  = modbus.inRegs.modbus_address;
            break;
            case ADR(password):
               unblock = modbus.inRegs.password == 208;
            break;
            case ADR(factory_number):
               if (unblock) {
                  unblock = false;
                  flash.factory_number 
                     = modbus.outRegs.factory_number
                     = modbus.inRegs.factory_number;
               }
               unblock = true;
            break;
         } // switch
      });
      
      pwm.frequency = modbus.outRegs.frequency = encoder * 10;
      led = pwm ^= enter;
         
   }//operator

   // 
   // 
};