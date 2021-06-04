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

/*! \file flexcn_context.hpp
 \brief
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2019
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr
 */

#ifndef FILE_SMF_CONTEXT_HPP_SEEN
#define FILE_SMF_CONTEXT_HPP_SEEN

#include <map>
#include <memory>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "3gpp_24.008.h" // common
#include "3gpp_29.244.h" // common
#include "3gpp_29.502.h" // common
#include "3gpp_29.503.h" // common
#include "common_root_types.h" // common
#include "itti.hpp"
#include "msg_pfcp.hpp"
#include "pistache/endpoint.h"
#include "pistache/http.h"
#include "pistache/router.h"
#include "flexcn_event.hpp"
#include "flexcn_procedure.hpp"
#include "uint_generator.hpp" // common

namespace flexcn {

class smf_qos_flow {
 public:
  smf_qos_flow() { clear(); }

  void clear() {
    ul_fteid    = {};
    dl_fteid    = {};
    pdr_id_ul   = {};
    pdr_id_dl   = {};
    precedence  = {};
    far_id_ul   = {};
    far_id_dl   = {};
    released    = false;
    qos_profile = {};
    cause_value = 0;
  }

  /*
   * Release resources associated with this flow
   * @param void
   * @return void
   */
  void deallocate_ressources();

  /*
   * Mark this flow as released
   * @param void
   * @return void
   */
  void mark_as_released();

  /*
   * Represent flow as string to be printed
   * @param void
   * @return void
   */
  std::string toString() const;

  pfcp::qfi_t qfi;           // QoS Flow Identifier
  pfcp::fteid_t ul_fteid;    // fteid of UPF
  pfcp::fteid_t dl_fteid;    // fteid of AN
  pfcp::pdr_id_t pdr_id_ul;  // Packet Detection Rule ID, UL
  pfcp::pdr_id_t pdr_id_dl;  // Packet Detection Rule ID, DL
  pfcp::precedence_t precedence;
  std::pair<bool, pfcp::far_id_t> far_id_ul;  // FAR ID, UL
  std::pair<bool, pfcp::far_id_t> far_id_dl;  // FAR ID, DL
  bool released;  // finally seems necessary, TODO try to find heuristic ?
  pdu_session_id_t pdu_session_id;
  qos_profile_t qos_profile;  // QoS profile
  uint8_t cause_value;        // cause
};

class smf_pdu_session : public std::enable_shared_from_this<smf_pdu_session> {
 public:
  smf_pdu_session() : m_pdu_session_mutex() { clear(); }

  void clear() {
    ipv4                = false;
    ipv6                = false;
    ipv4_address.s_addr = INADDR_ANY;
    ipv6_address        = in6addr_any;
    pdu_session_type    = {};
    seid                = 0;
    up_fseid            = {};
    released           = false;
    default_qfi.qfi    = NO_QOS_FLOW_IDENTIFIER_ASSIGNED;
    pdu_session_status = pdu_session_status_e::PDU_SESSION_INACTIVE;
    timer_T3590        = ITTI_INVALID_TIMER_ID;
    timer_T3591        = ITTI_INVALID_TIMER_ID;
    timer_T3592        = ITTI_INVALID_TIMER_ID;
  }

  smf_pdu_session(smf_pdu_session& b) = delete;

  /*
   * Set UE Address for this session
   * @param [paa_t &] paa: PAA
   * @return void
   */
  void set(const paa_t& paa);

  /*
   * Get UE Address of this session
   * @param [paa_t &] paa: PAA
   * @return void
   */
  void get_paa(paa_t& paa);

  /*
   * Set current status of PDU Session
   * @param [const pdu_session_status_e &] status: status to be set
   * @return void
   */
  void set_pdu_session_status(const pdu_session_status_e& status);

  /*
   * Get current status of PDU Session
   * @param void
   * @return pdu_session_status_e: status of PDU session
   */
  pdu_session_status_e get_pdu_session_status() const;

  /*
   * Set upCnxState for a N3 Tunnel
   * @param [upCnx_state_e&] state: new state of the N3 tunnel
   * @return void
   */
  void set_upCnx_state(const upCnx_state_e& state);

  /*
   * Get upCnxState of a N3 Tunnel
   * @param void
   * @return upCnx_state_e: current state of this N3 tunnel
   */
  upCnx_state_e get_upCnx_state() const;

  // deallocate_ressources is for releasing related-resources prior to the
  // deletion of objects since shared_ptr is actually heavy used for handling
  // objects, deletion of object instances cannot be always guaranteed when
  // removing them from a collection, so that is why actually the deallocation
  // of resources is not done in the destructor of objects.
  void deallocate_ressources(const std::string& dnn);

  /*
   * Represent PDU Session as string to be printed
   * @param void
   * @return void
   */
  std::string toString() const;

