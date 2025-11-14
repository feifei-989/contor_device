
# 设置最低 CMake 版本要求
cmake_minimum_required(VERSION 3.10)

# 项目信息
project(ModbusTCPWithCRC LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find dependencies ---
find_package(PkgConfig REQUIRED)

# For uWebSockets, we only need the include directory as it's header-only.
# We must, however, link its dependencies manually.
include_directories(/usr/local/include)
include_directories(/usr/local/include/uWebSockets)

# Find libmodbus
pkg_check_modules(LIBMODBUS REQUIRED libmodbus)
include_directories(${LIBMODBUS_INCLUDE_DIRS})

add_compile_options(-g -O0)

# 定义项目的源文件目录
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(LINK_LIB modbus)
set(EXE_FILE sjt)
# 搜索 src 目录下的所有 .cpp 和 .h 文件
file(GLOB_RECURSE SOURCES ${SRC_DIR}/*.cpp ${SRC_DIR}/*.h)

# 添加编译选项
add_compile_options(${LIBMODBUS_CFLAGS_OTHER})

# 定义可执行文件
add_executable(${EXE_FILE} ${SOURCES})

# 链接库
# We link z, uv, ssl, and crypto for uWebSockets, and libmodbus for the rest.
target_link_libraries(${EXE_FILE} 
    ${LINK_LIB} 
    z
    uv
    ssl
    crypto
    uSockets
)

# 安装路径设置
install(TARGETS ${EXE_FILE} DESTINATION bin)


