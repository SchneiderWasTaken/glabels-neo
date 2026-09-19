project (glabels LANGUAGES CXX)

#=======================================
# Sources
#=======================================
set (glabels_sources
  AboutDialog.cpp
  BarcodeMenu.cpp
  BarcodeMenuButton.cpp
  BarcodeMenuItem.cpp
  ColorButton.cpp
  ColorHistory.cpp
  ColorPaletteDialog.cpp
  ColorPaletteItem.cpp
  ColorSwatch.cpp
  Cursors.cpp
  DeployView.cpp
  EditVariableDialog.cpp
  FieldButton.cpp
  File.cpp
  FillConfig.cpp
  FillJobModel.cpp
  FillMerge.cpp
  FillView.cpp
  Help.cpp
  LabelEditor.cpp
  MainWindow.cpp
  MergeTableModel.cpp
  MergeView.cpp
  MiniPreviewPixmap.cpp
  NotebookUtil.cpp
  ObjectEditor.cpp
  PreferencesDialog.cpp
  PrinterMonitor.cpp
  PrintView.cpp
  PropertiesView.cpp
  Preview.cpp
  PreviewOverlayItem.cpp
  ReportBugDialog.cpp
  RollTemplatePath.cpp
  SelectProductDialog.cpp
  SimplePreview.cpp
  StartupView.cpp
  TemplateDesigner.cpp
  TemplatePicker.cpp
  TemplatePickerItem.cpp
  UndoRedoModel.cpp
  VariablesView.cpp
  ZplRenderer.cpp
)

set (glabels_qobject_headers
  AboutDialog.hpp
  BarcodeMenu.hpp
  BarcodeMenuButton.hpp
  BarcodeMenuItem.hpp
  ColorButton.hpp
  ColorHistory.hpp
  ColorPaletteDialog.hpp
  ColorPaletteItem.hpp
  DeployView.hpp
  EditVariableDialog.hpp
  FieldButton.hpp
  File.hpp
  FillConfig.hpp
  FillJobModel.hpp
  FillMerge.hpp
  FillView.hpp
  LabelEditor.hpp
  MainWindow.hpp
  MergeTableModel.hpp
  MergeView.hpp
  ObjectEditor.hpp
  PreferencesDialog.hpp
  PrinterMonitor.hpp
  PrintView.hpp
  PropertiesView.hpp
  Preview.hpp
  ReportBugDialog.hpp
  SelectProductDialog.hpp
  SimplePreview.hpp
  StartupView.hpp
  TemplateDesigner.hpp
  TemplatePicker.hpp
  UndoRedoModel.hpp
  VariablesView.hpp
  ZplRenderer.hpp
)

set (glabels_forms
  ui/AboutDialog.ui
  ui/EditVariableDialog.ui
  ui/FillView.ui
  ui/MergeView.ui
  ui/ObjectEditor.ui
  ui/PreferencesDialog.ui
  ui/PrintView.ui
  ui/PropertiesView.ui
  ui/ReportBugDialog.ui
  ui/SelectProductDialog.ui
  ui/StartupView.ui
  ui/TemplateDesignerIntroPage.ui
  ui/TemplateDesignerNamePage.ui
  ui/TemplateDesignerPageSizePage.ui
  ui/TemplateDesignerShapePage.ui
  ui/TemplateDesignerRectPage.ui
  ui/TemplateDesignerRoundPage.ui
  ui/TemplateDesignerEllipsePage.ui
  ui/TemplateDesignerCdPage.ui
  ui/TemplateDesignerPathPage.ui
  ui/TemplateDesignerContinuousPage.ui
  ui/TemplateDesignerNLayoutsPage.ui
  ui/TemplateDesignerOneLayoutPage.ui
  ui/TemplateDesignerTwoLayoutPage.ui
  ui/TemplateDesignerApplyPage.ui
  ui/TemplateDesignerThermalPage.ui
  ui/VariablesView.ui
)