  /*
   * Set a value to SEID
   * @param [const uint64_t &] seid: value to be set
   * @return void
   */
  void set_seid(const uint64_t& seid);

  /*
   * Generate a PDR ID
   * @param [pfcp::pdr_id_t &]: pdr_id: PDR ID generated
   * @return void
   */
  void generate_pdr_id(pfcp::pdr_id_t& pdr_id);

  /*
   * Release a PDR ID
   * @param [const pfcp::pdr_id_t &]: pdr_id: PDR ID to be released
   * @return void
   */
  void release_pdr_id(const pfcp::pdr_id_t& pdr_id);

  /*
   * Generate a FAR ID
   * @param [pfcp::far_id_t &]: far_id: FAR ID generated
   * @return void
   */
  void generate_far_id(pfcp::far_id_t& far_id);

  /*
   * Release a FAR ID
   * @param [const pfcp::far_id_t &]: far_id: FAR ID to be released
   * @return void
   */
  void release_far_id(const pfcp::far_id_t& far_id);

  /*
   * Generate a QoS Rule ID
   * @param [uint8_t &]: rule_id: QoS Rule ID generated
   * @return void
   */
  void generate_qos_rule_id(uint8_t& rule_id);

  /*
   * Release a QoS Rule ID
   * @param [uint8_t &]: rule_id: QoS Rule ID to be released
   * @return void
   */
  void release_qos_rule_id(const uint8_t& rule_id);

  /*
   * Mark a QoS Rule to be synchronised with UE
   * @param [const uint8_t ]: rule_id: QoS Rule ID to be synchronised with UE
   * @return void
   */
  void mark_qos_rule_to_be_synchronised(const uint8_t rule_id);

  /*
   * Get PDN Type of this PDU session
   * @param void
   * @return pdu_session_type_t: PDN Type
   */
  pdu_session_type_t get_pdu_session_type() const;

  /*
   * Set AMF Addr of the serving AMF
   * @param [const std::string&] addr: AMF Addr in string representation
   * @return void
   */
  void set_amf_addr(const std::string& addr);

  /*
   * Get AMF Addr of the serving AMF (in string representation)
   * @param [const std::string&] addr: store AMF IP Addr
   * @return void
   */
  void get_amf_addr(std::string& addr) const;
  std::string get_amf_addr() const;

  bool ipv4;  // IP Address(es): IPv4 address and/or IPv6 prefix
  bool ipv6;  // IP Address(es): IPv4 address and/or IPv6 prefix
  struct in_addr
      ipv4_address;  // IP Address(es): IPv4 address and/or IPv6 prefix
  struct in6_addr
      ipv6_address;  // IP Address(es): IPv4 address and/or IPv6 prefix
  pdu_session_type_t pdu_session_type;  // IPv4, IPv6, IPv4v6 or Non-IP

  bool released;  //(release access bearers request)

  //----------------------------------------------------------------------------
  // PFCP related members
  //----------------------------------------------------------------------------
  // PFCP Session
  uint64_t seid;
  pfcp::fseid_t up_fseid;
  //
  util::uint_generator<uint16_t> pdr_id_generator;
  util::uint_generator<uint32_t> far_id_generator;

  uint32_t pdu_session_id;
  std::string amf_id;
  std::string amf_addr;
  pdu_session_status_e pdu_session_status;
  upCnx_state_e
      upCnx_state;  // N3 tunnel status (ACTIVATED, DEACTIVATED, ACTIVATING)
  timer_id_t timer_T3590;
  timer_id_t timer_T3591;
  timer_id_t timer_T3592;

  pfcp::qfi_t default_qfi;                    // Default QFI for this session
  std::vector<uint8_t> qos_rules_to_be_synchronised;
  std::vector<uint8_t> qos_rules_to_be_removed;
  // 5GSM parameters and capabilities
  uint8_t maximum_number_of_supported_packet_filters;
  // TODO: 5GSM Capability (section 9.11.4.1@3GPP TS 24.501 V16.1.0)
  // TODO: Integrity protection maximum data rate (section 9.11.4.7@@3GPP
  // TS 24.501 V16.1.0)
  uint8_t
      number_of_supported_packet_filters;  // number_of_supported_packet_filters
  util::uint_generator<uint32_t> qos_rule_id_generator;

  // Shared lock
  mutable std::shared_mutex m_pdu_session_mutex;
};

class session_management_subscription {
 public:
  session_management_subscription(snssai_t snssai)
      : single_nssai(snssai), dnn_configurations(), m_mutex() {}

  /*
   * Insert a DNN configuration into the subscription
   * @param [std::string] dnn
   * @param [std::shared_ptr<dnn_configuration_t> &] dnn_configuration
   * @return void
   */
  void insert_dnn_configuration(
      const std::string& dnn,
      std::shared_ptr<dnn_configuration_t>& dnn_configuration);

