MESSAGE( STATUS "note: you dont have to use cmake, our hand-written Makefile more-or-less works" )
MESSAGE( STATUS "" )

#cmake_minimum_required(VERSION 3.0)
cmake_minimum_required(VERSION 2.8.7)

project(newtau)

SET (CMAKE_C_COMPILER   "/usr/bin/clang")
SET (CMAKE_CXX_COMPILER "/usr/bin/clang++")#-libc++")
SET (CMAKE_AR      "/usr/bin/llvm-ar")
SET (CMAKE_LINKER  "/usr/bin/llvm-ld")
SET (CMAKE_NM      "/usr/bin/llvm-nm")
SET (CMAKE_OBJDUMP "/usr/bin/llvm-objdump")
SET (CMAKE_RANLIB  "/usr/bin/llvm-ranlib")


set(MARPA_DIST ${CMAKE_SOURCE_DIR}/libmarpa/dist/)



set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -ggdb -g")# -stdlib=libc++ 
set(ASAN "-fsanitize-undefined-trap-on-error -fsanitize=address -fsanitize=integer -fsanitize=unsigned-integer-overflow ")
set(COMMON_FLAGS "-I${Boost_INCLUDE_DIRS} ${CMAKE_CXX_FLAGS} -I${MARPA_DIST} -Dwith_marpa -DJSON ")

set(SOURCE_FILES
    jsonld_tau.cpp
    jsonld_tau.h
    jsonld.cpp
    jsonld.h
    json_spirit.h
    misc.cpp
    misc.h
    nquads.cpp
    unifiers.cpp
    json_object.cpp
    json_object.h
    prover.cpp
    prover.h
    rdf.cpp
    rdf.h
    strings.h
    tau.cpp
    univar.cpp
    univar.h
    cppout.cpp
    newshit.cpp
    marpa_tau.cpp
)

find_package(Boost REQUIRED regex)

add_executable(tau_with_newshit ${SOURCE_FILES})
add_executable(tau ${SOURCE_FILES})
add_executable(tau-moderate ${SOURCE_FILES})
add_executable(tau-moderate0 ${SOURCE_FILES})
add_executable(tau-asan ${SOURCE_FILES})
add_executable(tau3 ${SOURCE_FILES})
add_executable(tau3-asan ${SOURCE_FILES})
add_executable(taud3 ${SOURCE_FILES})
add_executable(taud3-asan ${SOURCE_FILES})


SET_TARGET_PROPERTIES(tau_with_newshit   PROPERTIES COMPILE_FLAGS "${COMMON_FLAGS} -DDEBUG -O0 -DWITH_NEWSHIT -DDHT")
SET_TARGET_PROPERTIES(tau   PROPERTIES COMPILE_FLAGS "${COMMON_FLAGS} -DDEBUG -O0")
SET_TARGET_PROPERTIES(tau-moderate   PROPERTIES COMPILE_FLAGS "${COMMON_FLAGS} -O1")
SET_TARGET_PROPERTIES(tau-moderate0   PROPERTIES COMPILE_FLAGS "${COMMON_FLAGS} -O0")
SET_TARGET_PROPERTIES(tau-asan   PROPERTIES COMPILE_FLAGS "${COMMON_FLAGS} -DDEBUG -O0  ${ASAN}")
SET_TARGET_PROPERTIES(tau3  PROPERTIES COMPILE_FLAGS "${COMMON_FLAGS} -O3")
SET_TARGET_PROPERTIES(tau3-asan  PROPERTIES COMPILE_FLAGS "${COMMON_FLAGS} -O3 ${ASAN}")
SET_TARGET_PROPERTIES(taud3 PROPERTIES COMPILE_FLAGS "${COMMON_FLAGS} -DDEBUG -O3")
SET_TARGET_PROPERTIES(taud3-asan PROPERTIES COMPILE_FLAGS "${COMMON_FLAGS} -DDEBUG -O3 ${ASAN}")

#set(LIBSS "")

target_link_libraries(tau_with_newshit   PRIVATE ${Boost_LIBRARIES} ${CMAKE_DL_LIBS} -L${MARPA_DIST}/.libs -lmarpa)

target_link_libraries(tau    PRIVATE ${Boost_LIBRARIES} ${CMAKE_DL_LIBS} -L${MARPA_DIST}/.libs -lmarpa)
target_link_libraries(tau-moderate       PRIVATE ${Boost_LIBRARIES} ${CMAKE_DL_LIBS} "-L${MARPA_DIST}/.libs -lmarpa")
target_link_libraries(tau-moderate0     PRIVATE ${Boost_LIBRARIES} ${CMAKE_DL_LIBS} "-L${MARPA_DIST}/.libs -lmarpa")
target_link_libraries(tau3           PRIVATE ${Boost_LIBRARIES} ${CMAKE_DL_LIBS} "-L${MARPA_DIST}/.libs -lmarpa")
target_link_libraries(tau-asan            PRIVATE ${Boost_LIBRARIES} ${CMAKE_DL_LIBS} "-L${MARPA_DIST}/.libs -lmarpa")
target_link_libraries(tau3-asan           PRIVATE ${Boost_LIBRARIES} ${CMAKE_DL_LIBS} "-L${MARPA_DIST}/.libs -lmarpa")
target_link_libraries(taud3          PRIVATE ${Boost_LIBRARIES} ${CMAKE_DL_LIBS} "-L${MARPA_DIST}/.libs -lmarpa")
target_link_libraries(taud3-asan          PRIVATE ${Boost_LIBRARIES} ${CMAKE_DL_LIBS} "-L${MARPA_DIST}/.libs -lmarpa"  )
#target_include_directories(m-tau PRIVATE ${Boost_INCLUDE_DIRS} ${MARPA_DIST})
#target_include_directories(  tau PRIVATE ${Boost_INCLUDE_DIRS})



#<ngladitz> http://pastebin.com/Tm5AYHEg ... I added boost and dl and redid the source file listing to make it build (I didn't have a marpa.cpp) ... I don't really know what belongs where source file wise but it produces a binary :)
#<ngladitz> cmake 3.1 adds support for selection of language standards (though I would require 3.2 for it) which would make the -std=c++11 superfluous ... which in turn would make the build more portable
