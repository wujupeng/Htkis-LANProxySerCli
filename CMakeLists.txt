cmake_minimum_required(VERSION 3.14)
project(lan_proxy_gateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT UNIX)
    message(FATAL_ERROR "This project only supports Linux/Unix platforms")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MAXMINDDB REQUIRED libmaxminddb)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

include(FetchContent)

FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.0
)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(crow)

set(PROXY_CORE_SOURCES
    src/proxy_core/session.cpp
    src/proxy_core/server.cpp
)

set(ROUTE_DECISION_SOURCES
    src/route_decision/route_engine.cpp
    src/route_decision/geoip_resolver.cpp
    src/route_decision/domain_matcher.cpp
    src/route_decision/rule_manager.cpp
    src/route_decision/dns_cache.cpp
)

set(V2RAYN_MANAGER_SOURCES
    src/v2rayn_manager/v2rayn_process.cpp
    src/v2rayn_manager/v2rayn_config.cpp
    src/v2rayn_manager/vm_node_manager.cpp
    src/v2rayn_manager/vm_config_builder.cpp
    src/v2rayn_manager/vm_link_codec.cpp
    src/v2rayn_manager/vm_node_validator.cpp
)

set(WEB_SERVER_SOURCES
    src/web_server/crow_server.cpp
    src/web_server/auth_middleware.cpp
    src/web_server/jwt_util.cpp
    src/web_server/api_router.cpp
    src/web_server/ws_handler.cpp
)

set(USER_MANAGER_SOURCES
    src/user_manager/user_manager.cpp
    src/user_manager/password_hasher.cpp
)

set(LOG_MONITOR_SOURCES
    src/log_monitor/structured_logger.cpp
    src/log_monitor/metrics_collector.cpp
    src/log_monitor/audit_logger.cpp
    src/log_monitor/ws_sink.cpp
)

set(CONFIG_MANAGER_SOURCES
    src/config_manager/config_loader.cpp
)

set(CONCURRENCY_SOURCES
    src/concurrency/ip_affinity_table.cpp
    src/concurrency/node_health_checker.cpp
    src/concurrency/load_balancer.cpp
    src/concurrency/rate_limiter.cpp
    src/concurrency/overload_protector.cpp
    src/concurrency/connection_registry.cpp
    src/concurrency/connection_admitter.cpp
    src/concurrency/failover_handler.cpp
    src/concurrency/timeout_scanner.cpp
    src/concurrency/concurrency_metrics.cpp
    src/concurrency/system_tuner.cpp
)

add_executable(lan_proxy_gateway
    src/main.cpp
    ${PROXY_CORE_SOURCES}
    ${ROUTE_DECISION_SOURCES}
    ${V2RAYN_MANAGER_SOURCES}
    ${WEB_SERVER_SOURCES}
    ${USER_MANAGER_SOURCES}
    ${LOG_MONITOR_SOURCES}
    ${CONFIG_MANAGER_SOURCES}
    ${CONCURRENCY_SOURCES}
)

target_include_directories(lan_proxy_gateway PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${MAXMINDDB_INCLUDE_DIRS}
)

target_compile_definitions(lan_proxy_gateway PRIVATE
    ASIO_STANDALONE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO
    CROW_DISABLE_STATIC_DIR
    CROW_STATIC_DIRECTORY="/opt/lan-proxy-gateway/web/"
)

target_link_libraries(lan_proxy_gateway PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    ${MAXMINDDB_LIBRARIES}
    Threads::Threads
    Crow::Crow
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

target_compile_options(lan_proxy_gateway PRIVATE -Wall -Wextra -Wpedantic)
