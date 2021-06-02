/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this
 * file except in compliance with the License. You may obtain a copy of the
 * License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file flexcn_config.hpp
 * \brief
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2019
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr
 */

#ifndef FILE_FLEXCN_CONFIG_HPP_SEEN
#define FILE_FLEXCN_CONFIG_HPP_SEEN

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <libconfig.h++>
#include <mutex>
#include <vector>
#include "thread_sched.hpp" //comon/utils/

#include "3gpp_29.244.h" // common
#include "pfcp.hpp" //pfcp
#include "smf.h"

#define FLEXCN_CONFIG_STRING_SMF_CONFIG "FLEXCN"
#define FLEXCN_CONFIG_STRING_PID_DIRECTORY "PID_DIRECTORY"
#define FLEXCN_CONFIG_STRING_INSTANCE "INSTANCE"
#define FLEXCN_CONFIG_STRING_INTERFACES "INTERFACES"
#define FLEXCN_CONFIG_STRING_INTERFACE_NAME "INTERFACE_NAME"
#define FLEXCN_CONFIG_STRING_IPV4_ADDRESS "IPV4_ADDRESS"
#define FLEXCN_CONFIG_STRING_PORT "PORT"
#define FLEXCN_CONFIG_STRING_INTERFACE_N4 "N4"
#define FLEXCN_CONFIG_STRING_INTERFACE_SBI "SBI"
#define FLEXCN_CONFIG_STRING_SBI_HTTP2_PORT "HTTP2_PORT"
#define FLEXCN_CONFIG_STRING_API_VERSION "API_VERSION"

#define FLEXCN_CONFIG_STRING_IP_ADDRESS_POOL "IP_ADDRESS_POOL"
#define FLEXCN_CONFIG_STRING_ARP_UE "ARP_UE"
#define FLEXCN_CONFIG_STRING_ARP_UE_CHOICE_NO "NO"
#define FLEXCN_CONFIG_STRING_ARP_UE_CHOICE_LINUX "LINUX"
#define FLEXCN_CONFIG_STRING_ARP_UE_CHOICE_OAI "OAI"
#define FLEXCN_CONFIG_STRING_IPV4_ADDRESS_LIST "IPV4_LIST"
#define FLEXCN_CONFIG_STRING_IPV6_ADDRESS_LIST "IPV6_LIST"
#define FLEXCN_CONFIG_STRING_RANGE "RANGE"
#define FLEXCN_CONFIG_STRING_PREFIX "PREFIX"
#define FLEXCN_CONFIG_STRING_IPV4_ADDRESS_RANGE_DELIMITER "-"
#define FLEXCN_CONFIG_STRING_IPV6_ADDRESS_PREFIX_DELIMITER "/"
#define FLEXCN_CONFIG_STRING_DEFAULT_DNS_IPV4_ADDRESS "DEFAULT_DNS_IPV4_ADDRESS"
#define FLEXCN_CONFIG_STRING_DEFAULT_DNS_SEC_IPV4_ADDRESS                         \
  "DEFAULT_DNS_SEC_IPV4_ADDRESS"
#define FLEXCN_CONFIG_STRING_DEFAULT_DNS_IPV6_ADDRESS "DEFAULT_DNS_IPV6_ADDRESS"
#define FLEXCN_CONFIG_STRING_DEFAULT_DNS_SEC_IPV6_ADDRESS                         \
  "DEFAULT_DNS_SEC_IPV6_ADDRESS"
#define FLEXCN_CONFIG_STRING_UE_MTU "UE_MTU"

#define FLEXCN_CONFIG_STRING_INTERFACE_DISABLED "none"

#define FLEXCN_CONFIG_STRING_DNN_LIST "DNN_LIST"
#define FLEXCN_CONFIG_STRING_DNN_NI "DNN_NI"
#define FLEXCN_CONFIG_STRING_PDU_SESSION_TYPE "PDU_SESSION_TYPE"
#define FLEXCN_CONFIG_STRING_IPV4_POOL "IPV4_POOL"
#define FLEXCN_CONFIG_STRING_IPV6_POOL "IPV6_POOL"

#define FLEXCN_ABORT_ON_ERROR true
#define FLEXCN_WARN_ON_ERROR false

#define FLEXCN_CONFIG_STRING_SCHED_PARAMS "SCHED_PARAMS"
#define FLEXCN_CONFIG_STRING_THREAD_RD_CPU_ID "CPU_ID"
#define FLEXCN_CONFIG_STRING_THREAD_RD_SCHED_POLICY "SCHED_POLICY"
#define FLEXCN_CONFIG_STRING_THREAD_RD_SCHED_PRIORITY "SCHED_PRIORITY"

