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

# --- ribbon-ui: theme ---
#=======================================
# Theme tokens, stylesheet generation, icon recolouring, dialog and card painting
#=======================================
qt6_wrap_cpp (TestTheme_moc_sources TestTheme.hpp)
add_executable (TestTheme TestTheme.cpp ${TestTheme_moc_sources})
target_include_directories (TestTheme PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions (TestTheme PRIVATE GLABELS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries (TestTheme glabels_lib glabels_resources Model Qt6::Test Qt6::Widgets Qt6::Svg)
add_test (NAME Theme COMMAND TestTheme)
set_tests_properties (Theme PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Every restyled dialog in light and dark (set GLABELS_THEME_SCREENSHOTS=<dir> to save PNGs).
# The kiosk window and its admin dialog are compiled in from glabels-fill.
qt6_wrap_cpp (TestThemeGallery_moc_sources
  TestThemeGallery.hpp
  ${CMAKE_SOURCE_DIR}/glabels-fill/AdminDialog.hpp
  ${CMAKE_SOURCE_DIR}/glabels-fill/FillWindow.hpp
)
add_executable (TestThemeGallery
  TestThemeGallery.cpp
  ${CMAKE_SOURCE_DIR}/glabels-fill/AdminDialog.cpp
  ${CMAKE_SOURCE_DIR}/glabels-fill/FillWindow.cpp
  ${TestThemeGallery_moc_sources}
)
target_include_directories (TestThemeGallery PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_SOURCE_DIR}/glabels-fill)
target_compile_definitions (TestThemeGallery PRIVATE GLABELS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries (TestThemeGallery glabels_lib glabels_resources Model Qt6::Test Qt6::Widgets Qt6::Svg)
add_test (NAME ThemeGallery COMMAND TestThemeGallery)
set_tests_properties (ThemeGallery PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# --- end ribbon-ui: theme ---
