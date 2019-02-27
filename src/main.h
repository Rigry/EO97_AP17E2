#pragma once

#define ADR(reg) GET_ADR(In_regs, reg)

#include "pin.h"
#include "pwm_.h"
#include "flash.h"
#include "timers.h"
#include "modbus_slave.h"

using TX  = mcu::PA9;
using RX  = mcu::PA10;
using RTS = mcu::PA12;
using LED = mcu::PB15;// ??????
using PWM_pin = mcu::PC9;

////////////where задать начально значение коэф. заполнения//////////////////////

struct ADC_{
    uint16_t power{0};
};

struct Work {
      bool enable  :1;  // Bit 0 Start: prohibit (0), allow (1) a job; 
      uint16_t     :15; // Bits 15-1: reserved
   };

struct In_regs {
   
   UART::Settings uart_set;         // 0
   uint16_t modbus_address;         // 1
   uint16_t password;               // 2
   uint16_t factory_number;         // 3
   uint16_t frequency;              // 4
   uint16_t duty_cycle;             // 5
   Work     work;                   // 6

}__attribute__((packed));

struct Out_regs {

   uint16_t device_code;            // 0
   uint16_t factory_number;         // 1
   UART::Settings uart_set;         // 2
   uint16_t modbus_address;         // 3
   uint16_t frequency;              // 4
   uint16_t duty_cycle;             // 5

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
   uint16_t power          = 100;
};

void process()
{
   Flash<Flash_data, mcu::FLASH::Sector::_7> flash{};

   decltype(auto) modbus = Modbus_slave<In_regs, Out_regs>
                 ::make<mcu::Periph::USART1, 
                   TX, RX, RTS, LED>(flash.modbus_address, flash.uart_set);

   modbus.outRegs.device_code       = 8;// ???
   modbus.outRegs.factory_number    = flash.factory_number;
   modbus.outRegs.modbus_address    = flash.modbus_address;
   modbus.outRegs.uart_set          = flash.uart_set;
   modbus.arInRegsMax[ADR(uart_set)]= 0b11111111;
   modbus.inRegsMin.modbus_address  = 1;
   modbus.inRegsMax.modbus_address  = 255;

   decltype(auto) pwm = PWM::make<mcu::Periph::TIM3, PWM_pin>();

   ADC_ adc;

   Timer timer{1};

   while(1) {
      
      if (timer.event()) {
         pwm.frequency = modbus.inRegs.frequency;
         pwm.duty_cycle += (flash.power - adc.power) / flash.power;
      }

      modbus ([&](uint16_t registrAddress) {
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
            case ADR (frequency):
               modbus.outRegs.frequency
                  = modbus.inRegs.frequency;
            break;
            case ADR (duty_cycle):
               modbus.outRegs.duty_cycle
                  = modbus.inRegs.duty_cycle;
            break;
            case ADR(work):
               if (modbus.inRegs.work.enable) 
                  pwm.out_enable();
               else 
                  pwm.out_disable();
            break;
         }
      });
   }//while
}