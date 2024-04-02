# 项目名称
project(TCP_Server)

# 设置fiber库的源文件目录
set(FIBER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src/fiber)

# 将fiber目录下所有源文件添加到 FIBER_SRC_LIST 变量中
file(GLOB FIBER_SRC_LIST "${FIBER_SRC_DIR}/*.cpp")

# 包含fiber库头文件目录
include_directories(${FIBER_SRC_DIR}/include)

# 添加fiber库的源文件目录，以便于找到头文件
include_directories(${FIBER_SRC_DIR})

# 添加编译test_tcp_server.cc所需的源文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 创建fiber_lib静态库
add_library(fiber_lib STATIC ${FIBER_SRC_LIST})

# 编译test_tcp_server.cc到一个可执行文件
add_executable(test_tcp_server test_tcp_server.cc)

# 链接fiber_lib库到test_tcp_server可执行文件
target_link_libraries(test_tcp_server fiber_lib -levent -ldl)