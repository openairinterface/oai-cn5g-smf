# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(NAS_DIR "${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas")
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/3gpp)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/ies)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/common)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/5gsm-msgs)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/utils)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/logger)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/common)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/common_model)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils/bstr)

file(GLOB NAS_src_files
  ${NAS_DIR}/common/*.cpp
  ${NAS_DIR}/5gsm-msgs/*.cpp
  ${NAS_DIR}/ies/*.cpp
  ${NAS_DIR}/utils/*.c
  ${NAS_DIR}/utils/*.cpp
)

add_library (NAS ${NAS_src_files})
