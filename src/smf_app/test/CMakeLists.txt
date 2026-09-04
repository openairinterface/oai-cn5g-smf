# SPDX-License-Identifier: LicenseRef-CSSL-1.0
#
# SMF unit tests (GoogleTest + CTest). Configured only when BUILD_TESTING=ON
# (see ../CMakeLists.txt); mirrors src/pcf_app/test/CMakeLists.txt.
#
# NF_TARGET is retargeted to this test executable before including the shared
# *.cmake source lists so their `if (TARGET ${NF_TARGET})` guards attach the
# same model/logger/config/utils/http sources here that they attach to the
# real "smf" executable in oai_smf/CMakeLists.txt.

include(GoogleTest)

set(SMF_TEST_TARGET smf_unit_tests)
set(NF_TARGET ${SMF_TEST_TARGET})

add_executable(${SMF_TEST_TARGET}
  main.cpp
  smf_globals_stub.cpp
  test_smf_policy_manager.cpp
  test_upf_graph.cpp
  test_session_handler.cpp
  test_smf_msg.cpp
  test_pcf_qod_workflow.cpp
  ${SRC_TOP_DIR}/itti/itti.cpp
  ${SRC_TOP_DIR}/itti/itti_msg.cpp
)

include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/logger/logger.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/config/config.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/smf_model.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils/utils.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/common/common.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/http/http.cmake)

target_include_directories(${SMF_TEST_TARGET} PRIVATE
  ${SRC_TOP_DIR}/common/msg
  ${SRC_TOP_DIR}/common/utils
  ${SRC_TOP_DIR}/common/bstr
  ${SRC_TOP_DIR}/common/nas
  ${SRC_TOP_DIR}/api-server/api
  ${SRC_TOP_DIR}/api-server/impl
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/model
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/common
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/3gpp
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/pfcp
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/ies
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/common
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/utils
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas/5gsm-msgs
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/libngap
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/ngap/libngap_ies
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/http
  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/config
)

target_link_libraries(${SMF_TEST_TARGET} PRIVATE
  SMF
  UDP
  PFCP
  SMF_API
  SMF_COMMON_TYPES
  NGAP
  NAS
  gtest gtest_main
  -lnghttp2_asio
  boost_system boost_thread boost_chrono
  ssl crypto
  fmt spdlog
  yaml-cpp
  cpr::cpr curl
  pistache event
  pthread m rt dl
)

gtest_discover_tests(${SMF_TEST_TARGET} PROPERTIES LABELS "unit")
