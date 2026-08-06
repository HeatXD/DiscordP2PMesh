cmake_minimum_required(VERSION 3.16)

project(DiscordP2PMesh LANGUAGES C CXX)

set(DPMESH_DISCORD_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/discord_social_sdk"
    CACHE PATH "Path to the vendored Discord Social SDK, see thirdparty/README.md")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libjuice/CMakeLists.txt")
    message(FATAL_ERROR "thirdparty/libjuice is empty, run: git submodule update --init --recursive")
endif()

set(NO_TESTS ON CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/libjuice EXCLUDE_FROM_ALL)

find_package(Threads REQUIRED)

find_library(DPMESH_DISCORD_SDK_LIBRARY
    NAMES discord_partner_sdk
    PATHS "${DPMESH_DISCORD_SDK_DIR}/lib/release" "${DPMESH_DISCORD_SDK_DIR}/lib"
    NO_DEFAULT_PATH
)

if(NOT DPMESH_DISCORD_SDK_LIBRARY)
    message(FATAL_ERROR
        "Discord Social SDK not found under '${DPMESH_DISCORD_SDK_DIR}'. "
        "Download it and lay it out as described in thirdparty/README.md, or point "
        "-DDPMESH_DISCORD_SDK_DIR at an existing copy."
    )
endif()

add_library(discordp2pmesh
    src/discord_p2p_mesh.cpp
    src/session.cpp
    src/peer_mesh.cpp
    src/discordpp_impl.cpp
)

target_compile_features(discordp2pmesh PRIVATE cxx_std_20)
target_compile_definitions(discordp2pmesh PRIVATE DPMESH_EXPORTS)

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(discordp2pmesh PUBLIC DPMESH_STATIC)
endif()

if(MSVC)
    target_compile_definitions(discordp2pmesh PRIVATE NOMINMAX)
endif()

target_include_directories(discordp2pmesh
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include/private"
        "${DPMESH_DISCORD_SDK_DIR}/include"
)

# juice-static keeps ICE inside our own binary, so only the Discord SDK ships alongside.
target_link_libraries(discordp2pmesh
    PRIVATE juice-static Threads::Threads "${DPMESH_DISCORD_SDK_LIBRARY}"
)

# Cached so parent projects can find the runtime dependencies gathered below.
set(DPMESH_OUTPUT_DIR "${CMAKE_BINARY_DIR}/out" CACHE INTERNAL "DiscordP2PMesh output directory")

# $<1:...> stops multi-config generators from appending a per-config subdirectory.
set_target_properties(discordp2pmesh PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    BUILD_RPATH_USE_ORIGIN ON
    INSTALL_RPATH "$ORIGIN"
    ARCHIVE_OUTPUT_DIRECTORY "$<1:${DPMESH_OUTPUT_DIR}>"
    LIBRARY_OUTPUT_DIRECTORY "$<1:${DPMESH_OUTPUT_DIR}>"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${DPMESH_OUTPUT_DIR}>"
)

set_property(TARGET juice-static PROPERTY POSITION_INDEPENDENT_CODE ON)

# The SDK ships every platform in one tree, and on Windows the runtime DLL sits apart from
# the import library, so pick out only what this platform loads at runtime.
if(WIN32)
    file(GLOB DPMESH_RUNTIME_FILES "${DPMESH_DISCORD_SDK_DIR}/bin/release/*.dll")
elseif(APPLE)
    file(GLOB DPMESH_RUNTIME_FILES "${DPMESH_DISCORD_SDK_DIR}/lib/release/*.dylib")
else()
    file(GLOB DPMESH_RUNTIME_FILES "${DPMESH_DISCORD_SDK_DIR}/lib/release/*.so*")
endif()

if(DPMESH_RUNTIME_FILES)
    add_custom_command(TARGET discordp2pmesh POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            ${DPMESH_RUNTIME_FILES} "$<TARGET_FILE_DIR:discordp2pmesh>"
        COMMENT "Copying runtime dependencies next to discordp2pmesh"
    )
endif()
