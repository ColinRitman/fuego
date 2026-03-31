# CMakeLists.txt for Fuego CryptoNote Core library
# Copyright (c) 2017-2026 Fuego Developers
# Copyright (c) 2018-2019 Conceal Network & Conceal Devs
# Copyright (c) 2016-2019 The Karbowanec developers
# Copyright (c) 2012-2018 The CryptoNote developers
#
# This file is part of Fuego.
#
# Fuego is free software distributed in the hope that it
# will be useful, but WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You can redistribute it and/or modify it under the terms
# of the GNU General Public License v3 or later versions as published
# by the Free Software Foundation. Fuego includes elements written
# by third parties. See file labeled LICENSE for more details.
# You should have received a copy of the GNU General Public License
# along with Fuego. If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.16)
project(FuegoCore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Boost and JSONCPP
find_package(Boost 1.74.0 REQUIRED COMPONENTS system filesystem thread date_time chrono regex serialization program_options coroutine context atomic rt)
find_package(jsoncpp REQUIRED)
find_package(ICU REQUIRED)

# AES-NI instructions enabled for Crypto library
option(ENABLE_AES_NI "Enable AES-NI instructions for Crypto library" ON)
if(ENABLE_AES_NI)
    set(CRYPTO_FLAGS "-maes -mpclmul")
    set(CRYPTO_FLAGS "${CRYPTO_FLAGS} -msse4.2")
    add_definitions(-DENABLE_AES_NI)
endif()

# Global Flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wno-deprecated-builtins")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3")

# Check for supported compiler features
check_cxx_compiler_flag("-Wno-deprecated-builtins" HAS_DEPRECATED_BUILTINS)
if (HAS_DEPRECATED_BUILTINS)
  message(STATUS "C++ Standard: 17")
  message(STATUS "C++ Flags:  -std=c++17 -Wno-deprecated-builtins")
else()
  message(STATUS "C++ Standard: 17")
  message(STATUS "C++ Flags:  -std=c++17")
endif()

message(STATUS "Boost Found: ${Boost_INCLUDE_DIRS}")
message(STATUS "Found jsoncpp via find_package")
message(STATUS "Found jsoncpp headers at ${JSONCPP_INCLUDE_DIR}")
message(STATUS "jsoncpp include dirs: ${JSONCPP_INCLUDE_DIR}")
message(STATUS "jsoncpp libraries: ${JSONCPP_LIBRARIES}")
message(STATUS "jsoncpp library dirs: ${JSONCPP_LIBRARY_DIRS}")
message(STATUS "Boost libraries: ${Boost_LIBRARIES}")

message(STATUS "Found the following ICU libraries:
  data (required)
  i18n (required)
  uc (required)")
message(STATUS "Found ICU includes: ${ICU_INCLUDE_DIRS}")
message(STATUS "Found ICU libraries: ${ICU_LIBRARIES}")
message(STATUS "ICU version: ${ICU_VERSION}")


# Add source files
file(GLOB_RECURSE WALLET_SOURCES src/Wallet/WalletLegacy/*.cpp src/Wallet/WalletGreen/*.cpp src/Wallet/WalletTransactionSender/*.cpp src/Wallet/*.cpp src/SimpleWallet/*.cpp)
file(GLOB_RECURSE COMMON_SOURCES src/Common/*.cpp)
file(GLOB_RECURSE CRYPTO_SOURCES src/crypto/*.cpp)
file(GLOB_RECURSE CORE_SOURCES src/CryptoNoteCore/*.cpp src/CryptoNoteCore/BlockchainExplorer/*.cpp src/CryptoNoteCore/ConnectivityTool/*.cpp src/CryptoNoteCore/FuegoTor/*.cpp src/CryptoNoteCore/HTTP/*.cpp src/CryptoNoteCore/JsonRpcServer/*.cpp src/CryptoNoteCore/Logging/*.cpp src/CryptoNoteCore/Miner/*.cpp src/CryptoNoteCore/Mnemonics/*.cpp src/CryptoNoteCore/NodeRpcProxy/*.cpp src/CryptoNoteCore/Optimizer/*.cpp src/CryptoNoteCore/P2p/*.cpp src/CryptoNoteCore/PaymentGate/*.cpp src/CryptoNoteCore/PaymentGateService/*.cpp src/CryptoNoteCore/Platform/*.cpp src/CryptoNoteCore/Rpc/*.cpp src/CryptoNoteCore/Serialization/*.cpp src/CryptoNoteCore/System/*.cpp src/CryptoNoteCore/TestnetDaemon/*.cpp src/CryptoNoteCore/TestnetWallet/*.cpp src/CryptoNoteCore/Transfers/*.cpp src/CryptoNoteCore/WalletLegacy/*.cpp src/CryptoNoteCore/Wallet/*.cpp src/CryptoNoteCore/WalletGreen/*.cpp src/External/httplib.cpp)

# Target for the main daemon executable
add_executable(fuego-node ${CORE_SOURCES} ${CRYPTO_SOURCES} ${COMMON_SOURCES} ${TEST_SOURCES} ${WALLET_SOURCES})

# Target for the wallet executable
add_executable(fuego-wallet ${CORE_SOURCES} ${CRYPTO_SOURCES} ${COMMON_SOURCES} ${WALLET_SOURCES})

# Example of adding subdirectories (if they contain their own CMakeLists.txt)


target_link_libraries(fuego-wallet PRIVATE
    Boost::system Boost::filesystem Boost::thread Boost::date_time Boost::chrono Boost::regex Boost::serialization Boost::program_options Boost::coroutine Boost::context Boost::atomic
    jsoncpp::jsoncpp
    ICU::uc;ICU::i18n;ICU::data
    Threads::Threads
    CryptoNote::CryptoNoteCore
)

# Add include directories
target_include_directories(fuego-node PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_include_directories(fuego-wallet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Add definitions if needed
target_compile_definitions(fuego-node PUBLIC USE_UNSIGNED_HARDFORK)
target_compile_definitions(fuego-wallet PUBLIC USE_UNSIGNED_HARDFORK)

# Example of adding subdirectories (if they contain their own CMakeLists.txt)
# add_subdirectory(FuegoTor)
# add_subdirectory(ConnectivityTool)
# add_subdirectory (HTTP)
# add_subdirectory(JsonRpcServer)
# add_subdirectory(Logging)
# add_subdirectory(Miner)
# add_subdirectory(Mnemonics)
# add_subdirectory(NodeRpcProxy)
# add_subdirectory(Optimizer)
# add_subdirectory(P2p)
# add_subdirectory(Platform)
# add_subdirectory(Rpc)
# add_subdirectory(Serialization)
# add_subdirectory(System)
# add_subdirectory(TestnetDaemon)
# add_subdirectory(TestnetWallet)
# add_subdirectory(Transfers)
# add_subdirectory(Wallet)
# add_subdirectory(WalletLegacy)
# add_subdirectory(SwapDaemon)
# add_subdirectory(External/httplib)

# Target for the wallet executable (using wallet sources)
add_executable(fuego-wallet ${WALLET_SOURCES}) # wallet sources are added here

target_link_libraries(fuego-wallet PRIVATE
    Boost::system Boost::filesystem Boost::thread Boost::date_time Boost::chrono Boost::regex Boost::serialization Boost::program_options Boost::coroutine Boost::context Boost::atomic
    jsoncpp::jsoncpp
    ICU::uc;ICU::i18n;ICU::data
    Threads::Threads
    CryptoNote::CryptoNoteCore
)

# Example of adding subdirectories (if they contain their own CMakeLists.txt)
# add_subdirectory(EldernodeIndexManager)
# add_subdirectory(BurnDepositValidationService)
# add_subdirectory(FuegoTor)
# add_subdirectory(ConnectivityTool)
# add_subdirectory (HTTP)
# add_subdirectory(JsonRpcServer)
# add_subdirectory(Logging)
# add_subdirectory(Miner)
# add_subdirectory(Mnemonics)
# add_subdirectory(NodeRpcProxy)
# add_subdirectory(Optimizer)
# add_subdirectory(P2p)
# add_subdirectory(Platform)
# add_subdirectory(Rpc)
# add_subdirectory(Serialization)
# add_subdirectory(System)
# add_subdirectory(TestnetDaemon)
# add_subdirectory(TestnetWallet)
# add_subdirectory(Transfers)
# add_subdirectory(Wallet)
# add_subdirectory(WalletLegacy)
# add_subdirectory(SwapDaemon)
# add_subdirectory(External/httplib)

# Add include directories
target_include_directories(fuego-wallet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Add definitions if needed
target_compile_definitions(fuego-wallet PUBLIC USE_UNSIGNED_HARDFORK)
