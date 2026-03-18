# SPDX-License-Identifier: LicenseRef-CSSL-1.0

include_directories(${SRC_TOP_DIR}/api-server/api)
include_directories(${SRC_TOP_DIR}/api-server/impl)
include_directories(${SRC_TOP_DIR}/api-server/)
include_directories(${SRC_TOP_DIR}/common)
include_directories(${SRC_TOP_DIR}/common/msg)
include_directories(${SRC_TOP_DIR}/common/utils)
include_directories(${SRC_TOP_DIR}/common/bstr)
include_directories(${SRC_TOP_DIR}/common/nas)
include_directories(${SRC_TOP_DIR}/itti)
include_directories(${SRC_TOP_DIR}/pfcp)
include_directories(${SRC_TOP_DIR}/smf)
include_directories(${SRC_TOP_DIR}/udp)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/logger)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/smf)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/udm)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/ies)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/common)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/utils)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/5gsm-msgs)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/libngap)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/libngap_ies)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/ngap_ies)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/config/config.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils/utils.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/common/common.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/http/http.cmake)

include(${BUILD_TOP_DIR}/smf/used_models.cmake)

add_library (SMF STATIC
  smf_app.cpp
  smf_config.cpp
  smf_context.cpp
  smf_n1.cpp
  smf_n2.cpp
  smf_pfcp_association.cpp
  smf_pco.cpp
  smf_procedure.cpp
  smf_n4.cpp
  smf_n7.cpp
  smf_sbi.cpp
  smf_event.cpp
  smf_profile.cpp
  smf_subscription.cpp
  smf_msg.cpp
  smf_config_types.cpp
  smf_qos_upf_edge.cpp
  session_handler.cpp
)

