project (glabelsneo LANGUAGES CXX)

#=======================================
# Sources
#=======================================
set (glabelsneo_core_sources
  Cli.cpp
  Executor.cpp
  Inputs.cpp
  JobLoader.cpp
  JobSchema.cpp
  Prompter.cpp
  Reporter.cpp
  Terminal.cpp
)

#=====================================
# Core library (the whole tool minus main(), so the unit tests can drive it)
#=====================================
add_library (GlabelsneoCore STATIC
  ${glabelsneo_core_sources}
)

target_compile_features (GlabelsneoCore
  PUBLIC cxx_std_20
)

target_include_directories (GlabelsneoCore
  PUBLIC .
)

target_link_libraries (GlabelsneoCore
  PUBLIC
  Model
  Printing
)

#=====================================
# glabelsneo: the command line tool (a console program on every platform)
#=====================================
add_executable (glabelsneo
  main.cpp
)

target_link_libraries (glabelsneo
  GlabelsneoCore
)

#=====================================
# glabels-batch-qt: the previous command line tool, kept for one release.
#
# Installed copies carry a self-updater that requires this program in every update
# package and smoke-tests a new version by running it with --Version, so it must keep
# shipping until those installs have updated past this release.  See docs/CLI-SPEC.md.
#=====================================
add_executable (glabels-batch-qt
  legacy_main.cpp
)

target_compile_features (glabels-batch-qt
  PUBLIC cxx_std_20
)

target_link_libraries (glabels-batch-qt
  Model
)

#=======================================
# Install
#=======================================
install (TARGETS glabelsneo glabels-batch-qt RUNTIME DESTINATION bin)

#=======================================
# Unit tests
#=======================================
add_subdirectory (unit_tests)