#define FLEXCN_CONFIG_STRING_ITTI_TASKS "ITTI_TASKS"
#define FLEXCN_CONFIG_STRING_ITTI_TIMER_SCHED_PARAMS "ITTI_TIMER_SCHED_PARAMS"
#define FLEXCN_CONFIG_STRING_S11_SCHED_PARAMS "S11_SCHED_PARAMS"
#define FLEXCN_CONFIG_STRING_N4_SCHED_PARAMS "N4_SCHED_PARAMS"
#define FLEXCN_CONFIG_STRING_FLEXCN_APP_SCHED_PARAMS "FLEXCN_APP_SCHED_PARAMS"
#define FLEXCN_CONFIG_STRING_ASYNC_CMD_SCHED_PARAMS "ASYNC_CMD_SCHED_PARAMS"

#define FLEXCN_CONFIG_STRING_AMF "AMF"
#define FLEXCN_CONFIG_STRING_AMF_IPV4_ADDRESS "IPV4_ADDRESS"
#define FLEXCN_CONFIG_STRING_AMF_PORT "PORT"
#define FLEXCN_CONFIG_STRING_UDM "UDM"
#define FLEXCN_CONFIG_STRING_UDM_IPV4_ADDRESS "IPV4_ADDRESS"
#define FLEXCN_CONFIG_STRING_UDM_PORT "PORT"

#define FLEXCN_CONFIG_STRING_UPF_LIST "UPF_LIST"
#define FLEXCN_CONFIG_STRING_UPF_IPV4_ADDRESS "IPV4_ADDRESS"

#define FLEXCN_CONFIG_STRING_NRF "NRF"
#define FLEXCN_CONFIG_STRING_NRF_IPV4_ADDRESS "IPV4_ADDRESS"
#define FLEXCN_CONFIG_STRING_NRF_PORT "PORT"

#define FLEXCN_CONFIG_STRING_LOCAL_CONFIGURATION "LOCAL_CONFIGURATION"
#define FLEXCN_CONFIG_STRING_SESSION_MANAGEMENT_SUBSCRIPTION_LIST                 \
  "SESSION_MANAGEMENT_SUBSCRIPTION_LIST"
#define FLEXCN_CONFIG_STRING_NSSAI_SST "NSSAI_SST"
#define FLEXCN_CONFIG_STRING_NSSAI_SD "NSSAI_SD"
#define FLEXCN_CONFIG_STRING_DNN "DNN"
#define FLEXCN_CONFIG_STRING_DEFAULT_SESSION_TYPE "DEFAULT_SESSION_TYPE"
#define FLEXCN_CONFIG_STRING_DEFAULT_SSC_MODE "DEFAULT_SSC_MODE"
#define FLEXCN_CONFIG_STRING_QOS_PROFILE_5QI "QOS_PROFILE_5QI"
#define FLEXCN_CONFIG_STRING_QOS_PROFILE_PRIORITY_LEVEL                           \
  "QOS_PROFILE_PRIORITY_LEVEL"
#define FLEXCN_CONFIG_STRING_QOS_PROFILE_ARP_PRIORITY_LEVEL                       \
  "QOS_PROFILE_ARP_PRIORITY_LEVEL"
#define FLEXCN_CONFIG_STRING_QOS_PROFILE_ARP_PREEMPTCAP                           \
  "QOS_PROFILE_ARP_PREEMPTCAP"
#define FLEXCN_CONFIG_STRING_QOS_PROFILE_ARP_PREEMPTVULN                          \
  "QOS_PROFILE_ARP_PREEMPTVULN"
#define FLEXCN_CONFIG_STRING_SESSION_AMBR_UL "SESSION_AMBR_UL"
#define FLEXCN_CONFIG_STRING_SESSION_AMBR_DL "SESSION_AMBR_DL"

#define FLEXCN_CONFIG_STRING_SUPPORT_FEATURES "SUPPORT_FEATURES"
#define FLEXCN_CONFIG_STRING_SUPPORT_FEATURES_REGISTER_NRF "REGISTER_NRF"
#define FLEXCN_CONFIG_STRING_SUPPORT_FEATURES_DISCOVER_UPF "DISCOVER_UPF"
#define FLEXCN_CONFIG_STRING_SUPPORT_FEATURES_USE_LOCAL_SUBSCRIPTION_INFO         \
  "USE_LOCAL_SUBSCRIPTION_INFO"
#define FLEXCN_CONFIG_STRING_NAS_FORCE_PUSH_PCO                                   \
  "FORCE_PUSH_PROTOCOL_CONFIGURATION_OPTIONS"

#define FLEXCN_MAX_ALLOCATED_PDN_ADDRESSES 1024

