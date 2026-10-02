# Native Apple packaging; no paid signing service or installer scripts.
set(_architectures ${CMAKE_OSX_ARCHITECTURES})
list(SORT _architectures)
if(NOT "${_architectures}" STREQUAL "arm64;x86_64")
  message(FATAL_ERROR "The installer requires CMAKE_OSX_ARCHITECTURES=arm64;x86_64")
endif()
if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "11.0")
  message(FATAL_ERROR "The universal installer requires a macOS deployment target of at least 11.0")
endif()

include(CPackComponent)
set(CPACK_COMPONENTS_ALL Notices)
set(_bundles)
foreach(_product DensityD01 HarmonicH01 SequenceS01 LoopL01 ImpulseI01 FieldF01)
  get_target_property(_name ${_product} JUCE_PRODUCT_NAME)
  cpack_add_component_group(${_product} DISPLAY_NAME "${_name}")
  foreach(_format AU VST3)
    set(_target ${_product}_${_format})
    set(_destination VST3)
    if(_format STREQUAL "AU")
      set(_destination Components)
    endif()
    install(DIRECTORY "$<TARGET_BUNDLE_DIR:${_target}>"
      DESTINATION "Library/Audio/Plug-Ins/${_destination}"
      COMPONENT ${_target} USE_SOURCE_PERMISSIONS)
    cpack_add_component(${_target} DISPLAY_NAME "${_format}"
      DESCRIPTION "${_name} — ${_format}" GROUP ${_product})
    list(APPEND CPACK_COMPONENTS_ALL ${_target})
    list(APPEND _bundles ${_target})
  endforeach()
endforeach()

find_program(ASTE_RUSTC rustc REQUIRED)
set(_cleanup_app "${PROJECT_BINARY_DIR}/cleanup/Aste Plugin Cleanup.app")
configure_file("${PROJECT_SOURCE_DIR}/cmake/CleanupInfo.plist.in"
  "${_cleanup_app}/Contents/Info.plist" @ONLY)
add_custom_command(OUTPUT "${_cleanup_app}/Contents/MacOS/aste-plugin-cleanup"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_cleanup_app}/Contents/MacOS"
  COMMAND ${CMAKE_COMMAND} -E env "SDKROOT=${CMAKE_OSX_SYSROOT}"
    "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
    ${ASTE_RUSTC} --edition 2021 -D warnings -C opt-level=2 -C strip=symbols
    --target aarch64-apple-darwin "${PROJECT_SOURCE_DIR}/tools/macos_cleanup.rs"
    -o "${PROJECT_BINARY_DIR}/cleanup/arm64"
  COMMAND ${CMAKE_COMMAND} -E env "SDKROOT=${CMAKE_OSX_SYSROOT}"
    "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
    ${ASTE_RUSTC} --edition 2021 -D warnings -C opt-level=2 -C strip=symbols
    --target x86_64-apple-darwin "${PROJECT_SOURCE_DIR}/tools/macos_cleanup.rs"
    -o "${PROJECT_BINARY_DIR}/cleanup/x86_64"
  COMMAND lipo -create "${PROJECT_BINARY_DIR}/cleanup/arm64" "${PROJECT_BINARY_DIR}/cleanup/x86_64"
    -output "${_cleanup_app}/Contents/MacOS/aste-plugin-cleanup"
  COMMAND codesign --force --sign - "${_cleanup_app}"
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/macos_cleanup.rs" "${_cleanup_app}/Contents/Info.plist"
  VERBATIM)
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/macos_cleanup_tests"
  COMMAND ${ASTE_RUSTC} --edition 2021 --test "${PROJECT_SOURCE_DIR}/tools/macos_cleanup.rs"
    -o "${PROJECT_BINARY_DIR}/macos_cleanup_tests"
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/macos_cleanup.rs" VERBATIM)
add_test(NAME macos_cleanup_tests COMMAND "${PROJECT_BINARY_DIR}/macos_cleanup_tests")
install(DIRECTORY "${_cleanup_app}" DESTINATION Applications COMPONENT Notices USE_SOURCE_PERMISSIONS)
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/macos_productbuild"
  COMMAND ${ASTE_RUSTC} --edition 2021 "${PROJECT_SOURCE_DIR}/tools/macos_productbuild.rs"
    -o "${PROJECT_BINARY_DIR}/macos_productbuild"
  COMMAND ${ASTE_RUSTC} --edition 2021 --test "${PROJECT_SOURCE_DIR}/tools/macos_productbuild.rs"
    -o "${PROJECT_BINARY_DIR}/macos_productbuild_tests"
  COMMAND "${PROJECT_BINARY_DIR}/macos_productbuild_tests"
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/macos_productbuild.rs" VERBATIM)
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/macos_bundle_tests"
  COMMAND ${ASTE_RUSTC} --edition 2021
    "${PROJECT_SOURCE_DIR}/tests/macos_bundle_tests.rs"
    -o "${PROJECT_BINARY_DIR}/macos_bundle_tests"
  DEPENDS "${PROJECT_SOURCE_DIR}/tests/macos_bundle_tests.rs" VERBATIM)
