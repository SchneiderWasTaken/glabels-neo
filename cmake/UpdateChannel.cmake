# UpdateChannel.cmake - decide which release channel this build follows for self-update.
#
# Included from the top-level CMakeLists.txt after the git version information is known
# (needs RELEASE_TAG_VALID / RELEASE_TAG).  Sets, for update/UpdateConfig.hpp.in:
#
#    UPDATE_CHANNEL   "stable", "edge" or "" (self-update disabled in this build)
#    UPDATE_COMMIT    full hash of HEAD
#    UPDATE_TAG       release tag of HEAD, e.g. "v1.2.0" (empty if HEAD is not tagged)
#
# GLABELS_UPDATE_CHANNEL selects the behaviour:
#    auto     (default) stable when HEAD is exactly on a tag, edge when built by the push-to-main
#             run of the release workflow, otherwise off.  Local and pull-request builds never
#             try to update themselves.
#    stable | edge | off   force one.

set (GLABELS_UPDATE_CHANNEL "auto" CACHE STRING "Self-update channel of this build: auto, stable, edge or off")
set_property (CACHE GLABELS_UPDATE_CHANNEL PROPERTY STRINGS auto stable edge off)
set (GLABELS_UPDATE_REPO "SchneiderWasTaken/glabels-neo" CACHE STRING "GitHub repository (owner/name) that publishes the releases")

execute_process (
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND git rev-parse HEAD
  RESULT_VARIABLE UPDATE_COMMIT_VALID
  OUTPUT_VARIABLE UPDATE_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if (NOT "${UPDATE_COMMIT_VALID}" STREQUAL "0")
  set (UPDATE_COMMIT "")
endif ()

if ("${RELEASE_TAG_VALID}" STREQUAL "0")
  set (UPDATE_TAG "${RELEASE_TAG}")
else ()
  set (UPDATE_TAG "")
endif ()

set (UPDATE_CHANNEL "${GLABELS_UPDATE_CHANNEL}")
if (UPDATE_CHANNEL STREQUAL "auto")
  if (NOT UPDATE_TAG STREQUAL "")
    set (UPDATE_CHANNEL "stable")
  elseif ("$ENV{GITHUB_ACTIONS}" STREQUAL "true" AND "$ENV{GITHUB_EVENT_NAME}" STREQUAL "push" AND "$ENV{GITHUB_REF}" STREQUAL "refs/heads/main")
    set (UPDATE_CHANNEL "edge")
  else ()
    set (UPDATE_CHANNEL "off")
  endif ()
endif ()

# A channel is only usable if the build knows what it is, to compare against the newest release.
if (UPDATE_CHANNEL STREQUAL "stable" AND UPDATE_TAG STREQUAL "")
  message (WARNING "Self-update channel 'stable' needs HEAD to be on a release tag; self-update disabled.")
  set (UPDATE_CHANNEL "off")
endif ()
if (UPDATE_CHANNEL STREQUAL "edge" AND UPDATE_COMMIT STREQUAL "")
  message (WARNING "Self-update channel 'edge' needs a git checkout to identify the build; self-update disabled.")
  set (UPDATE_CHANNEL "off")
endif ()
if (UPDATE_CHANNEL STREQUAL "off")
  set (UPDATE_CHANNEL "")
endif ()

message (STATUS "Self-update channel ..... " "${UPDATE_CHANNEL}")
