find_package(OpenSSL REQUIRED)

add_executable(sha512 sha512.c ${PROJECT_SOURCE_DIR}/src/spake2_sha512.c)
target_include_directories(sha512 PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(sha512 PRIVATE aparse)

add_executable(fe fe.c ${PROJECT_SOURCE_DIR}/src/spake2_fe.c)
target_include_directories(fe PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(fe PRIVATE aparse OpenSSL::Crypto)
