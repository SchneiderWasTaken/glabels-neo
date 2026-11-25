#=======================================
# Test the Report a Problem dialog against a mock relay
#=======================================
qt6_wrap_cpp (TestReportBugDialog_moc_sources TestReportBugDialog.hpp)
add_executable (TestReportBugDialog TestReportBugDialog.cpp ${TestReportBugDialog_moc_sources})
target_include_directories (TestReportBugDialog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries (TestReportBugDialog glabels_lib Model Qt6::Test Qt6::Widgets Qt6::Network)
add_test (NAME ReportBugDialog COMMAND TestReportBugDialog)
# Widgets are created, but nothing needs to be shown on a real screen.
set_tests_properties (ReportBugDialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

#=======================================
# Test ribbon mapping, contextual tab rules, and MainWindow command-map coverage
#=======================================
qt6_wrap_cpp (TestRibbon_moc_sources TestRibbon.hpp)
add_executable (TestRibbon TestRibbon.cpp ${TestRibbon_moc_sources})
target_include_directories (TestRibbon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries (TestRibbon glabels_lib glabels_resources Model Qt6::Test Qt6::Widgets)
add_test (NAME Ribbon COMMAND TestRibbon)
set_tests_properties (Ribbon PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
