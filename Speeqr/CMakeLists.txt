cmake_minimum_required(VERSION 3.20)
project(speeqr VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Find Boost ─────────────────────────────────────────────
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

# ── Server executable ──────────────────────────────────────
add_executable(speeqr_server
    server/main.cpp
    server/NtpServer.cpp
)

target_include_directories(speeqr_server PRIVATE
    ${CMAKE_SOURCE_DIR}/server
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(speeqr_server PRIVATE
    Boost::system
    Boost::thread
    Threads::Threads
)

# ── MSVC specific flags (safe to keep) ─────────────────────
if(MSVC)
    target_compile_options(speeqr_server PRIVATE
        /W4 /O2 /EHsc
        /D_WIN32_WINNT=0x0601
        /DWIN32_LEAN_AND_MEAN
        /DNOMINMAX
    )
    target_compile_definitions(speeqr_server PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# ── Copy client folder next to binary ──────────────────────
add_custom_command(TARGET speeqr_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/client
        $<TARGET_FILE_DIR:speeqr_server>/client
    COMMENT "Copying web client..."
)