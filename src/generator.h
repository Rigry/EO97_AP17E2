#pragma once

#include "pin.h"
#include "pwm_.h"
#include "adc.h"
#include "flash.h"
#include "timers.h"
#include "encoder.h"
#include "modbus_slave.h"
#include "literals.h"
#include "search.h"
#include "NTC_table.h"


constexpr auto conversion_on_channel {16};
struct ADC_1{
   ADC_average& control     = ADC_average::make<mcu::Periph::ADC1>(conversion_on_channel);
   ADC_channel& temperatura = control.add_channel<mcu::PA2>(); 
   ADC_channel& current     = control.add_channel<mcu::PB0>();
   
};

// struct ADC_2{
//    ADC_average& control     = ADC_average::make<mcu::Periph::ADC2>(conversion_on_channel);
//    ADC_channel& temperatura = control.add_channel<mcu::PA2>(); 
// };

struct Mode {
   bool on          {false};
   bool search      {false};
   bool manual      {false};
   bool manual_tune {false};
   bool overheat    {false};
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
   uint16_t m_resonance;            // 6
   uint16_t a_resonance;            // 7
   uint16_t current;                // 8
   uint16_t current_resonance;      // 9
   uint16_t temperatura;            // 10
   Operation operation;             // 11

};//__attribute__((packed));

#define ADR(reg) GET_ADR(In_regs, reg)

template<class Flash_data, class Modbus>
class Generator
{
   enum State {wait_, auto_search, manual_search, auto_control, manual_control, set_power, emergency} state{State::wait_}; 
   enum State_scan {wait, pause, scan_down, scan_up, set_resonance} state_scan{State_scan::wait};
   
   ADC_1& adc_current;
   // ADC_2& adc_temp;
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
   uint16_t temperatura{0};
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
   bool search{false};
   bool overheat{false};
   bool power();
   bool scanning();
   void select_mode();
   bool scanning_up();
   bool scanning_down ();
   bool is_resonance();

   uint16_t milliamper(uint16_t adc) { return adc * 3.3 * 0.125 / (16 * 4095) * 1000;} // 0.125 коэффициент для пересчета, 1000 - для передачи в мА
   void temp(uint16_t adc) {
      
      // auto p = std::lower_bound(
      //     std::begin(NTC::u2904<33,5100>),
      //     std::end(NTC::u2904<33,5100>),
      //     adc,
      //     std::greater<uint32_t>());
      // temp = (p - NTC::u2904<33,5100>);
      // return temp;
      adc = adc / 16;
      for (size_t i = 0; i <= std::size(NTC::u2904<33,5100>) - 2; i++) {
         if (adc < NTC::u2904<33,5100>[i] and adc > NTC::u2904<33,5100>[i + 1])
         temperatura = i;
      }
   }
   size_t U = 33;
   size_t R = 5100;

public:
   
   Generator(ADC_1& adc_current, PWM& pwm, Pin& led_green, Pin& led_red, Mode& mode
           , Flash_data& flash, Modbus& modbus, Encoder& encoder) 
      : adc_current {adc_current}
      // , adc_temp {adc_temp}
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
      // adc.control.set_callback ([&]{
      //    adc.temperatura = adc.temperatura / 16;
      //    for (size_t i = 0; i <= std::size(NTC::u2904<33,5100>) - 2; i++) {
      //       if (adc.temperatura < NTC::u2904<33,5100>[i] and adc.temperatura > NTC::u2904<33,5100>[i + 1])
      //       temperatura = i;
      //    }
      
