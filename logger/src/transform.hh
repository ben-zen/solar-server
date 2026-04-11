#pragma once

#include <iostream>
#include <string>

#include <report.hh>
#include "weather.hh"

std::string convert_condition_to_filename(overall_condition condition);
int report_to_css(const status_report &report, std::ostream &out);