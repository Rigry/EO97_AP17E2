#define BOOST_TEST_MODULE test
#include <boost/test/unit_test.hpp>

#define F_CPU   72000000UL
#define STM32F405xx
#define TEST

#include <iostream>
#include <sstream>
#include <type_traits>
#include "mock_systick.h"
#include "mock_pwm.h"
#include "main.h"

BOOST_AUTO_TEST_SUITE (test_suite_main)

BOOST_AUTO_TEST_CASE(make)
{
   process();
}

BOOST_AUTO_TEST_SUITE_END()

