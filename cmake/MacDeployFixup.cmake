# MacDeployFixup.cmake
#
# Deterministic post-deploy fixup for macOS Qt app bundles.
#
# Homebrew's macdeployqt (which the Qt CMake deploy API wraps) fails to scan
# transitive framework dependencies when it cannot resolve the build-time
# rpaths, leaving frameworks like QtDBus out of the bundle.  This script
# closes the gap:
#
#   1. Scans every binary in the bundle (executable, frameworks, plugins)
#      for dependencies, copies any missing Qt frameworks / libraries from
#      the Qt install prefix into Contents/Frameworks, and rewrites their
#      ids to @rpath/...  (repeats to a fixpoint so nested deps are caught)
#   2. Rewrites absolute paths pointing into the Qt install directory to
#      @rpath equivalents
#   3. Ensures the executables carry the @executable_path/../Frameworks rpath
#   4. Re-signs the bundle ad-hoc (required after any install_name_tool work)
#
# Usage from install(CODE):
#   set(APP_BUNDLE  "${CMAKE_INSTALL_PREFIX}/MyApp.app")
#   set(QT_LIB_DIR  "<Qt prefix>/lib")
#   include(MacDeployFixup.cmake)

cmake_minimum_required(VERSION 3.22)

set(_fw_dir "${APP_BUNDLE}/Contents/Frameworks")
set(_macos_dir "${APP_BUNDLE}/Contents/MacOS")
set(_plugins_dir "${APP_BUNDLE}/Contents/PlugIns")

if (NOT EXISTS "${APP_BUNDLE}/Contents/Info.plist")
   message(FATAL_ERROR "MacDeployFixup: ${APP_BUNDLE} is not an app bundle")
endif ()
if (NOT EXISTS "${QT_LIB_DIR}")
   message(FATAL_ERROR "MacDeployFixup: Qt lib dir not found: ${QT_LIB_DIR}")
endif ()

file(MAKE_DIRECTORY "${_fw_dir}")

#---------------------------------------------------------------
# 0: prune plugins that would drag in heavyweight frameworks the
#    app does not use (QtQuick, QtQml, QtPdf, QtVirtualKeyboard)
#---------------------------------------------------------------
foreach (_prune
      "${_plugins_dir}/platforminputcontexts/libqtvirtualkeyboardplugin.dylib"
      "${_plugins_dir}/imageformats/libqpdf.dylib"
   )
   if (EXISTS "${_prune}")
      file(REMOVE "${_prune}")
   endif ()
endforeach ()


#---------------------------------------------------------------
# Helper: run otool -L and return the list of dependency entries
#---------------------------------------------------------------
function(_mdf_dependencies bin out_var)
   set(_deps "")
   if (EXISTS "${bin}")
      find_program(OTOOL_TOOL otool REQUIRED)
      execute_process(
         COMMAND "${OTOOL_TOOL}" -L "${bin}"
         OUTPUT_VARIABLE _otool_out
         ERROR_QUIET
         RESULT_VARIABLE _otool_res
      )
      if (_otool_res EQUAL 0)
         string(REPLACE "\n" ";" _lines "${_otool_out}")
         foreach (_line ${_lines})
            string(REGEX MATCH "^[\t ]*[^\t ]+" _dep "${_line}")
            string(STRIP "${_dep}" _dep)
            if (_dep AND NOT _dep MATCHES "^${bin}$")
               list(APPEND _deps "${_dep}")
            endif ()
         endforeach ()
      endif ()
   endif ()
   set(${out_var} "${_deps}" PARENT_SCOPE)
endfunction()


#---------------------------------------------------------------
# 1+2: fixpoint scan for missing dependencies
#---------------------------------------------------------------
find_program(OTOOL_TOOL otool REQUIRED)
find_program(INSTALL_NAME_TOOL_TOOL install_name_tool REQUIRED)