set (glabels_resource_files
  cursors.qrc
  icons.qrc
  images.qrc
)

qt6_wrap_cpp (glabels_moc_sources ${glabels_qobject_headers})
qt6_wrap_ui (glabels_forms_headers ${glabels_forms})
qt6_add_resources (glabels_qrc_sources ${glabels_resource_files})

if (WIN32)
  # Windows resource file
  set (glabels_win_rc glabels.rc)
endif ()

#=====================================
# Resources (OBJECT library -- compiled once, object files directly
# included in each exe that links it, so resource registrations are
# never stripped by the linker).
#=====================================
add_library (glabels_resources OBJECT ${glabels_qrc_sources})
target_compile_features (glabels_resources PRIVATE cxx_std_20)

#=====================================
# Shared library (used by glabels-qt and the glabels-fill kiosk)
#=====================================
add_library (glabels_lib STATIC
  ${glabels_sources}
  ${glabels_moc_sources}
  ${glabels_forms_headers}
)

target_compile_features (glabels_lib
  PUBLIC cxx_std_20
)

target_include_directories (glabels_lib
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_CURRENT_SOURCE_DIR}/qxlsx_compat
)

if( GLABELS_USE_XLSX )
    # QXlsx may come from a system package (QXlsx::QXlsx) or from
    # FetchContent in the top-level CMakeLists (plain QXlsx target).
    if( TARGET QXlsx::QXlsx )
        target_link_libraries (glabels_lib PUBLIC QXlsx::QXlsx)
    elseif( TARGET QXlsx )
        target_link_libraries (glabels_lib PUBLIC QXlsx)
    else()
        target_compile_definitions( glabels_lib PUBLIC GLABELS_FILL_NO_XLSX=1 )
    endif()
else()
    target_compile_definitions( glabels_lib PUBLIC GLABELS_FILL_NO_XLSX=1 )
endif()

target_link_libraries (glabels_lib
  PUBLIC
  Model
  Qt6::Concurrent
  Qt6::Widgets
  Qt6::Network
)

if(WIN32)
  target_link_libraries(glabels_lib PUBLIC winspool)
endif()

#=====================================
# glabels-qt executable (full designer)
#=====================================
add_executable (glabels-qt WIN32
  main.cpp
  ${glabels_win_rc}
)

