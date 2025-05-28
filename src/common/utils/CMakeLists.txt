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
include_directories(${SRC_TOP_DIR}/common)
include_directories(${SRC_TOP_DIR}/common/msg)
include_directories(${SRC_TOP_DIR}/common/utils)
include_directories(${SRC_TOP_DIR}/common/utils/bstr)
include_directories(${SRC_TOP_DIR}/itti)
include_directories(${SRC_TOP_DIR}/common/nas)
include_directories(${SRC_TOP_DIR}/api-server/model)
include_directories(${SRC_TOP_DIR}/smf_app)
include_directories(${SRC_TOP_DIR}/nas)
include_directories(${SRC_TOP_DIR}/nas/ies)
include_directories(${SRC_TOP_DIR}/nas/sm/msg)
include_directories(${SRC_TOP_DIR}/nas/mm)
include_directories(${SRC_TOP_DIR}/nas/mm/msg)
include_directories(${SRC_TOP_DIR}/nas/sm)
include_directories(${SRC_TOP_DIR}/nas/sm/msg)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/logger)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/ies)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/common)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/utils)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/5gsm-msgs)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/libngap)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/libngap_ies)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/smf)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/config/config.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/common_model/common_model.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/nrf/nrf_model.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/pcf/pcf_model.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/smf/smf_model.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils/utils.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/common/common.cmake)

set(CN_UTILS_SRC STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/smf_3gpp_conversions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/epc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mime_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fqdn.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/async_shell_cmd.cpp
    )

add_library(CN_UTILS ${CN_UTILS_SRC})