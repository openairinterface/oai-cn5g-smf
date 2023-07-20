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

/*! \file smf_config.hpp
 * \brief
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN, Stefan Spettel
 \company Eurecom, phine.tech
 \date 2023
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr,
 stefan.spettel@phine.tech
 */

#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mutex>
#include <vector>
#include "thread_sched.hpp"

#include "3gpp_29.244.h"
#include "pfcp.hpp"
#include "smf.h"
#include "smf_profile.hpp"
#include "config.hpp"
#include "logger_base.hpp"
#include "smf_config_types.hpp"

namespace oai::config::smf {

const std::string USE_LOCAL_PCC_RULES_CONFIG_VALUE = "use_local_pcc_rules";
const std::string USE_LOCAL_SUBSCRIPTION_INFOS_CONFIG_VALUE =
    "use_local_subscription_info";
const std::string USE_EXTERNAL_AUSF_CONFIG_VALUE = "use_external_ausf";
const std::string USE_EXTERNAL_UDM_CONFIG_VALUE  = "use_external_udm";
const std::string USE_EXTERNAL_NSSF_CONFIG_VALUE = "use_external_nssf";

const snssai_t DEFAULT_SNSSAI{1, 0xFFFFFF};
const session_ambr_t DEFAULT_S_AMBR{"1000Mbps", "1000Mbps"};
const std::string DEFAULT_DNN  = "default";
const uint8_t DEFAULT_SSC_MODE = 1;
const subscribed_default_qos_t DEFAULT_QOS{
    9,
    {1, "NOT_PREEMPT", "NOT_PREEMPTABLE"},
    1};

typedef struct interface_cfg_s {
  std::string if_name;
  struct in_addr addr4;
  struct in6_addr addr6;
  unsigned int mtu;
  unsigned int port;
  oai::util::thread_sched_params thread_rd_sched_params;
} interface_cfg_t;

typedef struct itti_cfg_s {
  oai::util::thread_sched_params itti_timer_sched_params;
  oai::util::thread_sched_params n4_sched_params;
  oai::util::thread_sched_params smf_app_sched_params;
  oai::util::thread_sched_params async_cmd_sched_params;
} itti_cfg_t;

typedef struct dnn_s {
  std::string dnn;
  std::string dnn_label;
  struct in_addr ue_pool_range_low;
  struct in_addr ue_pool_range_high;
  struct in6_addr paa_pool6_prefix;
  uint8_t paa_pool6_prefix_len;
  pdu_session_type_t pdu_session_type;
} dnn_t;

class smf_config : public config {
 private:
  // TODO only temporary, to avoid changing all the references to the config in
  // all the calling classes
  void to_smf_config();

  // TODO only temporary, we should not resolve on startup in the config
  static in_addr resolve_nf(const std::string& host);

  void update_used_nfs() override;

 public:
  spdlog::level::level_enum log_level;
  unsigned int instance = 0;
  interface_cfg_t n4;
  interface_cfg_t sbi;
  unsigned int sbi_http2_port;
  std::string sbi_api_version;
  itti_cfg_t itti;

  std::map<std::string, dnn_t> dnns;

  bool force_push_pco;

  bool register_nrf;
  bool discover_upf;
  bool discover_pcf;
  bool use_local_subscription_info;
  bool use_local_pcc_rules;
  unsigned int http_version;
  bool enable_ur;
  bool enable_dl_pdr_in_pfcp_sess_estab;
  std::string local_n3_addr;

  std::vector<pfcp::node_id_t> upfs;

  // Network instance
  // bool network_instance_configuration;
  struct upf_nwi_list_s {
    pfcp::node_id_t upf_id;
    std::string domain_access;
    std::string domain_core;
    //      std::string domain_sgi_lan;
  };
  typedef struct upf_nwi_list_s upf_nwi_list_t;

  std::vector<upf_nwi_list_t> upf_nwi_list;

  smf_config(const std::string& configPath, bool logStdout, bool logRotFile);

  int get_pfcp_node_id(pfcp::node_id_t& node_id);
  int get_pfcp_fseid(pfcp::fseid_t& fseid);
  bool is_dotted_dnn_handled(
      const std::string& dnn, const pdu_session_type_t& pdn_session_type);
  std::string get_default_dnn();

  /**
   * Returns network instance of iface_type typ. If not found, empty string is
   * returned
   * @param node_id IP address or FQDN to match against configuration
   * @return NWI or empty string
   */
  std::string get_nwi(
      const pfcp::node_id_t& node_id, const ::smf::iface_type& type) const;

  /**
   * Returns configured UPF based on node_id.
   * Compares UPF host config value with node_id FQDN and IPv4 address in this
   * order
   * @param node_id PFCP node id, FQDN, IPv4 address must be set
   * @throws std::invalid_argument  in case UPF is not to be found or type is
   * IPv6
   * @return upf
   */
  const oai::config::smf::upf& get_upf(const pfcp::node_id_t& node_id) const;

  /**
   * Returns SMF configuration pointer which stores SMF-specific configuration
   * @return SMF configuration
   */
  std::shared_ptr<smf_config_type> smf() const;

  /**
   * Returns UE DNS from the DNN, returns default DNS if DNN is not found
   * @param dnn
   * @return
   */
  const ue_dns& get_dns_from_dnn(const std::string& dnn);

  bool init() override;
};

}  // namespace oai::config::smf
