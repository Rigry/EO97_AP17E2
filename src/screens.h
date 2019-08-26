#pragma once

#include "screen_common.h"
#include <array>
#include <bitset>

constexpr auto mode = std::array {
    "авто  ",
    "ручной"  
};

constexpr std::string_view mode_to_string(int i) {
    return mode[i];
}

struct Main_screen : Screen {
   String_buffer& lcd;
   Eventer enter_event;
   Callback<> out_callback;
   uint16_t& temperatura;
   uint16_t& frequency;
   uint16_t& current;
   bool& mode;
    
   // int _ {0}; // без этой фигни оптимизатор чудил

   Main_screen(
      String_buffer& lcd
      , Enter_event enter_event
      , Out_callback out_callback
      , uint16_t& temperatura
      , uint16_t& frequency
      , uint16_t& current
      , bool& mode
      
   ) : lcd          {lcd}
     , enter_event  {enter_event.value}
     , out_callback {out_callback.value}
     , temperatura  {temperatura}
     , frequency    {frequency}
     , current      {current}
     , mode         {mode}
   {}

   void init() override {
      enter_event ([this]{ out_callback(); });
      lcd.clear();
      lcd.line(0) << "F=" << frequency;
      lcd.line(0).cursor(10) << "t=" << temperatura;
      lcd.line(1) << "I=" << current;
      lcd.line(1).cursor(10) << ::mode[this->mode];
   }

   void deinit() override {
      enter_event (nullptr);
   }

   void draw() override {
      lcd.line(0).cursor(2)  << frequency   << "Гц";
      lcd.line(0).cursor(12).width(2) << temperatura << 'C';
      lcd.line(1).cursor(2)  << current     << "А";
   }

};