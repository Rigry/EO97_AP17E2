#pragma once

#include "screen_common.h"
#include <array>
#include <bitset>

constexpr auto info = std::array {
    "А",
    "П"  
};

constexpr std::string_view info_to_string(int i) {
    return info[i];
}

constexpr auto mode = std::array {
    "Автоматический",
    "Пользовательский"  
};

constexpr std::string_view mode_to_string(int i) {
    return mode[i];
}

constexpr auto tune = std::array {
    "Автоматическая",
    "Пользовательская"  
};

constexpr std::string_view tune_to_string(int i) {
    return tune[i];
}

constexpr auto tune_control = std::array {
    "Начать",
    "Закончить"  
};

constexpr std::string_view tune_control_to_string(int i) {
    return tune_control[i];
}

constexpr auto search = std::array {
    "да",
    "нет"  
};

constexpr std::string_view search_to_string(int i) {
    return search[i];
}

// template <class function>
struct Main_screen : Screen {
   String_buffer& lcd;
   // Eventer enter_event;
   Buttons_events eventers;
   Callback<> out_callback;
   uint16_t& temperatura;
   uint16_t& duty_cycle;
   uint16_t& frequency;
   uint16_t& current;
   bool& mode;
   bool& tune;
   bool& on;

   Main_screen(
      String_buffer& lcd
      // , Enter_event enter_event
      , Buttons_events eventers
      , Out_callback out_callback
      , uint16_t& temperatura
      , uint16_t& duty_cycle
      , uint16_t& frequency
      , uint16_t& current
      , bool& mode
      , bool& tune
      , bool& on
      
   ) : lcd          {lcd}
   //   , enter_event  {enter_event.value}
     , eventers     {eventers}
     , out_callback {out_callback.value}
     , temperatura  {temperatura}
     , duty_cycle   {duty_cycle}
     , frequency    {frequency}
     , current      {current}
     , mode         {mode}
     , tune         {tune}
     , on           {on}
   {}

   void init() override {
      // enter_event ([this]{ out_callback(); });
      eventers.enter ([this]{ on ^= 1;});
      eventers.up    ([this]{ ;  });
      eventers.down  ([this]{ ;  });
      eventers.out   ([this]{ out_callback(); });
      lcd.clear();
      lcd.line(0) << "F=";
      lcd.line(0).cursor(11) << "P=";
      lcd.line(1) << "I=";
      lcd.line(1).cursor(10).width(2) << temperatura << "C";;
      lcd.line(1).cursor(14) << ::info[this->tune];
      lcd.line(1).cursor(15) << ::info[this->mode];
   }

   void deinit() override {
      // enter_event (nullptr);
      eventers.up    (nullptr);
      eventers.down  (nullptr);
      eventers.out   (nullptr);
   }

   void draw() override {
      lcd.line(0).cursor(2).div_1000(frequency) << "кГц";
      lcd.line(0).cursor(13).width(2) << (duty_cycle / 5) << '%';
      lcd.line(1).cursor(2).div_1000(current) << "А";
      lcd.line(1).cursor(10).width(2) << temperatura << "C";
      // lcd.line(1).cursor(8) << duty_cycle << "%";
   }

};

struct Temp_show_screen : Screen {
   String_buffer& lcd;
   Buttons_events eventers;
   Callback<> out_callback;
   const std::string_view name;
   const std::string_view unit;
   const uint16_t& temperatura;

   Temp_show_screen (
         String_buffer& lcd
       , Buttons_events eventers
       , Out_callback out_callback
       , std::string_view name
       , std::string_view unit
       , uint16_t& temperatura
   ) : lcd          {lcd}
     , eventers     {eventers}
     , out_callback {out_callback.value}
     , name {name}
     , unit {unit}
     , temperatura  {temperatura}
   {}

   void init() override {
      eventers.up    ([this]{ });
      eventers.down  ([this]{ });
      eventers.out   ([this]{ out_callback(); });
      lcd.line(0) << name << next_line;
      redraw();
   }
   void deinit() override {
      eventers.up    (nullptr);
      eventers.down  (nullptr);
      eventers.out   (nullptr);
   }

   void draw() override {}

   

private:

   void redraw() {
      lcd.line(1) << temperatura << unit;
      lcd << clear_after;
   }
};

