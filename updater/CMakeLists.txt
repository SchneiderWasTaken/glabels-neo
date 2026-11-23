project (glabels-updater LANGUAGES CXX)

#=======================================
# glabels-updater: swaps in a staged update after the application has exited.
#
# Standard library only, no Qt: it runs from a temporary copy while the install directory
# (including Qt) is being replaced, so it must not depend on anything inside it.  It is
# installed next to the main executable and found by the application via that location.
#=======================================
add_executable (glabels-updater glabels-updater.cpp)

target_compile_features (glabels-updater PRIVATE cxx_std_20)

if (WIN32)
   # No console window flashing up; wmain keeps non-ASCII install paths intact.
   set_target_properties (glabels-updater PROPERTIES WIN32_EXECUTABLE TRUE)
   if (MSVC)
      target_link_options (glabels-updater PRIVATE /ENTRY:wmainCRTStartup)
      # Static runtime: the copy in the temp directory must not need the install's DLLs.
      set_property (TARGET glabels-updater PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
   else ()
      target_link_options (glabels-updater PRIVATE -municode -static -static-libgcc -static-libstdc++)
   endif ()
elseif (NOT APPLE)
   target_link_options (glabels-updater PRIVATE -static-libgcc -static-libstdc++)
   find_package (Threads REQUIRED)
   target_link_libraries (glabels-updater PRIVATE Threads::Threads)
endif ()

#
# Installed beside the main executable
#
if (APPLE)
   install (TARGETS glabels-updater RUNTIME DESTINATION glabels-qt.app/Contents/MacOS)
else ()
   install (TARGETS glabels-updater RUNTIME DESTINATION bin)
endif ()
