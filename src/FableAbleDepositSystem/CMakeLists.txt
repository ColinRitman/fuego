# Copyright (c) 2017-2025 Elderfire Privacy Council
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

set(FABLE_ABLE_DEPOSIT_SYSTEM_SOURCES
  FableAbleDepositManager.cpp
  FableAbleStabilityValidator.cpp
)

set(FABLE_ABLE_DEPOSIT_SYSTEM_HEADERS
  ../include/FableAbleDepositSystem.h
)

# Create the FableAbleDepositSystem library
add_library(FableAbleDepositSystem STATIC
  ${FABLE_ABLE_DEPOSIT_SYSTEM_SOURCES}
  ${FABLE_ABLE_DEPOSIT_SYSTEM_HEADERS}
)

# Link with required libraries
target_link_libraries(FableAbleDepositSystem
  PRIVATE
    Common
    Crypto
    Logging
    Serialization
)

# Include directories
target_include_directories(FableAbleDepositSystem
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Compiler definitions
target_compile_definitions(FableAbleDepositSystem
  PRIVATE
    FABLE_ABLE_DEPOSIT_SYSTEM_VERSION="1.0.0"
)

# Compiler flags
if(MSVC)
  target_compile_options(FableAbleDepositSystem PRIVATE /W4)
else()
  target_compile_options(FableAbleDepositSystem PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set C++ standard
set_target_properties(FableAbleDepositSystem PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Install the library
install(TARGETS FableAbleDepositSystem
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)

# Install headers
install(FILES ${FABLE_ABLE_DEPOSIT_SYSTEM_HEADERS}
  DESTINATION include
)