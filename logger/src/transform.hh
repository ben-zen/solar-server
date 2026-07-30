#pragma once

#include <filesystem>
#include <iostream>

#include <report.hh>

int report_to_css(const status_report &report, std::ostream &out);
int report_to_css(const status_report &report, const std::filesystem::path &destination);