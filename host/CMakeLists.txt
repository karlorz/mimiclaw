cmake_minimum_required(VERSION 3.16)
project(mimiclaw_host C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(LWS REQUIRED libwebsockets)
pkg_check_modules(CJSON REQUIRED libcjson)

add_executable(mimiclaw-host
    main_host.c

    platform/config_host.c
    platform/path_map_host.c
    platform/platform_http_host.c
    platform/platform_kv_host.c
    platform/message_bus_host.c
    platform/ws_server_host.c
    platform/freertos_host.c
    platform/platform_types_host.c

    ../main/platform/platform_queue.c

    ../main/agent/agent_loop.c
    ../main/agent/context_builder.c
    ../main/memory/memory_store.c
    ../main/memory/session_mgr.c
    ../main/llm/llm_proxy.c
    ../main/tools/tool_registry.c
    ../main/tools/tool_web_search.c
    ../main/tools/tool_get_time.c
    ../main/tools/tool_files.c
)

target_compile_definitions(mimiclaw-host PRIVATE MIMI_HOST_BUILD=1)

target_include_directories(mimiclaw-host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${LWS_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_compile_options(mimiclaw-host PRIVATE
    ${LWS_CFLAGS_OTHER}
    ${CJSON_CFLAGS_OTHER}
)

target_link_directories(mimiclaw-host PRIVATE
    ${LWS_LIBRARY_DIRS}
    ${CJSON_LIBRARY_DIRS}
)

target_link_libraries(mimiclaw-host PRIVATE
    Threads::Threads
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    ${LWS_LIBRARIES}
    ${CJSON_LIBRARIES}
)

add_executable(path_map_host_test
    tests/path_map_host_test.c
    platform/path_map_host.c
    platform/platform_types_host.c
)

target_compile_definitions(path_map_host_test PRIVATE MIMI_HOST_BUILD=1)

target_include_directories(path_map_host_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

add_test(NAME path_map_host_test COMMAND path_map_host_test)
