#pragma once

#include "pin.h"
#include "pwm_.h"
#include "adc.h"
#include "flash.h"
#include "timers.h"
#include "encoder.h"
#include "button_old.h"
#include "modbus_slave.h"
#include "literals.h"

// #if defined(USE_MOCK_PWM)
// using PWM_t = mock::PWM;
// #else
// using PWM_t = ::PWM;
// #endif

constexpr auto conversion_on_channel {16};
struct ADC_{
   ADC_average& control = ADC_average::make<mcu::Periph::ADC1>(conversion_on_channel);
   ADC_channel& current = control.add_channel<mcu::PB0>();
};

struct Operation {
   enum Mode      {wait = 0b00, auto_search, manual_search, work, emergency};
   Mode mode :3; //Bit 0-1 Mode: wait (00), auto_search mode (01), manual_search (10), work(11), emergency(100);
   uint16_t  :13; //Bits 15:2 res: Reserved, must be kept cleared
}__attribute__((packed));

struct In_regs {
   
   UART::Settings uart_set;         // 0
   uint16_t modbus_address;         // 1
   uint16_t password;               // 2
   uint16_t factory_number;         // 3
   uint16_t frequency;              // 4
   uint16_t power;                  // 5
   Operation operation;             // 6

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
   enum State {wait, pause, scan_up, scan_down, set_resonance, set_power, work} state {State::wait};
   
   ADC_& adc;
   PWM& pwm;
   Pin& led_green;
   Pin& led_red;
   Flash& flash;
   mcu::Button& enter;
   mcu::Button& reset;
   Modbus& modbus;
   Encoder& encoder;
   Timer timer{100_ms};
   Timer on_power{1000_ms};
   Timer delay {};
   uint16_t current{0};
   uint16_t current_up{0};
   uint16_t current_down{0};
   uint16_t resonance{0};
   uint16_t resonance_up{0};
   uint16_t resonance_down{0};
   uint16_t min_frequency {19_kHz};
   uint16_t max_frequency {25_kHz};
   uint16_t duty_cycle {200};
   int16_t  step {10_Hz};
   bool scan {false};
   bool search{true};
   bool power();
   bool scanning_up();
   bool scanning_down ();
   bool is_resonance();
   uint16_t milliamper (uint16_t adc) 
   {
      return adc * 3.3 * 0.125 / (16 * 4095) * 1000; // 0.125 коэффициент для пересчета, 1000 - для передачи в мА
   }

public:
   Task(ADC_& adc, PWM& pwm, Pin& led_green, Pin& led_red, Flash& flash, mcu::Button& enter, mcu::Button& reset, Modbus& modbus, Encoder& encoder) 
      : adc {adc}
      , pwm {pwm}
      , led_green {led_green}
      , led_red {led_red}
      , flash {flash}
      , enter {enter}
      , reset {reset}
      , modbus {modbus}
      , encoder {encoder}
   {
      // adc.control.set_callback ([&]{
      //    pwm.duty_cycle += adc.power > modbus.inRegs.power ? -1 : 1;
      // });
      adc.control.start();
      encoder = 1800;
      pwm.duty_cycle = 100;
      pwm.frequency = max_frequency;
      
   }

   void operator()() {

      led_red   = search ^= reset;
      led_green = pwm    ^= enter;
      state = pwm ? state : State::wait;
      modbus.outRegs.frequency          = pwm.frequency;
      modbus.outRegs.resonanse          = resonance;
      modbus.outRegs.current            = milliamper(adc.current);
      modbus.outRegs.current_resonance  = milliamper(current);
      modbus.outRegs.duty_cycle         = pwm.duty_cycle;
      
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

      switch (state) {
         case wait:
            if (pwm & not search) {
               delay.start(3000_ms);
               state = State::set_resonance;
            } else if (pwm & search) {
               state = State::pause;
            }
         break;
         case pause:
            if (delay.done()) {
               delay.stop();
               current = 0;
               current_up = 0;
               current_down = 0;
               resonance = 0;
               pwm.frequency = max_frequency;
               state = State::scan_down;
            }
         break;
         case scan_down:
            if (not scanning_down())
               state = State::scan_up;
         break;
         case scan_up:
            if (not scanning_up()) {
               search = false;
               state = State::set_resonance;
            }
         break;
         case set_resonance:
            if (is_resonance()) 
               state = State::set_power;
         break;
         case set_power:
            if (power()) 
               state = State::work;
         break;
         case work:
            pwm.frequency = encoder;
            // if (timer.event()) {
            //    if (pwm.frequency < resonance) {
            //       if (adc.current < current * 0.8)
            //          pwm.frequency += step;
            //       else if (adc.current > current * 0.9)
            //          pwm.frequency += -step;
            //    } else if (pwm.frequency > resonance) {
            //       if (adc.current < current * 0.8)
            //          pwm.frequency += -step;
            //       else if (adc.current > current * 0.9)
            //          pwm.frequency += step;
            //    }
            // }
         break;
      } //switch (state)

   }//operator

};

template<class Flash, class Modbus>
bool Task<Flash, Modbus>::scanning_down ()
{  
   pwm.duty_cycle = 100;

   if (adc.current > current_down) {
      current_down = adc.current;
      resonance_down = pwm.frequency;
   }
   
   if (timer.event())
      pwm.frequency += pwm.frequency > min_frequency ? -step : 0;
   
   return pwm.frequency != min_frequency ? true : false;      
}

template<class Flash, class Modbus>
bool Task<Flash, Modbus>::scanning_up ()
{  
   pwm.duty_cycle = 100;

   if (adc.current > current_up) {
      current_up = adc.current;
      resonance_up = pwm.frequency;
   }
   
   if (timer.event())
      pwm.frequency += pwm.frequency < max_frequency ? step : 0;
   
   return pwm.frequency != max_frequency ? true : false;      
}

template<class Flash, class Modbus>
bool Task<Flash, Modbus>::is_resonance ()
{
   resonance = current_up > current_down ? resonance_up : resonance_down;
   encoder   = resonance;
   if (timer.event()) {
      if (pwm.frequency >= resonance)
         pwm.frequency += pwm.frequency > resonance ? -step : 0;
      else
         pwm.frequency += pwm.frequency < resonance ? step : 0;
   }     
   return pwm.frequency == resonance;
}

template<class Flash, class Modbus>
bool Task<Flash, Modbus>::power ()
{
   if (adc.current > current) {
      current = adc.current;
   }
   if (on_power.event())   
      pwm.duty_cycle += pwm.duty_cycle < duty_cycle ? 1 : -1;

   return pwm.duty_cycle >= duty_cycle;
}