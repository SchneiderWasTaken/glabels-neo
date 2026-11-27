project (Printing LANGUAGES CXX)

#=======================================
# Sources
#=======================================
set (printing_sources
  PrintService.cpp
  ZplPrinter.cpp
)

#=====================================
# Target
#=====================================
# Printer resolution, PDF output and Zebra/ZPL transport with no user-interface
# code, shared by the glabelsneo command line tool and the GUI.
add_library (Printing STATIC
  ${printing_sources}
)

target_compile_features (Printing
  PUBLIC cxx_std_20
)

target_include_directories (Printing
  PUBLIC ..
)

target_link_libraries (Printing
  PUBLIC
  Model
  Qt6::Network
  Qt6::PrintSupport
)

if (WIN32)
  target_link_libraries (Printing PUBLIC winspool)
endif ()
