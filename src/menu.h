#pragma once

#include "string_buffer.h"
#include "hd44780.h"
#include "select_screen.h"
#include "set_screen.h"
#include "screens.h"
#include "button.h"
// #include "tune_up.h"
#include "generator.h"

template<class Pins, class Flash_data, class Modbus_regs, size_t qty_lines = 4, size_t size_line = 20>
struct Menu : TickSubscriber {
   String_buffer lcd {};
   HD44780& hd44780 {HD44780::make(Pins{}, lcd.get_buffer())};
   Button_event& up;
   Button_event& down;
   Button_event& enter;
   Flash_data&   flash;
   Modbus_regs&  modbus;
   Mode&         mode;

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
      , Mode&         mode
   ) : up{up}, down{down}, enter{enter}
      , flash{flash}, modbus{modbus}, mode{mode}
   {
      tick_subscribe();
      current_screen->init();
      while(not hd44780.init_done()){}
   }

   Main_screen main_screen {
          lcd, buttons_events
      //   , Enter_event  { [this](auto c){enter.set_click_callback(c);}}
        , Out_callback { [this]{ change_screen(main_select); }}
        , modbus.temperatura
        , modbus.duty_cycle
        , modbus.frequency
        , modbus.current
        , mode.overheat
        , flash.m_control
        , flash.m_search
        , mode.on
   };

    Select_screen<5> main_select {
          lcd, buttons_events
        , Out_callback          { [this]{change_screen(main_screen);  }}
        , Line {"Параметры"      ,[this]{change_screen(option_select);}}
        , Line {"Настройка"      ,[this]{ if (flash.search)
                                             change_screen(select_tune_end);
                                          else
                                             change_screen(select_tune_begin);  }}
        , Line {"Режим работы"   ,[this]{change_screen(mode_set);     }}
        , Line {"Конфигурация"   ,[this]{change_screen(config_select);}}
        , Line {"Аварии"         ,[this]{change_screen(alarm_select); }}
   };

   Select_screen<3> option_select {
          lcd, buttons_events
        , Out_callback    {       [this]{ change_screen(main_select);    }}
        , Line {"Mощность"       ,[this]{ change_screen(duty_cycle_set); }}
        , Line {"Рабочая частота",[this]{ change_screen(frequency_set);  }}
        , Line {"Температура"    ,[this]{ change_screen(temp_select);    }}
   };

   uint8_t mode_ {flash.m_control};
   Set_screen<uint8_t, mode_to_string> mode_set {
        lcd, buttons_events
      , "Выбор режима"
      , ""
      , mode_
      , Min<uint8_t>{0}, Max<uint8_t>{::mode.size() - 1}
      , Out_callback    { [this]{ change_screen(main_select); }}
      , Enter_callback  { [this]{ 
         flash.m_control = mode_;
            change_screen(main_select);
      }}
   };

   Select_screen<2> select_tune_begin {
          lcd, buttons_events
        , Out_callback    {        [this]{ change_screen(main_select);    }}
        , Line {"Режим настройки" ,[this]{ change_screen(tune_set);       }}
        , Line {"Начать"          ,[this]{ flash.search = true;
                                           change_screen(main_screen);    }}
   };

   Select_screen<2> select_tune_end {
          lcd, buttons_events
        , Out_callback    {        [this]{ change_screen(main_select);    }}
        , Line {"Режим настройки" ,[this]{ change_screen(tune_set);       }}
        , Line {"Закончить"       ,[this]{ flash.search = false;
                                           change_screen(main_screen);    }}
   };
   
   uint8_t tune_ {flash.m_search};
   Set_screen<uint8_t, tune_to_string> tune_set {
        lcd, buttons_events
      , "Настройка"
      , ""
      , tune_
      , Min<uint8_t>{0}, Max<uint8_t>{::tune.size() - 1}
      , Out_callback    { [this]{ if (flash.search)
                                    change_screen(select_tune_end);
                                  else
                                    change_screen(select_tune_begin); }}
      , Enter_callback  { [this]{ 
         flash.m_search = tune_;
            if (flash.search)
               change_screen(select_tune_end);
            else
               change_screen(select_tune_begin);
      }}
   };
   
   uint8_t power{flash.power};
   Set_screen<uint8_t> duty_cycle_set {
        lcd, buttons_events
      , "Мощность от ном."
      , " %"
      , power
      , Min<uint8_t>{0_percent}, Max<uint8_t>{100_percent}
      , Out_callback    { [this]{ change_screen(option_select); }}
      , Enter_callback  { [this]{ 
         flash.power = power;
            change_screen(option_select); }}
   };

   uint16_t work_frequency{flash.work_frequency};
   Set_screen<uint16_t> frequency_set {
        lcd, buttons_events
      , "Рабочая частота"
      , " Гц"
      , work_frequency
      , Min<uint16_t>{18_kHz}, Max<uint16_t>{45_kHz}
      , Out_callback    { [this]{ change_screen(option_select); }}
      , Enter_callback  { [this]{ 
         flash.work_frequency = work_frequency;
            change_screen(option_select); }}
   };

   Select_screen<3> temp_select {
          lcd, buttons_events
        , Out_callback    {      [this]{ change_screen(main_select);     }}
        , Line {"Текущая"       ,[this]{ change_screen(temp_show);  }}
        , Line {"Максимальная"  ,[this]{ change_screen(temp_set);;  }}
        , Line {"Восстановления",[this]{ change_screen(temp_recow);;  }}
   };

   Temp_show_screen temp_show {
        lcd, buttons_events
      , Out_callback  { [this]{ change_screen(temp_select); }}
      , "Текущая темпер."
      , " С"
      , modbus.temperatura
   };
   
   uint8_t temperatura{flash.temperatura};
   Set_screen<uint8_t> temp_set {
        lcd, buttons_events
      , "Температ. откл."
      , " С"
      , temperatura
      , Min<uint8_t>{0}, Max<uint8_t>{100}
      , Out_callback    { [this]{ change_screen(temp_select); }}
      , Enter_callback  { [this]{ 
         flash.temperatura = temperatura;
            change_screen(temp_select); }}
   };

   uint8_t recovery{flash.recovery};
   Set_screen<uint8_t> temp_recow {
        lcd, buttons_events
      , "Температ. вкл."
      , " С"
      , recovery
      , Min<uint8_t>{0}, Max<uint8_t>{100}
      , Out_callback    { [this]{ change_screen(temp_select); }}
      , Enter_callback  { [this]{ 
         flash.recovery = recovery;
            change_screen(temp_select); }}
   };

   // uint8_t tune_{flash.m_search};
   // Set_screen<uint8_t,tune_to_string> tune_select {
   //      lcd, buttons_events
   //    , "Настройка"
   //    , ""
   //    , tune_
   //    , Min<uint8_t>{0}, Max<uint8_t>{::tune.size() - 1}
   //    , Out_callback    { [this]{ change_screen(mode_set); }}
   //    , Enter_callback  { [this]{ 
   //       flash.m_search = tune_;
   //       flash.m_search = true;
   //          change_screen(main_select); }}
   // };

   // uint8_t search_ {flash.search};
   // Set_screen<uint8_t, search_to_string> end_tune {
   //      lcd, buttons_events
   //    , "Закончить нас-ку"
   //    , ""
   //    , search_
   //    , Min<uint8_t>{0}, Max<uint8_t>{::search.size() - 1}
   //    , Out_callback    { [this]{ change_screen(main_select); }}
   //    , Enter_callback  { [this]{ 
   //       flash.search = search_;
   //          change_screen(main_select);
   //    }}
   // };

   Select_screen<3> alarm_select {
          lcd, buttons_events
        , Out_callback    { [this]{ change_screen(main_select);  }}
        , Line {"Раздел в",[]{}}
        , Line {"разработке" ,[]{}}

   };

   Select_screen<2> config_select {
          lcd, buttons_events
        , Out_callback             { [this]{change_screen(main_select);     }}
        , Line {"Девиация" ,[]{}}
        , Line {"Booster" ,[]{}}
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