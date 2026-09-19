project (glabels-fill LANGUAGES CXX)

#=======================================
# Sources
#=======================================
set (glabels_fill_sources
  main.cpp
  AdminDialog.cpp
  FillWindow.cpp
)

set (glabels_fill_headers
  AdminDialog.hpp
  FillWindow.hpp
)

qt6_wrap_cpp (glabels_fill_moc_sources ${glabels_fill_headers})

#=======================================
# Static single-file option
#=======================================
option( GLABELS_FILL_STATIC "Build glabels-fill as a fully-static single-file executable" OFF )

if( GLABELS_FILL_STATIC )
    # Embed the template database as a Qt resource so the exe needs no
    # external files.
    file( GLOB FILL_TEMPLATE_FILES
        "${CMAKE_SOURCE_DIR}/templates/*.xml"
        "${CMAKE_SOURCE_DIR}/templates/*.dtd"
    )
    set( FILL_TEMPLATES_QRC "${CMAKE_CURRENT_BINARY_DIR}/templates.qrc" )
    file( WRITE ${FILL_TEMPLATES_QRC} "<!DOCTYPE RCC><RCC version=\"1.0\"><qresource prefix=\"/\">\n" )
    foreach( f ${FILL_TEMPLATE_FILES} )
        get_filename_component( fname ${f} NAME )
        file( APPEND ${FILL_TEMPLATES_QRC} "  <file alias=\"templates/${fname}\">${f}</file>\n" )
    endforeach()
    file( APPEND ${FILL_TEMPLATES_QRC} "</qresource></RCC>\n" )
    qt6_add_resources( FILL_TEMPLATES_QRC_SOURCES ${FILL_TEMPLATES_QRC} )
endif()

#=====================================
# Target
#=====================================
add_executable (glabels-fill WIN32
  ${glabels_fill_sources}
  ${glabels_fill_moc_sources}
  ${FILL_TEMPLATES_QRC_SOURCES}
)

target_compile_features (glabels-fill PRIVATE cxx_std_20)

#=====================================
# macOS app bundle
#=====================================
if (APPLE)
   target_sources (glabels-fill PRIVATE "${CMAKE_SOURCE_DIR}/glabels/icons/glabels.icns")
   set_source_files_properties ("${CMAKE_SOURCE_DIR}/glabels/icons/glabels.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
   set_target_properties (glabels-fill PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_BUNDLE_NAME "gLabels Fill"
      MACOSX_BUNDLE_GUI_IDENTIFIER "org.glabels.glabels-fill"
      MACOSX_BUNDLE_BUNDLE_VERSION "${VERSION_STRING}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION_STRING}"
      MACOSX_BUNDLE_ICON_FILE "glabels.icns"
      MACOSX_BUNDLE_INFO_STRING "gLabels Fill Kiosk"
      INSTALL_RPATH "@executable_path/../Frameworks"
   )
endif ()

if( GLABELS_FILL_STATIC )
    # Omit QXlsx (Excel) in the static build -- it's a dynamic-only dep.
    # The kiosk falls back to CSV for data sources.
    target_compile_definitions( glabels-fill PRIVATE GLABELS_FILL_NO_XLSX=1 )

    target_link_libraries (glabels-fill PRIVATE
        glabels_lib glabels_resources
        "-Wl,-Bstatic"
        zstd tiff webp webpdemux webpmux png16 jpeg freetype harfbuzz
        graphite2 md4c pcre2-16 b2 double-conversion z bz2
        brotlidec brotlicommon winpthread
        "-Wl,-Bdynamic"
    )
    target_link_options( glabels-fill PRIVATE
        -static-libgcc -static-libstdc++ -static
    )
else()
    target_link_libraries (glabels-fill
        PRIVATE glabels_lib glabels_resources
    )
endif()

#=======================================
# Install
#=======================================
if (APPLE)
   install (TARGETS glabels-fill BUNDLE DESTINATION . RUNTIME DESTINATION bin)
   install (DIRECTORY ${CMAKE_SOURCE_DIR}/templates/
            DESTINATION glabels-fill.app/Contents/Resources/templates)
   install (DIRECTORY ${CMAKE_BINARY_DIR}/translations/
            DESTINATION glabels-fill.app/Contents/Resources/translations
            FILES_MATCHING PATTERN "*.qm")
   qt6_generate_deploy_app_script (
      TARGET glabels-fill
      OUTPUT_SCRIPT glabels_fill_deploy_script
      NO_UNSUPPORTED_PLATFORM_ERROR
   )
   install (SCRIPT ${glabels_fill_deploy_script})
   install (CODE "
      set( APP_BUNDLE \"\${CMAKE_INSTALL_PREFIX}/glabels-fill.app\" )
      set( QT_LIB_DIR \"${QT_LIB_DIR}\" )
      include( \"${CMAKE_SOURCE_DIR}/cmake/MacDeployFixup.cmake\" )
   ")
else ()
   install (TARGETS glabels-fill RUNTIME DESTINATION bin)
endif ()