namespace flexcn {

typedef struct interface_cfg_s {
  std::string if_name;
  struct in_addr addr4;
  struct in_addr network4;
  struct in6_addr addr6;
  unsigned int mtu;
  unsigned int port;
  util::thread_sched_params thread_rd_sched_params;
} interface_cfg_t;

typedef struct itti_cfg_s {
  util::thread_sched_params itti_timer_sched_params;
  util::thread_sched_params n4_sched_params;
  util::thread_sched_params smf_app_sched_params;
  util::thread_sched_params async_cmd_sched_params;
} itti_cfg_t;

class flexcn_config {
 private:
  int load_itti(const libconfig::Setting& itti_cfg, itti_cfg_t& cfg);
  int load_interface(const libconfig::Setting& if_cfg, interface_cfg_t& cfg);
  int load_thread_sched_params(
      const libconfig::Setting& thread_sched_params_cfg,
      util::thread_sched_params& cfg);

 public:
  /* Reader/writer lock for this configuration */
  std::mutex m_rw_lock;
  std::string pid_dir;
  unsigned int instance = 0;

  interface_cfg_t n4;
  interface_cfg_t sbi;
  unsigned int sbi_http2_port;
  std::string sbi_api_version;
  itti_cfg_t itti;

  struct in_addr default_dnsv4;
  struct in_addr default_dns_secv4;
  struct in6_addr default_dnsv6;
  struct in6_addr default_dns_secv6;

  // computed from config, UE IP adresses that matches
  // ue_pool_network[]/ue_pool_netmask[] but do not match ue_pool_range_low[] -
  // ue_pool_range_high[]
  // The problem here is that OpenFlow do not deal with ip ranges but with
  // netmasks

  bool force_push_pco;
  uint ue_mtu;

  bool register_nrf;
  bool discover_upf;
  bool use_local_subscription_info;

  struct {
    struct in_addr ipv4_addr;
    unsigned int port;
    std::string api_version;
  } amf_addr;

  struct {
    struct in_addr ipv4_addr;
    unsigned int port;
    std::string api_version;
  } udm_addr;

  std::vector<pfcp::node_id_t> upfs;

  struct {
    struct in_addr ipv4_addr;
    unsigned int port;
    std::string api_version;
  } nrf_addr;

#define SMF_NUM_SESSION_MANAGEMENT_SUBSCRIPTION_MAX 5
  struct {
    snssai_t single_nssai;
    std::string session_type;
    uint8_t ssc_mode;
    subscribed_default_qos_t default_qos;
    session_ambr_t session_ambr;
  } session_management_subscription
      [SMF_NUM_SESSION_MANAGEMENT_SUBSCRIPTION_MAX];
  uint8_t num_session_management_subscription;

  flexcn_config()
      : m_rw_lock(),
        pid_dir(),
        instance(0),
        n4(),
        sbi(),
        itti(),
        upfs() {
    default_dnsv4.s_addr     = INADDR_ANY;
    default_dns_secv4.s_addr = INADDR_ANY;
    default_dnsv6            = in6addr_any;
    default_dns_secv6        = in6addr_any;

    force_push_pco = true;
    ue_mtu         = 1500;

    itti.itti_timer_sched_params.sched_priority = 85;
    itti.n4_sched_params.sched_priority         = 84;
    itti.smf_app_sched_params.sched_priority    = 84;
    itti.async_cmd_sched_params.sched_priority  = 84;

    n4.thread_rd_sched_params.sched_priority = 90;
    n4.port                                  = pfcp::default_port;

    sbi.thread_rd_sched_params.sched_priority = 90;
    sbi.port                                  = 80;

    amf_addr.ipv4_addr.s_addr = INADDR_ANY;
    amf_addr.port             = 80;
    amf_addr.api_version      = "v1";
    udm_addr.ipv4_addr.s_addr = INADDR_ANY;
    udm_addr.port             = 80;
    udm_addr.api_version      = "v1";

    nrf_addr.ipv4_addr.s_addr = INADDR_ANY;
    nrf_addr.port             = 80;
    nrf_addr.api_version      = "v1";

    num_session_management_subscription = 0;

    for (int i = 0; i < SMF_NUM_SESSION_MANAGEMENT_SUBSCRIPTION_MAX; i++) {
      session_management_subscription[i] = {};
    }
    sbi_http2_port  = 8080;
    sbi_api_version = "v1";

    use_local_subscription_info = false;
    register_nrf                = false;
    discover_upf                = false;
  };
  ~flexcn_config();
  void lock() { m_rw_lock.lock(); };
  void unlock() { m_rw_lock.unlock(); };
  int load(const std::string& config_file);
  int finalize();
  void display();
  int get_pfcp_node_id(pfcp::node_id_t& node_id);
  int get_pfcp_fseid(pfcp::fseid_t& fseid);

};

}  // namespace flexcn

#endif /* FILE_SMF_CONFIG_HPP_SEEN */