  /*
   * Find a DNN configuration
   * @param [std::string] dnn
   * @param [std::shared_ptr<dnn_configuration_t> &] dnn_configuration
   * @return void
   */
  void find_dnn_configuration(
      const std::string& dnn,
      std::shared_ptr<dnn_configuration_t>& dnn_configuration) const;

  /*
   * Verify whether DNN configuration with a given DNN exist
   * @param [std::string &] dnn
   * @return bool: return true if the configuration exist, otherwise return
   * false
   */
  bool dnn_configuration(const std::string& dnn) const;

 private:
  snssai_t single_nssai;
  std::map<std::string, std::shared_ptr<dnn_configuration_t>>
      dnn_configurations;  // dnn <->dnn_configuration

  // Shared lock
  mutable std::shared_mutex m_mutex;
};

class smf_context;

class smf_context : public std::enable_shared_from_this<smf_context> {
 public:
  smf_context()
      : m_context(),
        pending_procedures(),
        dnn_subscriptions(),
        scid(0),
        event_sub(),
        plmn() {
    supi_prefix = {};
    
  }

  smf_context(smf_context& b) = delete;

  virtual ~smf_context() {
    Logger::flexcn_app().debug("Delete FLEXCN Context instance...");
    // Disconnect the boost connection
    if (sm_context_status_connection.connected())
      sm_context_status_connection.disconnect();
    if (ee_pdu_session_release_connection.connected())
      ee_pdu_session_release_connection.disconnect();
  }


#define IS_FIND_PDN_WITH_LOCAL_TEID true
#define IS_FIND_PDN_WITH_PEER_TEID false

  /*
   * Handle N4 message (session establishment response) from UPF
   * @param [itti_n4_session_establishment_responset&]
   * @return void
   */
  void handle_itti_msg(itti_n4_session_establishment_response&);

  /*
   * Handle N4 message (session modification response) from UPF
   * @param [itti_n4_session_modification_response&]
   * @return void
   */
  void handle_itti_msg(itti_n4_session_modification_response&);

  /*
   * Handle N4 message (session deletion response) from UPF
   * @param [itti_n4_session_deletion_response&]
   * @return void
   */
  void handle_itti_msg(itti_n4_session_deletion_response&);

  /*
   * Handle N4 message (session report) from UPF
   * @param [itti_n4_session_report_request&]
   * @return void
   */
  void handle_itti_msg(std::shared_ptr<itti_n4_session_report_request>&);

  /*
   * Convert all members of this class to string for logging
   * @return std::string
   */
  std::string toString() const;

  /*
   * Set the value for Supi
   * @param [const supi_t&] s
   * @return void
   */
  void set_supi(const supi_t& s);

  /*
   * Get Supi member
   * @param
   * @return supi_t
   */
  supi_t get_supi() const;

  /*
   * Set SM Context ID
   * @param [const scid_t &] id: SM Context Id
   * @return void
   */
  void set_scid(const scid_t& id);

  /*
   * Get SM Context ID
   * @param [void
   * @return scid_t: SM Context Id
   */
  scid_t get_scid() const;

  /*
   * Get Supi prefix
   * @param [const std::string &]  prefix: Supi prefix (e.g., imsi)
   * @return void
   */
  void get_supi_prefix(std::string& prefix) const;

  /*
   * Get Supi prefix
   * @param [const std::string &]  prefix: Supi prefix (e.g., imsi)
   * @return void
   */
  void set_supi_prefix(std::string const& value);

  /*
   * Set AMF Addr of the serving AMF
   * @param [const std::string&] addr: AMF Addr in string representation
   * @return void
   */
  void set_amf_addr(const std::string& addr);

  /*
   * Get AMF Addr of the serving AMF (in string representation)
   * @param [const std::string&] addr: store AMF IP Addr
   * @return void
   */
  void get_amf_addr(std::string& addr) const;

  void set_plmn(const plmn_t& plmn);
  void get_plmn(plmn_t& plmn) const;

 private:
  std::vector<std::shared_ptr<smf_procedure>> pending_procedures;
  // snssai-sst <-> session management subscription
  std::map<uint8_t, std::shared_ptr<session_management_subscription>>
      dnn_subscriptions;
  supi_t supi;
  std::string supi_prefix;
  scid_t scid;  // SM Context ID
  plmn_t plmn;

  // AMF IP addr
  string amf_addr;
  // Big recursive lock
  mutable std::recursive_mutex m_context;

  // for Event Handling
  smf_event event_sub;
  bs2::connection sm_context_status_connection;
  bs2::connection ee_pdu_session_release_connection;
};
}  // namespace flexcn

#endif
