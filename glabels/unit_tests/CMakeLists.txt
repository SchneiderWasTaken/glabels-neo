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
