#pragma once

#include "string_buffer.h"
#include "hd44780.h"
#include "select_screen.h"
#include "set_screen.h"
#include "screens.h"
#include "button.h"

template<class Pins, class Flash_data, class Modbus_regs, size_t qty_lines = 4, size_t size_line = 20>
struct Menu : TickSubscriber {
   String_buffer lcd {};
   HD44780& hd44780 {HD44780::make(Pins{}, lcd.get_buffer())};
   Button_event& up;
   Button_event& down;
   Button_event& enter;
   Flash_data&   flash;
   Modbus_regs&  modbus;

   Screen* current_screen {&main_screen};
   size_t tick_count{0};

   Buttons_events buttons_events {
        Up_event    {[this](auto c){   up.set_click_callback(c);}}
      , Down_event  {[this](auto c){ down.set_click_callback(c);}}
      , Enter_event {[this](auto c){enter.set_click_callback(c);}}
      , Out_event   {[this](auto c){enter.set_long_push_callback(c);}}
      , Increment_up_event   {[this](auto c){  up.set_increment_callback(c);}}
      , Increment_down_event {[this](auto c){down.set_increment_callback(c);}}
   };

   Menu (
        Pins pins
      , Button_event& up
      , Button_event& down
      , Button_event& enter
      , Flash_data&   flash
      , Modbus_regs&  modbus
   ) : up{up}, down{down}, enter{enter}
      , flash{flash}, modbus{modbus}
   {
      tick_subscribe();
      current_screen->init();
      while(not hd44780.init_done()){}
   }

   Main_screen main_screen {
          lcd
        , Enter_event  { [this](auto c){enter.set_click_callback(c);}}
        , Out_callback { [this]{ change_screen(main_select); }}
        , modbus.temperatura
        , modbus.frequency
        , modbus.current
        , flash.mode
   };

    Select_screen<4> main_select {
          lcd, buttons_events
        , Out_callback       { [this]{change_screen(main_screen);  }}
        , Line {"Режим работы",[this]{change_screen(mode_set);     }}
        , Line {"Аварии"      ,[this]{change_screen(alarm_select); }}
        , Line {"Наработка"   ,[this]{change_screen(work_select);  }}
        , Line {"Конфигурация",[this]{change_screen(config_select);}}
   };

   // Select_screen<2> mode_select {
   //        lcd, buttons_events
   //      , Out_callback    { [this]{ change_screen(main_select);  }}
   //      , Line {"Автоматический",[]{}}
   //      , Line {"Ручной"        ,[]{}}
   // };

   Select_screen<3> alarm_select {
          lcd, buttons_events
        , Out_callback    { [this]{ change_screen(main_select);  }}
        , Line {"Ошибки работы",[]{}}
        , Line {"Ошибки RS485" ,[]{}}
        , Line {"Сброс ошибок" ,[]{}}
   };

   Select_screen<2> work_select {
          lcd, buttons_events
        , Out_callback             { [this]{ change_screen(main_select);  }}
        , Line {"Наработка",[]{}}
        , Line {"Сброс наработки   ",[]{}}
   };

   Select_screen<3> config_select {
          lcd, buttons_events
        , Out_callback             { [this]{change_screen(main_select);     }}
        , Line {"Девиация" ,[]{}}
        , Line {"Booster" ,[]{}}
        , Line {"Дегазация" ,[]{}}
   };

   uint8_t mode_ {flash.uart_set.parity_enable};
   Set_screen<uint8_t, mode_to_string> mode_set {
        lcd, buttons_events
      , "Выбор режима"
      , mode_
      , Min<uint8_t>{0}, Max<uint8_t>{mode.size() - 1}
      , Out_callback    { [this]{ change_screen(main_select); }}
      , Enter_callback  { [this]{ 
         flash.mode = bool(mode_);
            change_screen(main_select);
      }}
    };

   void notify() override {
      every_qty_cnt_call(tick_count, 50, [this]{
         current_screen->draw();
      });
   }

   void change_screen(Screen& new_screen) {
      current_screen->deinit();
      current_screen = &new_screen;
      current_screen->init();
   }

};