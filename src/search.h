#pragma once

#include "pin.h"
#include "pwm_.h"
#include "adc.h"
#include "flash.h"
#include "timers.h"
#include "encoder.h"
#include "modbus_slave.h"
#include "literals.h"

template<class Flash_data>
class Search
{
   enum State {wait_, manual_, auto_, emergency_} state {State::wait_};
   
   PWM& pwm;
   Encoder& encoder;
   Flash_data& flash;

   uint16_t resonance;
   uint16_t frequency;
   uint16_t work_frequency;

public:

   Search(PWM& pwm, Encoder& encoder, Flash_data& flash)
      : pwm{pwm}
      , encoder{encoder}
      , flash{flash}
   {}

   // void enable(bool v) {enable_ = v;}
   // void search(bool v) {search_ = v;}
   bool enable{false};
   bool search{false};
   operator uint16_t() {return frequency;}

   void operator() (uint16_t work_frequency)
   {
      switch (state) {
         case wait_:
            
         break;
         case manual_:
            
         break;
         case auto_:
           
         break;
         case emergency_:
         break;
      } //switch (state) 
   } //void operator() (uint16_t frequency)
};