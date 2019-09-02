#pragma once

#include "pin.h"
#include "pwm_.h"
#include "adc.h"
#include "flash.h"
#include "timers.h"
#include "encoder.h"
// #include "button_old.h"
#include "modbus_slave.h"
#include "literals.h"
   
constexpr auto conversion_on_channel {16};
struct ADC_{
   ADC_average& control = ADC_average::make<mcu::Periph::ADC1>(conversion_on_channel);
   ADC_channel& current = control.add_channel<mcu::PB0>();
};

// структура для меню, битовое поле не лезет 
struct Mode {
   bool on          {false};
   bool search      {false};
   bool manual      {false};
   bool manual_tune {false};
} state;

struct Operation {
   bool on            :1; 
   bool search        :1;
   bool manual        :1;
   bool manual_tune   :1;
   uint16_t           :12; //Bits 11:2 res: Reserved, must be kept cleared
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
      uint16_t resonance;              // 6
      uint16_t current;                // 7
      uint16_t current_resonance;      // 8
      uint16_t temperatura;            // 9
      Operation operation;             // 10

   };//__attribute__((packed));



#define ADR(reg) GET_ADR(In_regs, reg)

template<class Flash_data, class Modbus>
class Generator
{ 
   enum State {wait, manual, manual_tune, pause, scan_up, scan_down, set_resonance, set_power, work} state {State::wait};
   
   ADC_& adc;
   PWM& pwm;
   Pin& led_green;
   Pin& led_red;
   Mode& mode;
   Flash_data& flash;
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
   uint16_t range_frequency{ 2_kHz};
   uint16_t work_frequency {18_kHz};
   uint16_t min_frequency  {19_kHz};
   uint16_t max_frequency  {25_kHz};
   uint16_t duty_cycle {200};
   int16_t  step {10_Hz};
   bool scan {false};
   bool search{false};
   bool power();
   bool scanning_up();
   bool scanning_down ();
   bool is_resonance();

   uint16_t milliamper(uint16_t adc) { return adc * 3.3 * 0.125 / (16 * 4095) * 1000;} // 0.125 коэффициент для пересчета, 1000 - для передачи в мА

   void if_off_to_wait() { 
      if (state != State::manual and state != State::manual_tune)
         state = pwm ? state : State::wait; 
   }

public:
   
   Generator(ADC_& adc, PWM& pwm, Pin& led_green, Pin& led_red, Mode& mode
           , Flash_data& flash, Modbus& modbus, Encoder& encoder) 
      : adc {adc}
      , pwm {pwm}
      , led_green {led_green}
      , led_red {led_red}
      , mode {mode}
      , flash {flash}
      , modbus {modbus}
      , encoder {encoder}
   {
      // adc.control.set_callback ([&]{
      //    pwm.duty_cycle += adc.power > modbus.inRegs.power ? -1 : 1;
      // });
      adc.control.start();
      encoder = max_frequency;
      pwm.duty_cycle = 100;
      pwm.frequency = max_frequency;
   }

   void operator()() {

      (mode.on and flash.resonance) or (mode.on and search) ? pwm.out_enable() : pwm.out_disable(); // вкл по кнопке энкодера

      mode.on = pwm ? mode.on : false;
      
      led_red   = search; // индикация нужен поиск, погаснет, когда поиск закончится
      led_green = pwm;    // индикация работы генератора

      modbus.outRegs.frequency               = pwm.frequency;
      flash.resonance                        = 
      modbus.outRegs.resonance               = resonance;
      modbus.outRegs.current                 = milliamper(adc.current);
      modbus.outRegs.current_resonance       = milliamper(current);
      modbus.outRegs.duty_cycle              = pwm.duty_cycle;
      modbus.outRegs.operation.on            = mode.on;
      modbus.outRegs.operation.search        = search = flash.search;
      modbus.outRegs.operation.manual        = flash.manual;
      modbus.outRegs.operation.manual_tune   = flash.manual_tune;

      work_frequency = flash.work_frequency;
      min_frequency  = work_frequency - range_frequency;
      max_frequency  = work_frequency + range_frequency;
      duty_cycle     = flash.power * 5;
      
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
               unblock = modbus.inRegs.password == 127;
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
            if (flash.manual) {
               state = State::manual;
               pwm.frequency = encoder = work_frequency;
            } else if (flash.manual_tune) {
               state = State::manual_tune;
               pwm.frequency = encoder = work_frequency;
            } else if (pwm and not search) {
               delay.start(3000_ms);
               state = State::set_resonance;
            } else if (pwm and search) {
               state = State::pause;
            } 
         break;
         case manual:
            pwm.duty_cycle = duty_cycle;
            pwm.frequency  = encoder;
            if (not search) {
               flash.resonance = resonance = pwm.frequency;
            }
            if (not flash.manual) {
               state = State::wait;
            }
         break;
         case manual_tune:
            if (search) {
               pwm.duty_cycle = 100;
               pwm.frequency = encoder;
            } else {
               flash.resonance = resonance = pwm.frequency;
               state = State::set_power;
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
               flash.search = false;
               resonance = current_up > current_down ? resonance_up : resonance_down;
               state = State::set_resonance;
            }
         break;
         case set_resonance:
            if (is_resonance()) {
               state = State::set_power;
            }
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

      if_off_to_wait();

   }//operator

};

template<class Flash, class Modbus>
bool Generator<Flash, Modbus>::scanning_down ()
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
bool Generator<Flash, Modbus>::scanning_up ()
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
bool Generator<Flash, Modbus>::is_resonance ()
{
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
bool Generator<Flash, Modbus>::power ()
{
   if (adc.current > current) {
      current = adc.current;
   }
   if (on_power.event())   
      pwm.duty_cycle += pwm.duty_cycle < duty_cycle ? 1 : -1;

   return pwm.duty_cycle >= duty_cycle;
}