#=====================================
# macOS app bundle
#=====================================
if (APPLE)
   target_sources (glabels-qt PRIVATE icons/glabels.icns)
   set_source_files_properties (icons/glabels.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
   set_target_properties (glabels-qt PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_BUNDLE_NAME "gLabels"
      MACOSX_BUNDLE_GUI_IDENTIFIER "org.glabels.glabels-qt"
      MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
      MACOSX_BUNDLE_ICON_FILE "glabels.icns"
      MACOSX_BUNDLE_INFO_STRING "gLabels Label Designer"
      MACOSX_BUNDLE_COPYRIGHT "GPLv3"
      INSTALL_RPATH "@executable_path/../Frameworks"
   )
endif ()

# Resources must be linked DIRECTLY to the executable (OBJECT libraries
# do not propagate transitively through STATIC libraries).
target_link_libraries (glabels-qt
  PRIVATE glabels_lib glabels_resources
)

#=======================================
# Install
#=======================================
if (APPLE)
   install (TARGETS glabels-qt BUNDLE DESTINATION . RUNTIME DESTINATION bin)
   # Bundle data so the .app is fully self-contained
   install (DIRECTORY ${CMAKE_SOURCE_DIR}/templates/
            DESTINATION glabels-qt.app/Contents/Resources/templates)
   install (DIRECTORY ${CMAKE_BINARY_DIR}/translations/
            DESTINATION glabels-qt.app/Contents/Resources/translations
            FILES_MATCHING PATTERN "*.qm")
   # Deploy Qt frameworks/plugins into the installed bundle so it is
   # self-contained and distributable.
   qt6_generate_deploy_app_script (
      TARGET glabels-qt
      OUTPUT_SCRIPT glabels_deploy_script
      NO_UNSUPPORTED_PLATFORM_ERROR
   )
   install (SCRIPT ${glabels_deploy_script})
   # Homebrew's macdeployqt misses transitive frameworks (e.g. QtDBus) and
   # breaks the signature; finish the bundle ourselves.
   install (CODE "
      set( APP_BUNDLE \"\${CMAKE_INSTALL_PREFIX}/glabels-qt.app\" )
      set( QT_LIB_DIR \"${QT_LIB_DIR}\" )
      include( \"${CMAKE_SOURCE_DIR}/cmake/MacDeployFixup.cmake\" )
   ")
else ()
   install (TARGETS glabels-qt RUNTIME DESTINATION bin)
endif ()

install (FILES icons/glabels-flat/scalable/apps/glabels.svg DESTINATION share/icons/hicolor/scalable/apps)
install (FILES icons/glabels-flat/16x16/apps/glabels.svg    DESTINATION share/icons/hicolor/16x16/apps)
install (FILES icons/glabels-flat/22x22/apps/glabels.svg    DESTINATION share/icons/hicolor/22x22/apps)
install (FILES icons/glabels-flat/32x32/apps/glabels.svg    DESTINATION share/icons/hicolor/32x32/apps)
install (FILES icons/glabels-flat/48x48/apps/glabels.svg    DESTINATION share/icons/hicolor/48x48/apps)

install (FILES icons/mimetypes/scalable/x-glabels-project.svg DESTINATION share/icons/hicolor/scalable/mimetypes)
install (FILES icons/mimetypes/16x16/x-glabels-project.svg    DESTINATION share/icons/hicolor/16x16/mimetypes)
install (FILES icons/mimetypes/22x22/x-glabels-project.svg    DESTINATION share/icons/hicolor/22x22/mimetypes)
install (FILES icons/mimetypes/24x24/x-glabels-project.svg    DESTINATION share/icons/hicolor/24x24/mimetypes)

#
# Windows Runtime
#
if (WIN32)

    find_program (WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    #
    # Visual Studio toolchain
    #
    if (MSVC)
        # Run windeployqt immediately after build
        add_custom_command (TARGET glabels-qt POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E
                env PATH="${QT_BIN_DIR}" "${WINDEPLOYQT_EXECUTABLE}"
                    --verbose 0
                    --no-compiler-runtime
                    --no-opengl-sw
                    \"$<TARGET_FILE:glabels-qt>\"
        )

      # Install files staged by windeployqt
        install (DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/ DESTINATION bin
                 FILES_MATCHING PATTERN *.dll PATTERN *.qm)

        # Install necessary system libraries
        set (CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
        include (InstallRequiredSystemLibraries)
    endif (MSVC)

    #
    # MSYS/MINGW toolchain
    #
    if (MINGW)
        # Run windeployqt immediately after build
        add_custom_command (TARGET glabels-qt POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E
                env PATH="${QT_BIN_DIR}" "${WINDEPLOYQT_EXECUTABLE}"
                    --verbose 0
                    --release
                    --no-compiler-runtime
                    --no-opengl-sw
                    \"$<TARGET_FILE:glabels-qt>\"
        )

        # Install files staged by windeployqt
        install (DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/ DESTINATION bin
                 FILES_MATCHING PATTERN *.dll PATTERN *.qm)

        # Install necessary system libraries
        install (FILES
            ${QT_BIN_DIR}/libgcc_s_dw2-1.dll
            ${QT_BIN_DIR}/libstdc++-6.dll
            ${QT_BIN_DIR}/libwinpthread-1.dll
            DESTINATION bin
        )
        if (ZLIB_FOUND)
          install (FILES ${MINGW_BIN_DIR}/zlib1.dll DESTINATION bin)
        endif ()
    endif (MINGW)

endif (WIN32)