add_custom_target(macos_bundle_check ALL
  DEPENDS "${PROJECT_BINARY_DIR}/macos_bundle_tests"
          "${PROJECT_BINARY_DIR}/macos_productbuild"
          "${PROJECT_BINARY_DIR}/macos_cleanup_tests"
          "${_cleanup_app}/Contents/MacOS/aste-plugin-cleanup" ${_bundles})
add_test(NAME macos_bundle_tests
  COMMAND "${PROJECT_BINARY_DIR}/macos_bundle_tests" "${PROJECT_BINARY_DIR}" "${PROJECT_VERSION}")

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
              "${PROJECT_SOURCE_DIR}/docs/THIRD_PARTY_NOTICES.md"
              "${PROJECT_SOURCE_DIR}/docs/MACOS_INSTALL.txt"
        DESTINATION "Library/Application Support/Aste" COMPONENT Notices)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/LICENSES/"
        DESTINATION "Library/Application Support/Aste/LICENSES" COMPONENT Notices)
foreach(_notice
    LICENSE.md
    modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt
    modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt
    "modules/juce_audio_formats/codecs/flac/Flac Licence.txt"
    "modules/juce_audio_formats/codecs/oggvorbis/Ogg Vorbis Licence.txt"
    modules/juce_graphics/image_formats/jpglib/README
    modules/juce_graphics/image_formats/pnglib/LICENSE
    modules/juce_graphics/fonts/harfbuzz/COPYING
    modules/juce_graphics/unicode/sheenbidi/LICENSE
    modules/juce_core/zip/zlib/README)
  get_filename_component(_parent "${_notice}" DIRECTORY)
  install(FILES "${JUCE_SOURCE_DIR}/${_notice}"
    DESTINATION "Library/Application Support/Aste/ThirdParty/JUCE/${_parent}" COMPONENT Notices)
endforeach()
cpack_add_component(Notices DISPLAY_NAME "Licenses and cleanup utility" REQUIRED)

# Ship matching first-party AND dependency source next to the installer.
# Explicit roots avoid archiving build outputs, Git history, or local secrets.
install(DIRECTORY ${PROJECT_SOURCE_DIR}/apps ${PROJECT_SOURCE_DIR}/libs
                  ${PROJECT_SOURCE_DIR}/tests ${PROJECT_SOURCE_DIR}/tools
                  ${PROJECT_SOURCE_DIR}/cmake ${PROJECT_SOURCE_DIR}/docs
                  ${PROJECT_SOURCE_DIR}/LICENSES ${PROJECT_SOURCE_DIR}/.github
        DESTINATION Aste COMPONENT Source EXCLUDE_FROM_ALL
        PATTERN ".DS_Store" EXCLUDE)
