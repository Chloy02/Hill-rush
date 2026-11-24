cmake_minimum_required(VERSION 3.22)

project(hillrush
    VERSION 1.1.0
    DESCRIPTION "Hill Rush - a physics hill-climb driving game"
    HOMEPAGE_URL "https://example.invalid/hillrush"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(HILLRUSH_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(HILLRUSH_STATIC_RUNTIME "Link libstdc++ and libgcc statically (portable binaries)" ON)
option(HILLRUSH_BUILD_TESTS "Build the unit test executable" ON)
option(HILLRUSH_WERROR "Treat warnings in game code as errors" OFF)

if(HILLRUSH_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# Third-party libraries (vendored, see third_party/README.md)
# ---------------------------------------------------------------------------
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PLATFORM "Desktop" CACHE STRING "" FORCE)
set(OPENGL_VERSION "3.3" CACHE STRING "" FORCE)
# X11 only: works natively on X11 and through XWayland on every Wayland desktop,
# without depending on libdecor / wayland-scanner at build or run time.
set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/raylib EXCLUDE_FROM_ALL)
# The game saves screenshots itself (to the user's data dir); raylib's built-in
# F12 handler would drop files into whatever the current directory happens to be.
target_compile_definitions(raylib PRIVATE SUPPORT_SCREEN_CAPTURE=0)

set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BENCHMARKS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/box2d EXCLUDE_FROM_ALL)

