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

# Popup audit (docs/ribbon-ui/POPUP-AUDIT.md): rows shown and corner pixels of every popup kind
qt6_wrap_cpp (TestThemePopups_moc_sources TestThemePopups.hpp)
add_executable (TestThemePopups TestThemePopups.cpp ${TestThemePopups_moc_sources})
target_include_directories (TestThemePopups PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries (TestThemePopups glabels_lib glabels_resources Model Qt6::Test Qt6::Widgets Qt6::Svg)
add_test (NAME ThemePopups COMMAND TestThemePopups)
set_tests_properties (ThemePopups PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Product Template Designer in the new design: step list logic, side bar, illustrations, every page
qt6_wrap_cpp (TestThemeDesigner_moc_sources TestThemeDesigner.hpp)
add_executable (TestThemeDesigner TestThemeDesigner.cpp ${TestThemeDesigner_moc_sources})
target_include_directories (TestThemeDesigner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries (TestThemeDesigner glabels_lib glabels_resources Model Qt6::Test Qt6::Widgets Qt6::Svg)
add_test (NAME ThemeDesigner COMMAND TestThemeDesigner)
set_tests_properties (ThemeDesigner PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# --- end ribbon-ui: theme ---

# --- ribbon-ui: core ---
#=======================================
# Test ribbon mapping, contextual tab rules, and MainWindow command-map coverage
#=======================================
qt6_wrap_cpp (TestRibbon_moc_sources TestRibbon.hpp)
add_executable (TestRibbon TestRibbon.cpp ${TestRibbon_moc_sources})
target_include_directories (TestRibbon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries (TestRibbon glabels_lib glabels_resources Model Qt6::Test Qt6::Widgets)
add_test (NAME Ribbon COMMAND TestRibbon)
set_tests_properties (Ribbon PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

#=======================================
# Test snapping engine, guides, View > Snap group, and LabelEditor mouse snapping
#=======================================
qt6_wrap_cpp (TestSnap_moc_sources TestSnap.hpp)
add_executable (TestSnap TestSnap.cpp ${TestSnap_moc_sources})
target_include_directories (TestSnap PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries (TestSnap glabels_lib glabels_resources Model Qt6::Test Qt6::Widgets)
add_test (NAME Snap COMMAND TestSnap)
set_tests_properties (Snap PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# --- end ribbon-ui: core ---

#=======================================
# Test Redash external data source & Fill integration against a mock server
#=======================================
qt6_wrap_cpp (TestRedashDataSource_moc_sources TestRedashDataSource.hpp)
add_executable (TestRedashDataSource TestRedashDataSource.cpp ${TestRedashDataSource_moc_sources})
target_include_directories (TestRedashDataSource PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries (TestRedashDataSource glabels_lib glabels_resources Model Qt6::Test Qt6::Widgets Qt6::Network)
add_test (NAME RedashDataSource COMMAND TestRedashDataSource)
set_tests_properties (RedashDataSource PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

#=======================================
# Test Google Sheets OAuth, Picker, Sheets API v4 & Public CSV against a mock server
#=======================================
qt6_wrap_cpp (TestGoogleSheetsDataSource_moc_sources TestGoogleSheetsDataSource.hpp)
add_executable (TestGoogleSheetsDataSource TestGoogleSheetsDataSource.cpp ${TestGoogleSheetsDataSource_moc_sources})
target_include_directories (TestGoogleSheetsDataSource PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries (TestGoogleSheetsDataSource glabels_lib glabels_resources Model Qt6::Test Qt6::Widgets Qt6::Network)
add_test (NAME GoogleSheetsDataSource COMMAND TestGoogleSheetsDataSource)
set_tests_properties (GoogleSheetsDataSource PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Pictures for the README and the website: skipped unless GLABELS_MARKETING_SHOTS=<dir> is set.
qt6_wrap_cpp (TestMarketingShots_moc_sources TestMarketingShots.hpp)
add_executable (TestMarketingShots TestMarketingShots.cpp ${TestMarketingShots_moc_sources})
target_include_directories (TestMarketingShots PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions (TestMarketingShots PRIVATE GLABELS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries (TestMarketingShots glabels_lib glabels_resources Model Qt6::Test Qt6::Widgets Qt6::Svg)
add_test (NAME MarketingShots COMMAND TestMarketingShots)
set_tests_properties (MarketingShots PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