      // // led_red = adc.temperature < t;
      // });
      adc_current.control.start();
      // adc_temp.control.start();
   }

   void operator()() {
      
      if (overheat |= temperatura > flash.temperatura)
            mode.overheat = overheat  = temperatura > flash.recovery;
      
      mode.on and (flash.m_resonance or search) and not overheat ? pwm.out_enable() : pwm.out_disable(); // вкл по кнопке энкодера

      mode.on = pwm or overheat ? mode.on : false;
      
      led_red   = search = flash.search; // индикация нужен поиск, погаснет, когда поиск закончится
      led_green = pwm;    // индикация работы генератора
      
      temp(adc_current.temperatura);

      modbus.outRegs.frequency               = pwm.frequency;
      // modbus.outRegs.frequency               = pwm.frequency;
      // flash.m_resonance                        = 
      // modbus.outRegs.m_resonance             = resonance;
      modbus.outRegs.current                 = milliamper(adc_current.current);
      modbus.outRegs.temperatura             = temperatura;
      // modbus.outRegs.current_resonance       = milliamper(current);
      modbus.outRegs.duty_cycle              = pwm.duty_cycle;
      // modbus.outRegs.operation.on            = mode.on;
      // modbus.outRegs.operation.search        = flash.m_search;
      // modbus.outRegs.operation.manual        = flash.m_control;
      // modbus.outRegs.operation.manual_tune   = flash.m_search;

      work_frequency = flash.work_frequency;
      min_frequency  = work_frequency - range_frequency;
      max_frequency  = work_frequency + range_frequency;
      duty_cycle = flash.power * 5;
      
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
      
      switch (state)
      {
         case wait_:
            if (search) {
               if (flash.m_search) {
                  pwm.duty_cycle = duty_cycle;
                  pwm.frequency = encoder = work_frequency;
                  state = State::manual_search;
               } else {
                  current = 0;
                  current_up = 0;
                  current_down = 0;
                  pwm.duty_cycle = 100;
                  pwm.frequency = max_frequency;
                  state = State::auto_search;
               }
            } else if(not search and pwm) {
               select_mode();
            } else {
               if (flash.m_search) {
                  pwm.frequency = encoder = flash.m_resonance;
               } else {
                  pwm.frequency = encoder = flash.a_resonance;
               }
            }
         break;
         case auto_search:
            if (pwm) {
               if (not scanning()) {
                  select_mode();
               }
            }
            if (not pwm and not search) state = State::wait_;
         break;
         case manual_search:
            pwm.frequency = encoder;
            if (not search) {
               flash.m_resonance = pwm.frequency;
               select_mode();
            }
            if (not pwm and not search) state = State::wait_;
         break;
         case set_power:
            if (power()) 
               state = State::auto_control;
            if (not pwm) state = State::wait_;
         break;
         case auto_control:
            if (pwm) {
               pwm.frequency = encoder;
            }
            if (not pwm) state = State::wait_;
         break;
         case manual_control:
            pwm.frequency = encoder;
            if (not pwm) state = State::wait_;
         break;
         case emergency:
         break;
      } //switch (state)
   } //void operator()()
};

template<class Flash, class Modbus>
void Generator<Flash, Modbus>::select_mode()
{
   if (flash.m_control) {
      pwm.duty_cycle = duty_cycle;
      state = State::manual_control;
   } else {
      pwm.duty_cycle = 100;
      state = State::set_power;
   }

   if (flash.m_search) {
      pwm.frequency = encoder = flash.m_resonance;
   } else {
      pwm.frequency = encoder = flash.a_resonance;
   }

}

template<class Flash, class Modbus>
bool Generator<Flash, Modbus>::scanning()
{
   bool tmp{false};
   switch (state_scan){
      
      case wait:
         delay.start(1000_ms);
         state_scan = State_scan::pause;
      break;
      case pause:
         if (delay.done()) {
            delay.stop();
            current = 0;
            current_up = 0;
            current_down = 0;
            resonance = 0;
            pwm.frequency = max_frequency;
            state_scan = State_scan::scan_down;
         }
      break;
      case scan_down:
         if (not scanning_down())
            state_scan = State_scan::scan_up; 
      break;
      case scan_up:
         if (not scanning_up()) {
            flash.search = false;
            flash.a_resonance = current_up > current_down ? resonance_up : resonance_down;
            state_scan = State_scan::set_resonance;
         }
      break;
      case set_resonance:
         if (is_resonance()) {
            tmp = true;
         }
      break;

   } //switch (state_scan)

   return not tmp;
}

template<class Flash, class Modbus>
bool Generator<Flash, Modbus>::scanning_down ()
{  
   pwm.duty_cycle = 100;

   if (adc_current.current > current_down) {
      current_down = adc_current.current;
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

   if (adc_current.current > current_up) {
      current_up = adc_current.current;
      resonance_up = pwm.frequency;
   }
   
   if (timer.event())
      pwm.frequency += pwm.frequency < max_frequency ? step : 0;
   
   return pwm.frequency != max_frequency ? true : false;      
}

template<class Flash, class Modbus>
bool Generator<Flash, Modbus>::is_resonance ()
{
   if (timer.event()) {
      if (pwm.frequency >= flash.a_resonance)
         pwm.frequency += pwm.frequency > flash.a_resonance ? -step : 0;
      else
         pwm.frequency += pwm.frequency < flash.a_resonance ? step : 0;
   }     
   return pwm.frequency == flash.a_resonance;
}

template<class Flash, class Modbus>
bool Generator<Flash, Modbus>::power ()
{
   if (adc_current.current > current) {
      flash.current = current = adc_current.current;
   }
   if (on_power.event())   
      pwm.duty_cycle += pwm.duty_cycle < duty_cycle ? 1 : -1;

   return pwm.duty_cycle >= duty_cycle;
}