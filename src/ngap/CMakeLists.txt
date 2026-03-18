# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(NGAP_DIR "${SRC_TOP_DIR}/ngap")

file(GLOB NGAP_src_files
    ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/libngap/*.c
    ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/libngap_ies/*.c
    ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/ngap_msgs/*.cpp
    ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/ngap_ies/*.cpp
    ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/utils/*.cpp
)

add_library(NGAP ${NGAP_src_files})

target_include_directories(NGAP PUBLIC ${SRC_TOP_DIR}/${MOUNTED_COMMON}/3gpp)
target_include_directories(NGAP PUBLIC ${SRC_TOP_DIR}/${MOUNTED_COMMON}/common)
target_include_directories(NGAP PUBLIC ${SRC_TOP_DIR}/${MOUNTED_COMMON}/logger)
target_include_directories(NGAP PUBLIC ${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/common_model)
target_include_directories(NGAP PUBLIC ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/libngap)
target_include_directories(NGAP PUBLIC ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/libngap_ies)
target_include_directories(NGAP PUBLIC ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/ngap_ies)
target_include_directories(NGAP PUBLIC ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/ngap_msgs)
target_include_directories(NGAP PUBLIC ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/utils)
target_include_directories(NGAP PUBLIC ${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils)
target_include_directories(NGAP PUBLIC ${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils/bstr)
