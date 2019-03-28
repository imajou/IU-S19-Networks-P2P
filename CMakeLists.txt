cmake_minimum_required(VERSION 3.13)
project(peep2peep C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

file(GLOB PEER_FILES
        "assets/*.txt"
        )
file(COPY ${PEER_FILES} DESTINATION myDestination)

add_executable(peep2peep peep.c peep.h peep_log.c peep_log.h peep_util.c peep_util.h peep_struct.h)

target_link_libraries(peep2peep pthread)