install(FILES ${PROJECT_SOURCE_DIR}/CMakeLists.txt ${PROJECT_SOURCE_DIR}/LICENSE
              ${PROJECT_SOURCE_DIR}/README.md ${PROJECT_SOURCE_DIR}/CHANGELOG.md
              ${PROJECT_SOURCE_DIR}/.clang-format
              ${PROJECT_SOURCE_DIR}/.gitignore
        DESTINATION Aste COMPONENT Source EXCLUDE_FROM_ALL)
install(DIRECTORY "${JUCE_SOURCE_DIR}/"
        DESTINATION JUCE COMPONENT Source EXCLUDE_FROM_ALL
        PATTERN ".git" EXCLUDE PATTERN ".DS_Store" EXCLUDE)

set(CPACK_GENERATOR productbuild)
set(CPACK_COMMAND_PRODUCTBUILD "${PROJECT_BINARY_DIR}/macos_productbuild")
set(CPACK_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
set(CPACK_ASTE_MIN_MACOS "${CMAKE_OSX_DEPLOYMENT_TARGET}")
set(CPACK_PACKAGE_NAME Aste)
set(CPACK_PACKAGE_VENDOR "Aste contributors")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(_package_name "Aste-${PROJECT_VERSION}-macOS-universal-development")
set(CPACK_PACKAGE_FILE_NAME "${_package_name}")
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/packages")
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
set(CPACK_PRODUCTBUILD_IDENTIFIER "invalid.aste.installer")
set(CPACK_PRODUCTBUILD_DOMAINS TRUE)
set(CPACK_PRODUCTBUILD_DOMAINS_ANYWHERE FALSE)
set(CPACK_PRODUCTBUILD_DOMAINS_USER FALSE)
set(CPACK_PRODUCTBUILD_DOMAINS_ROOT TRUE)
configure_file("${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_BINARY_DIR}/AGPL-3.0.txt" COPYONLY)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_BINARY_DIR}/AGPL-3.0.txt")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/docs/MACOS_INSTALL.txt")
include(CPack)

set(_delivery "${PROJECT_BINARY_DIR}/delivery")
set(_source "${PROJECT_BINARY_DIR}/corresponding-source")
add_custom_target(aste_macos_package
  COMMAND "${PROJECT_BINARY_DIR}/macos_bundle_tests" "${PROJECT_BINARY_DIR}" "${PROJECT_VERSION}"
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${_delivery}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_delivery}"
  COMMAND ${CMAKE_CPACK_COMMAND} --config "${PROJECT_BINARY_DIR}/CPackConfig.cmake" -C Release
  COMMAND "${PROJECT_BINARY_DIR}/macos_bundle_tests" "${PROJECT_BINARY_DIR}" "${PROJECT_VERSION}"
    "${CPACK_PACKAGE_DIRECTORY}/${_package_name}.pkg"
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CPACK_PACKAGE_DIRECTORY}/${_package_name}.pkg" "${_delivery}/Install Aste.pkg"
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${_source}"
  COMMAND ${CMAKE_COMMAND} --install "${PROJECT_BINARY_DIR}" --config Release
    --component Source --prefix "${_source}"
  COMMAND ${CMAKE_COMMAND} -E chdir "${_source}"
    ${CMAKE_COMMAND} -E tar czf "${_delivery}/Aste-${PROJECT_VERSION}-Source.tar.gz" --format=gnutar .
  COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_SOURCE_DIR}/docs/MACOS_INSTALL.txt"
    "${_delivery}/Read Me.txt"
  COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_BINARY_DIR}/build-metadata.json" "${_delivery}"
  COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_BINARY_DIR}/test-results.xml" "${_delivery}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${_cleanup_app}" "${_delivery}/Aste Plugin Cleanup.app"
  COMMAND hdiutil create -ov -format UDZO -volname "Aste ${PROJECT_VERSION} Development"
    -srcfolder "${_delivery}" "${CPACK_PACKAGE_DIRECTORY}/${_package_name}.dmg"
  DEPENDS macos_bundle_check
  COMMENT "Creating unnotarized universal installer with corresponding source"
  VERBATIM)
