if (Qt6Test_FOUND)

  #=======================================
  # Test UpdateManifest: parsing, validation, "is there something newer"
  #=======================================
  qt6_wrap_cpp (TestUpdateManifest_moc_sources TestUpdateManifest.hpp)
  add_executable (TestUpdateManifest TestUpdateManifest.cpp ${TestUpdateManifest_moc_sources})
  target_link_libraries (TestUpdateManifest Update Qt6::Test)
  target_compile_definitions (TestUpdateManifest PRIVATE MAKE_MANIFEST_SCRIPT="${CMAKE_SOURCE_DIR}/scripts/make-update-manifest.sh")
  add_test (NAME UpdateManifest COMMAND TestUpdateManifest)

  #=======================================
  # Test InstallLayout: what gets replaced, where, and when updating is refused
  #=======================================
  qt6_wrap_cpp (TestInstallLayout_moc_sources TestInstallLayout.hpp)
  add_executable (TestInstallLayout TestInstallLayout.cpp ${TestInstallLayout_moc_sources})
  target_link_libraries (TestInstallLayout Update Qt6::Test)
  add_test (NAME InstallLayout COMMAND TestInstallLayout)

  #=======================================
  # Test the glabels-updater helper end to end (real binary, real directories)
  #=======================================
  qt6_wrap_cpp (TestUpdaterHelper_moc_sources TestUpdaterHelper.hpp)
  add_executable (TestUpdaterHelper TestUpdaterHelper.cpp ${TestUpdaterHelper_moc_sources})
  target_link_libraries (TestUpdaterHelper Update Qt6::Test)
  target_compile_definitions (TestUpdaterHelper PRIVATE UPDATER_EXE="$<TARGET_FILE:glabels-updater>")
  add_dependencies (TestUpdaterHelper glabels-updater)
  add_test (NAME UpdaterHelper COMMAND TestUpdaterHelper)

  #=======================================
  # Test UpdateService against a local web server: check, download, verify, unpack, smoke test
  #=======================================
  add_executable (FakeGlabelsBatch fake_batch.cpp)    # stands in for glabels-batch-qt (answers --Version)
  target_compile_features (FakeGlabelsBatch PRIVATE cxx_std_20)    # uses std::filesystem; links nothing that sets the standard
  qt6_wrap_cpp (TestUpdateService_moc_sources TestUpdateService.hpp)
  add_executable (TestUpdateService TestUpdateService.cpp ${TestUpdateService_moc_sources})
  target_link_libraries (TestUpdateService Update Qt6::Test)
  target_compile_definitions (TestUpdateService PRIVATE FAKE_BATCH_EXE="$<TARGET_FILE:FakeGlabelsBatch>")
  add_dependencies (TestUpdateService FakeGlabelsBatch)
  add_test (NAME UpdateService COMMAND TestUpdateService)

  #=======================================
  # Test applying a staged update with real processes: a running program is swapped out and restarted
  #=======================================
  add_executable (UpdateApplyHarness apply_harness.cpp)    # plays the running program
  target_link_libraries (UpdateApplyHarness Update)
  qt6_wrap_cpp (TestUpdateApply_moc_sources TestUpdateApply.hpp)
  add_executable (TestUpdateApply TestUpdateApply.cpp ${TestUpdateApply_moc_sources})
  target_link_libraries (TestUpdateApply Update Qt6::Test)
  target_compile_definitions (TestUpdateApply PRIVATE
    HARNESS_EXE="$<TARGET_FILE:UpdateApplyHarness>"
    UPDATER_EXE="$<TARGET_FILE:glabels-updater>"
    FAKE_PROGRAM_EXE="$<TARGET_FILE:FakeGlabelsBatch>")
  add_dependencies (TestUpdateApply UpdateApplyHarness glabels-updater FakeGlabelsBatch)
  add_test (NAME UpdateApply COMMAND TestUpdateApply)

  #=======================================
  # Test the user interface pieces (Help menu entry, Preferences page), offscreen
  #=======================================
  qt6_wrap_cpp (TestUpdateUi_moc_sources TestUpdateUi.hpp)
  add_executable (TestUpdateUi TestUpdateUi.cpp ${TestUpdateUi_moc_sources})
  target_link_libraries (TestUpdateUi glabels_lib Qt6::Test)
  add_test (NAME UpdateUi COMMAND TestUpdateUi)
  set_tests_properties (UpdateUi PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

endif (Qt6Test_FOUND)
