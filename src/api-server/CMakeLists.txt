# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(SMF_API_SERVER_DIR "${SRC_TOP_DIR}/api-server")

include_directories(${SMF_API_SERVER_DIR}/api)
include_directories(${SMF_API_SERVER_DIR}/impl)
include_directories(${SMF_API_SERVER_DIR}/)
include_directories(${SRC_TOP_DIR}/smf_app)
include_directories(${SRC_TOP_DIR}/itti)
include_directories(${SRC_TOP_DIR}/common)
include_directories(${SRC_TOP_DIR}/common/msg)
include_directories(${SRC_TOP_DIR}/common/utils)
include_directories(${SRC_TOP_DIR}/common/utils/bstr)
include_directories(${SRC_TOP_DIR}/common/nas)
include_directories(${SRC_TOP_DIR}/pfcp)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/pfcp)
include_directories(${SRC_TOP_DIR}/udp)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/logger)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/http)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/ies)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/common)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/utils)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/5gsm-msgs)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/config/config.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/common_model/common_model.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/smf/smf_model.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/nrf/nrf_model.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/smf/smf_model.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/pcf/pcf_model.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/udm/udm.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils/utils.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/common/common.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/http/http.cmake)

file(GLOB SMF_API_SERVER_src_files
    ${SMF_API_SERVER_DIR}/smf-http2-server.cpp
    ${SMF_API_SERVER_DIR}/smf-api-server.cpp
    ${SMF_API_SERVER_DIR}/api/*.cpp
    ${SMF_API_SERVER_DIR}/impl/*.cpp
)

add_library(SMF_API STATIC
           ${SMF_API_SERVER_src_files})