# Treat third-party headers as system headers so our warning flags don't apply to them.
foreach(dep raylib box2d)
    get_target_property(dep_includes ${dep} INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(${dep} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${dep_includes}")
endforeach()

# ENet 1.3.18 (reliable UDP for online races). Built here rather than with its
# own CMakeLists, which adds install rules and no usage requirements.
file(GLOB ENET_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/enet/*.c")
add_library(enet STATIC ${ENET_SOURCES})
target_include_directories(enet SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/enet/include")
if(WIN32)
    target_link_libraries(enet PUBLIC ws2_32 winmm)
else()
    # The same platform checks as ENet's own build.
    include(CheckFunctionExists)
    include(CheckStructHasMember)
    include(CheckTypeSize)
    foreach(fn fcntl poll getaddrinfo getnameinfo gethostbyname_r gethostbyaddr_r inet_pton inet_ntop)
        string(TOUPPER "HAS_${fn}" flag)
        check_function_exists(${fn} ${flag})
        if(${flag})
            target_compile_definitions(enet PRIVATE ${flag}=1)
        endif()
    endforeach()
    check_struct_has_member("struct msghdr" "msg_flags" "sys/types.h;sys/socket.h" HAS_MSGHDR_FLAGS)
    if(HAS_MSGHDR_FLAGS)
        target_compile_definitions(enet PRIVATE HAS_MSGHDR_FLAGS=1)
    endif()
    set(CMAKE_EXTRA_INCLUDE_FILES "sys/types.h" "sys/socket.h")
    check_type_size("socklen_t" HAS_SOCKLEN_T BUILTIN_TYPES_ONLY)
    unset(CMAKE_EXTRA_INCLUDE_FILES)
    if(HAS_SOCKLEN_T)
        target_compile_definitions(enet PRIVATE HAS_SOCKLEN_T=1)
    endif()
endif()

# ---------------------------------------------------------------------------
# Embedded assets: every file listed here is compiled into the executable.
# ---------------------------------------------------------------------------
set(HILLRUSH_EMBED_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/LilitaOne-Regular.ttf")
set(HILLRUSH_EMBED_NAMES
    "font_main")
set(EMBED_OUT "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedAssets.cpp")
add_custom_command(
    OUTPUT "${EMBED_OUT}"
    COMMAND "${CMAKE_COMMAND}"
        "-DOUT=${EMBED_OUT}"
        "-DFILES=${HILLRUSH_EMBED_FILES}"
        "-DNAMES=${HILLRUSH_EMBED_NAMES}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFiles.cmake"
    DEPENDS ${HILLRUSH_EMBED_FILES} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFiles.cmake"
    COMMENT "Embedding game assets"
    VERBATIM)

# ---------------------------------------------------------------------------
# Warnings for our own code only
# ---------------------------------------------------------------------------
set(HILLRUSH_WARNINGS -Wall -Wextra -Wshadow -Wno-missing-field-initializers)
if(HILLRUSH_WERROR)
    list(APPEND HILLRUSH_WARNINGS -Werror)
endif()

configure_file(src/core/Version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/core/Version.hpp" @ONLY)

# ---------------------------------------------------------------------------
# hillrush_sim: game rules and physics. No raylib dependency, so it can be
# unit-tested and soak-tested headless.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE HILLRUSH_SIM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sim/*.cpp")
add_library(hillrush_sim STATIC ${HILLRUSH_SIM_SOURCES})
target_include_directories(hillrush_sim PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(hillrush_sim PUBLIC box2d::box2d)
target_compile_options(hillrush_sim PRIVATE ${HILLRUSH_WARNINGS})

# ---------------------------------------------------------------------------
# hillrush_net: online races (rooms, discovery, car snapshots). No raylib
# either; ENet stays private to it, because its Windows headers clash with
# raylib's names.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE HILLRUSH_NET_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/net/*.cpp")
add_library(hillrush_net STATIC ${HILLRUSH_NET_SOURCES})
target_link_libraries(hillrush_net PUBLIC hillrush_sim PRIVATE enet)
target_compile_options(hillrush_net PRIVATE ${HILLRUSH_WARNINGS})
if(WIN32)
    target_compile_definitions(hillrush_net PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ---------------------------------------------------------------------------
# hillrush: the game executable
# ---------------------------------------------------------------------------
file(GLOB_RECURSE HILLRUSH_GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/*.cpp")
add_executable(hillrush src/main.cpp ${HILLRUSH_GAME_SOURCES} "${EMBED_OUT}")
target_link_libraries(hillrush PRIVATE hillrush_net hillrush_sim raylib)
# raylib bundles GLFW; the game calls it directly for reliable fullscreen handling.
target_include_directories(hillrush SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib/src/external/glfw/include")
target_compile_options(hillrush PRIVATE ${HILLRUSH_WARNINGS})
if(HILLRUSH_STATIC_RUNTIME AND NOT HILLRUSH_SANITIZE)
    target_link_options(hillrush PRIVATE -static-libstdc++ -static-libgcc)
endif()
set_target_properties(hillrush PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(WIN32)
    # Icon + version info, no console window, and one self-contained .exe.
    enable_language(RC)
    set(HILLRUSH_ICO "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/hillrush.ico")
    configure_file(packaging/windows/hillrush.rc.in "${CMAKE_CURRENT_BINARY_DIR}/generated/hillrush.rc" @ONLY)
    target_sources(hillrush PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/hillrush.rc")
    set_target_properties(hillrush PROPERTIES WIN32_EXECUTABLE ON OUTPUT_NAME HillRush)
    if(MINGW)
        # C99-conforming printf (%zu, %lld) instead of the old msvcrt one.
        target_compile_definitions(hillrush_sim PUBLIC __USE_MINGW_ANSI_STDIO=1)
        target_link_options(hillrush PRIVATE -static)
    endif()
endif()

install(TARGETS hillrush RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(HILLRUSH_BUILD_TESTS)
    enable_testing()
    file(GLOB HILLRUSH_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    add_executable(hillrush_tests ${HILLRUSH_TEST_SOURCES})
    target_link_libraries(hillrush_tests PRIVATE hillrush_net hillrush_sim)
    target_compile_options(hillrush_tests PRIVATE ${HILLRUSH_WARNINGS})
    set_target_properties(hillrush_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    if(MINGW)
        target_link_options(hillrush_tests PRIVATE -static)
    endif()
    add_test(NAME unit_tests COMMAND hillrush_tests)
endif()
