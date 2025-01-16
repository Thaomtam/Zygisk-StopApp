cmake_minimum_required(VERSION 3.22.1)

# Đặt tên dự án
project(KTASTOPAPP)

# Sử dụng chuẩn C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Đầu ra của thư viện sẽ được đặt trong thư mục build
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Đặt các cờ biên dịch
set(C_FLAGS "-Werror=format -fdata-sections -ffunction-sections")
set(CXX_FLAGS "-Werror=format -fdata-sections -ffunction-sections -fno-exceptions -fno-rtti")

# Cấu hình tối ưu hóa cho chế độ Release
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(C_FLAGS "${C_FLAGS} -O2 -fvisibility=hidden -fvisibility-inlines-hidden")
else ()
    set(C_FLAGS "${C_FLAGS} -O0")
endif ()

# Thêm cờ biên dịch vào CMake
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_FLAGS} ${CXX_FLAGS}")

# Đặt cờ liên kết
set(LINKER_FLAGS "-ffixed-x18 -Wl,--hash-style=both")
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(LINKER_FLAGS "${LINKER_FLAGS} -Wl,-exclude-libs,ALL -Wl,--gc-sections")
endif ()
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")

# Thêm thư viện chính cho module KTASTOPAPP
add_library(KTASTOPAPP SHARED main.cpp)

# Liên kết thư viện Android log
target_link_libraries(KTASTOPAPP log)

# Bật exceptions riêng cho main_fixed.cpp
set_source_files_properties(${CMAKE_SOURCE_DIR}/main.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

# Hỗ trợ armeabi-v7a và arm64-v8a
set(CMAKE_ANDROID_ARCH_ABI "arm64-v8a;armeabi-v7a")

# Kiểm tra chế độ Release để strip symbols (giữ đơn giản, tránh lỗi build)
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET KTASTOPAPP POST_BUILD
            COMMAND ${CMAKE_STRIP} --strip-unneeded "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libKTASTOPAPP.so"
            COMMENT "Stripping unneeded symbols from KTASTOPAPP")
endif ()