set(_pass 0)
set(_changed TRUE)
while (_changed AND _pass LESS 12)
   math(EXPR _pass "${_pass} + 1")
   set(_changed FALSE)

   # Collect every binary in the bundle
   set(_bins "${_macos_dir}/*")
   file(GLOB _fw_bins "${_fw_dir}/*.framework/Versions/A/*")
   file(GLOB _dylibs "${_fw_dir}/*.dylib")
   file(GLOB_RECURSE _plugin_bins "${_plugins_dir}/*")
   list(APPEND _bins ${_fw_bins} ${_dylibs} ${_plugin_bins})

   foreach (_bin ${_bins})
      if (NOT EXISTS "${_bin}")
         continue()
      endif ()
      get_filename_component(_name "${_bin}" NAME)
      # skip non-binaries (headers, symlinks to Versions/A handled via REALPATH dupes, etc.)
      if (IS_SYMLINK "${_bin}")
         continue()
      endif ()

      _mdf_dependencies("${_bin}" _deps)

      foreach (_dep ${_deps})
         set(_target "")
         set(_rel "")
         if (_dep MATCHES "^@rpath/(.+)$")
            set(_rel "${CMAKE_MATCH_1}")
            if (_rel MATCHES "^([^/]+\\.framework)/")
               set(_target "${_fw_dir}/${CMAKE_MATCH_1}")
            else ()
               set(_target "${_fw_dir}/${_rel}")
            endif ()
         elseif (_dep MATCHES "^${QT_LIB_DIR}/(.+)$")
            set(_rel "${CMAKE_MATCH_1}")
            if (_rel MATCHES "^([^/]+\\.framework)/")
               set(_target "${_fw_dir}/${CMAKE_MATCH_1}")
            else ()
               set(_target "${_fw_dir}/${_rel}")
            endif ()
            # rewrite the absolute path to @rpath so it resolves in-bundle
            execute_process(
               COMMAND "${INSTALL_NAME_TOOL_TOOL}" -change "${_dep}" "@rpath/${_rel}" "${_bin}"
               OUTPUT_QUIET ERROR_QUIET
            )
            set(_changed TRUE)
         else ()
            continue()
         endif ()

         if (_target AND NOT EXISTS "${_target}")
            set(APP_BUNDLE "${APP_BUNDLE}")
            if (_rel MATCHES "^([^/]+\\.framework)/")
               set(_fw_name "${CMAKE_MATCH_1}")
               if (EXISTS "${QT_LIB_DIR}/${_fw_name}")
                  # Resolve through the (possibly symlinked) source, then a
                  # plain copy: internal framework symlinks are relative and
                  # stay valid.  (file(COPY ... FOLLOW_SYMLINK_CHAIN) is
                  # broken for this layout.)
                  get_filename_component(_fw_real "${QT_LIB_DIR}/${_fw_name}" REALPATH)
                  file(REMOVE_RECURSE "${_target}")
                  file(COPY "${_fw_real}" DESTINATION "${_fw_dir}")
                  # drop embedded signature; we re-sign at the end
                  file(REMOVE_RECURSE "${_target}/Versions/_CodeSignature")
                  # normalize id
                  execute_process(
                     COMMAND "${INSTALL_NAME_TOOL_TOOL}" -id "@rpath/${_fw_name}/Versions/A/${_fw_name}"
                             "${_target}/Versions/A/${_fw_name}"
                     OUTPUT_QUIET ERROR_QUIET
                  )
                  message(STATUS "MacDeployFixup: copied ${_fw_name} (${_pass})")
                  set(_changed TRUE)
               else ()
                  message(WARNING "MacDeployFixup: cannot find ${_fw_name} in ${QT_LIB_DIR}")
               endif ()
            elseif (_rel MATCHES "\\.dylib$")
               set(_src "${QT_LIB_DIR}/${_rel}")
               if (EXISTS "${_src}")
                  get_filename_component(_real "${_src}" REALPATH)
                  file(COPY "${_real}" DESTINATION "${_fw_dir}")
                  # REALPATH may resolve to a versioned name; normalize to
                  # the name the @rpath expects
                  get_filename_component(_realname "${_real}" NAME)
                  if (NOT _realname STREQUAL _rel)
                     file(RENAME "${_fw_dir}/${_realname}" "${_target}")
                  endif ()
                  execute_process(
                     COMMAND "${INSTALL_NAME_TOOL_TOOL}" -id "@rpath/${_rel}" "${_target}"
                     OUTPUT_QUIET ERROR_QUIET
                  )
                  message(STATUS "MacDeployFixup: copied ${_rel} (${_pass})")
                  set(_changed TRUE)
               else ()
                  message(WARNING "MacDeployFixup: cannot find ${_src}")
               endif ()
            endif ()
         endif ()
      endforeach ()
   endforeach ()
endwhile()

#---------------------------------------------------------------
# 3: ensure executables carry the bundle rpath
#---------------------------------------------------------------
file(GLOB _exes "${_macos_dir}/*")
foreach (_exe ${_exes})
   if (NOT EXISTS "${_exe}" OR IS_SYMLINK "${_exe}")
      continue()
   endif ()
   execute_process(
      COMMAND "${OTOOL_TOOL}" -l "${_exe}"
      OUTPUT_VARIABLE _otool_out
      ERROR_QUIET
   )
   if (NOT _otool_out MATCHES "@executable_path/../Frameworks")
      execute_process(
         COMMAND "${INSTALL_NAME_TOOL_TOOL}" -add_rpath "@executable_path/../Frameworks" "${_exe}"
         OUTPUT_QUIET ERROR_QUIET
      )
   endif ()
endforeach ()

#---------------------------------------------------------------
# 4: re-sign ad-hoc (after all install_name_tool work)
#---------------------------------------------------------------
execute_process(COMMAND /usr/bin/xattr -cr "${APP_BUNDLE}" OUTPUT_QUIET ERROR_QUIET)
execute_process(
   COMMAND /usr/bin/codesign --force --deep -s - "${APP_BUNDLE}"
   RESULT_VARIABLE _cs_res
   OUTPUT_QUIET ERROR_QUIET
)
if (NOT _cs_res EQUAL 0)
   message(WARNING "MacDeployFixup: codesign failed for ${APP_BUNDLE}")
endif ()

message(STATUS "MacDeployFixup: finished ${APP_BUNDLE}")
