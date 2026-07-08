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
install (TARGETS glabels-fill RUNTIME DESTINATION bin)
