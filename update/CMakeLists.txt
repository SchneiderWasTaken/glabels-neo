project (Update LANGUAGES CXX)

#=======================================
# Self-update support (no GUI): release manifest, version comparison, install layout,
# download / verify / stage / apply.  The user interface lives in glabels/UpdateUi.*;
# the final directory swap is done by the separate glabels-updater helper (../updater).
#=======================================

# Auto-generated: channel, repository and identity of this build
configure_file (UpdateConfig.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/UpdateConfig.hpp @ONLY)

set (Update_sources
  InstallLayout.cpp
  UpdateManifest.cpp
  UpdateService.cpp
  UpdateStore.cpp
)

set (Update_qobject_headers
  UpdateService.hpp
)

qt6_wrap_cpp (Update_moc_sources ${Update_qobject_headers})

add_library (Update STATIC
  ${Update_sources}
  ${Update_moc_sources}
)

target_compile_features (Update
  PUBLIC cxx_std_20
)

target_include_directories (Update
  PUBLIC .. ${CMAKE_CURRENT_BINARY_DIR}/..
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries (Update
  PUBLIC
  Qt6::Core
  Qt6::Network
)

#=======================================
# Subdirectories
#=======================================
add_subdirectory (unit_tests)
