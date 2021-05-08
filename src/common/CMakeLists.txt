################################################################################
# Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The OpenAirInterface Software Alliance licenses this file to You under
# the OAI Public License, Version 1.1  (the "License"); you may not use this file
# except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.openairinterface.org/?page_id=698
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#-------------------------------------------------------------------------------
# For more information about the OpenAirInterface (OAI) Software Alliance:
#      contact@openairinterface.org
################################################################################

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/msg)
# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/nas)
# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ngap)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common/utils)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${SRC_TOP_DIR}/../build/ext/spdlog/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

add_library(3GPP_COMMON_TYPES STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/logger.cpp
)

file(GLOB COMMON_src_files
    ${SRC_TOP_DIR}/common/utils/*.c
    ${SRC_TOP_DIR}/common/utils/bstr/*.c
    ${SRC_TOP_DIR}/common/*.c
)   

include_directories(${SRC_TOP_DIR}/common)
include_directories(${SRC_TOP_DIR}/common/msg)
include_directories(${SRC_TOP_DIR}/common/utils)
include_directories(${SRC_TOP_DIR}/common/utils/bstr)
include_directories(${SRC_TOP_DIR}/common/nas)

# TODO: Determine if we need nettle lib and cryto lib here
# They are added here just by "history" reasons.
add_library(COMMON ${COMMON_src_files} ${NETTLE_LIBRARIES} ${CRYPTO_LIBRARIES})

