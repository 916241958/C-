cmake_minimum_required(VERSION 3.15)
project(Connection_Pool)

set(CMAKE_CXX_STANDARD 11)

set(INC_DIR D:\\mysql5.1\\mysql-5.7.24-winx64\\include)
set(LINK_DIR D:\\mysql5.1\\mysql-5.7.24-winx64\\lib)

#引入头文件
include_directories(${INC_DIR})

#引入库文件
link_directories(${INC_DIR})
link_libraries(libmysql)

add_executable(Connection_Pool src/main.cpp include/Connection.h include/public.h include/CommonConnectionPool.h src/CommonConnectionPool.cpp src/Connection.cpp src/pch.cpp include/pch.h)

target_link_libraries(Connection_Pool libmysql)