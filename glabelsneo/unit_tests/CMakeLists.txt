if (Qt6Test_FOUND)

  # Shared by every test here: the core library of the tool, the test data, and Qt Test.
  function (glabelsneo_test name)
    qt6_wrap_cpp (${name}_moc_sources ${name}.hpp)
    add_executable (${name} ${name}.cpp ${${name}_moc_sources})
    target_link_libraries (${name} GlabelsneoCore Qt6::Test)
    target_compile_definitions (${name} PRIVATE
      TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
      GLABELSNEO_EXE="$<TARGET_FILE:glabelsneo>"
      GLABELS_BATCH_EXE="$<TARGET_FILE:glabels-batch-qt>")
    add_dependencies (${name} glabelsneo glabels-batch-qt)
  endfunction ()

  #=======================================
  # JSON/in-memory merge backends and record parsing
  #=======================================
  glabelsneo_test (TestRecords)
  add_test (NAME GlabelsneoRecords COMMAND TestRecords)

  #=======================================
  # Job documents, field scan, precedence
  #=======================================
  glabelsneo_test (TestJobs)
  add_test (NAME GlabelsneoJobs COMMAND TestJobs)

  #=======================================
  # Printing layer: ZPL, printer resolution, PDF/PNG output
  #=======================================
  glabelsneo_test (TestPrinting)
  add_test (NAME GlabelsneoPrinting COMMAND TestPrinting)

  #=======================================
  # The tool end to end: exit codes, JSON interface, prompts, the updater's --Version contract
  #=======================================
  glabelsneo_test (TestCli)
  add_test (NAME GlabelsneoCli COMMAND TestCli)

endif ()
