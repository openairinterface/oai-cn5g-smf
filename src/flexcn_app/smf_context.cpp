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

/*! \file smf_context.cpp
 \brief
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2019
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr
 */

#include "smf_context.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>

#include "3gpp_24.501.h" //common
#include "3gpp_29.500.h" //common
#include "3gpp_29.502.h" //common 
#include "SmContextCreatedData.h"
#include "itti.hpp"
#include "logger.hpp"
#include "flexcn_app.hpp"
#include "flexcn_config.hpp"
#include "smf_event.hpp"
#include "smf_sbi.hpp"
#include "smf_pfcp_association.hpp"
#include "smf_procedure.hpp"
#include "3gpp_conversions.hpp"
#include "string.hpp"

extern "C" {
#include "dynamic_memory_check.h"
}

using namespace flexcn;

extern itti_mw* itti_inst;
extern flexcn::flexcn_app* flexcn_app_inst;
extern flexcn::flexcn_config flexcn_cfg;

//------------------------------------------------------------------------------
void smf_qos_flow::mark_as_released() {
  released = true;
}

//------------------------------------------------------------------------------
std::string smf_qos_flow::toString() const {
  std::string s = {};
  s.append("QoS Flow:\n");
  s.append("\tQFI:\t\t\t\t")
      .append(std::to_string((uint8_t) qfi.qfi))
      .append("\n");
  s.append("\tUL FTEID:\t\t").append(ul_fteid.toString()).append("\n");
  s.append("\tDL FTEID:\t\t").append(dl_fteid.toString()).append("\n");
  s.append("\tPDR ID UL:\t\t\t")
      .append(std::to_string(pdr_id_ul.rule_id))
      .append("\n");
  s.append("\tPDR ID DL:\t\t\t")
      .append(std::to_string(pdr_id_dl.rule_id))
      .append("\n");

  s.append("\tPrecedence:\t\t\t")
      .append(std::to_string(precedence.precedence))
      .append("\n");
  if (far_id_ul.first) {
    s.append("\tFAR ID UL:\t\t\t")
        .append(std::to_string(far_id_ul.second.far_id))
        .append("\n");
  }
  if (far_id_dl.first) {
    s.append("\tFAR ID DL:\t\t\t")
        .append(std::to_string(far_id_dl.second.far_id))
        .append("\n");
  }
  return s;
}
//------------------------------------------------------------------------------
void smf_qos_flow::deallocate_ressources() {
  clear();
  Logger::flexcn_app().info(
      "Resources associated with this QoS Flow (%d) have been released",
      (uint8_t) qfi.qfi);
}

//------------------------------------------------------------------------------
void smf_pdu_session::set(const paa_t& paa) {
  switch (paa.pdu_session_type.pdu_session_type) {
    case PDU_SESSION_TYPE_E_IPV4:
      ipv4                              = true;
      ipv6                              = false;
      ipv4_address                      = paa.ipv4_address;
      pdu_session_type.pdu_session_type = paa.pdu_session_type.pdu_session_type;
      break;
    case PDU_SESSION_TYPE_E_IPV6:
      ipv4                              = false;
      ipv6                              = true;
      ipv6_address                      = paa.ipv6_address;
      pdu_session_type.pdu_session_type = paa.pdu_session_type.pdu_session_type;
      break;
    case PDU_SESSION_TYPE_E_IPV4V6:
      ipv4                              = true;
      ipv6                              = true;
      ipv4_address                      = paa.ipv4_address;
      ipv6_address                      = paa.ipv6_address;
      pdu_session_type.pdu_session_type = paa.pdu_session_type.pdu_session_type;
      break;
    case PDU_SESSION_TYPE_E_UNSTRUCTURED:
    case PDU_SESSION_TYPE_E_ETHERNET:
    case PDU_SESSION_TYPE_E_RESERVED:
      ipv4                              = false;
      ipv6                              = false;
      pdu_session_type.pdu_session_type = paa.pdu_session_type.pdu_session_type;
      break;
    default:
      Logger::flexcn_app().error(
          "flexcn_pdu_session::set(paa_t) Unknown PDN type %d",
          paa.pdu_session_type.pdu_session_type);
  }
}

//------------------------------------------------------------------------------
void smf_pdu_session::get_paa(paa_t& paa) {
  switch (pdu_session_type.pdu_session_type) {
    case PDU_SESSION_TYPE_E_IPV4:
      ipv4             = true;
      ipv6             = false;
      paa.ipv4_address = ipv4_address;
      break;
    case PDU_SESSION_TYPE_E_IPV6:
      ipv4             = false;
      ipv6             = true;
      paa.ipv6_address = ipv6_address;
      break;
    case PDU_SESSION_TYPE_E_IPV4V6:
      ipv4             = true;
      ipv6             = true;
      paa.ipv4_address = ipv4_address;
      paa.ipv6_address = ipv6_address;
      break;
    case PDU_SESSION_TYPE_E_UNSTRUCTURED:
    case PDU_SESSION_TYPE_E_ETHERNET:
    case PDU_SESSION_TYPE_E_RESERVED:
      ipv4 = false;
      ipv6 = false;
      break;
    default:
      Logger::flexcn_app().error(
          "flexcn_pdu_session::get_paa (paa_t) Unknown PDN type %d",
          pdu_session_type.pdu_session_type);
  }
  paa.pdu_session_type.pdu_session_type = pdu_session_type.pdu_session_type;
}

void smf_pdu_session::set_seid(const uint64_t& s) {
  seid = s;
}

//------------------------------------------------------------------------------
// TODO check if far_id should be uniq in the UPF or in the context of a pdn
// connection
void smf_pdu_session::generate_far_id(pfcp::far_id_t& far_id) {
  far_id.far_id = far_id_generator.get_uid();
}

//------------------------------------------------------------------------------
void smf_pdu_session::release_far_id(const pfcp::far_id_t& far_id) {
  far_id_generator.free_uid(far_id.far_id);
}

//------------------------------------------------------------------------------
// TODO check if prd_id should be uniq in the UPF or in the context of a pdn
// connection
void smf_pdu_session::generate_pdr_id(pfcp::pdr_id_t& pdr_id) {
  pdr_id.rule_id = pdr_id_generator.get_uid();
}

//------------------------------------------------------------------------------
// TODO check if prd_id should be uniq in the UPF or in the context of a pdn
// connection
void smf_pdu_session::release_pdr_id(const pfcp::pdr_id_t& pdr_id) {
  pdr_id_generator.free_uid(pdr_id.rule_id);
}

//------------------------------------------------------------------------------
void smf_pdu_session::generate_qos_rule_id(uint8_t& rule_id) {
  rule_id = qos_rule_id_generator.get_uid();
}

//------------------------------------------------------------------------------
void smf_pdu_session::release_qos_rule_id(const uint8_t& rule_id) {
  qos_rule_id_generator.free_uid(rule_id);
}

//------------------------------------------------------------------------------
std::string smf_pdu_session::toString() const {
  std::string s     = {};
  smf_qos_flow flow = {};

  s.append("PDN CONNECTION:\n");
  s.append("\tPDN type:\t\t\t")
      .append(pdu_session_type.toString())
      .append("\n");
  if (ipv4)
    s.append("\tPAA IPv4:\t\t\t")
        .append(conv::toString(ipv4_address))
        .append("\n");
  if (ipv6)
    s.append("\tPAA IPv6:\t\t\t")
        .append(conv::toString(ipv6_address))
        .append("\n");
  if (default_qfi.qfi) {
    s.append("\tDefault QFI:\t\t\t")
        .append(std::to_string(default_qfi.qfi))
        .append("\n");
  } else {
    s.append("\tDefault QFI:\t\t\t").append("No QFI available").append("\n");
  }

  s.append("\tSEID:\t\t\t\t").append(std::to_string(seid)).append("\n");

  return s;
}

//------------------------------------------------------------------------------
void smf_pdu_session::set_pdu_session_status(
    const pdu_session_status_e& status) {
  // TODO: Should consider congestion handling
  Logger::flexcn_app().info(
      "Set PDU Session Status to %s",
      pdu_session_status_e2str.at(static_cast<int>(status)).c_str());
  std::unique_lock lock(m_pdu_session_mutex);
  pdu_session_status = status;
}

//------------------------------------------------------------------------------
pdu_session_status_e smf_pdu_session::get_pdu_session_status() const {
  std::shared_lock lock(m_pdu_session_mutex);
  return pdu_session_status;
}

//------------------------------------------------------------------------------
void smf_pdu_session::set_upCnx_state(const upCnx_state_e& state) {
  Logger::flexcn_app().info(
      "Set upCnxState to %s",
      upCnx_state_e2str.at(static_cast<int>(state)).c_str());
  std::unique_lock lock(m_pdu_session_mutex);
  upCnx_state = state;
}

//------------------------------------------------------------------------------
upCnx_state_e smf_pdu_session::get_upCnx_state() const {
  std::shared_lock lock(m_pdu_session_mutex);
  return upCnx_state;
}

//------------------------------------------------------------------------------
pdu_session_type_t smf_pdu_session::get_pdu_session_type() const {
  return pdu_session_type;
}


//------------------------------------------------------------------------------
void smf_pdu_session::mark_qos_rule_to_be_synchronised(const uint8_t rule_id) {

}

//------------------------------------------------------------------------------
void smf_pdu_session::set_amf_addr(const std::string& addr) {
  amf_addr = addr;
}

//------------------------------------------------------------------------------
void smf_pdu_session::get_amf_addr(std::string& addr) const {
  addr = amf_addr;
}

//------------------------------------------------------------------------------
std::string smf_pdu_session::get_amf_addr() const {
  return amf_addr;
}

//------------------------------------------------------------------------------
void session_management_subscription::insert_dnn_configuration(
    const std::string& dnn,
    std::shared_ptr<dnn_configuration_t>& dnn_configuration) {
  std::unique_lock lock(m_mutex);
  dnn_configurations.insert(
      std::pair<std::string, std::shared_ptr<dnn_configuration_t>>(
          dnn, dnn_configuration));
}

//------------------------------------------------------------------------------
void session_management_subscription::find_dnn_configuration(
    const std::string& dnn,
    std::shared_ptr<dnn_configuration_t>& dnn_configuration) const {
  Logger::flexcn_app().info("Find DNN configuration with DNN %s", dnn.c_str());
  std::shared_lock lock(m_mutex);
  if (dnn_configurations.count(dnn) > 0) {
    dnn_configuration = dnn_configurations.at(dnn);
  }
}

//------------------------------------------------------------------------------
bool session_management_subscription::dnn_configuration(
    const std::string& dnn) const {
  std::shared_lock lock(m_mutex);
  if (dnn_configurations.count(dnn) > 0) {
    return true;
  } else {
    return false;
  }
}

//------------------------------------------------------------------------------
void smf_context::insert_procedure(std::shared_ptr<smf_procedure>& sproc) {
  std::unique_lock<std::recursive_mutex> lock(m_context);
  pending_procedures.push_back(sproc);
}

//------------------------------------------------------------------------------
bool smf_context::find_procedure(
    const uint64_t& trxn_id, std::shared_ptr<smf_procedure>& proc) {
  std::unique_lock<std::recursive_mutex> lock(m_context);
  auto found = std::find_if(
      pending_procedures.begin(), pending_procedures.end(),
      [trxn_id](const std::shared_ptr<smf_procedure>& i) -> bool {
        return i->trxn_id == trxn_id;
      });
  if (found != pending_procedures.end()) {
    proc = *found;
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------
void smf_context::remove_procedure(smf_procedure* proc) {
}

//------------------------------------------------------------------------------
void smf_context::handle_itti_msg(
    itti_n4_session_establishment_response& seresp) {
}

//------------------------------------------------------------------------------
void smf_context::handle_itti_msg(
    itti_n4_session_modification_response& smresp) {
}

//------------------------------------------------------------------------------
void smf_context::handle_itti_msg(itti_n4_session_deletion_response& sdresp) {
}

//------------------------------------------------------------------------------
void smf_context::handle_itti_msg(
    std::shared_ptr<itti_n4_session_report_request>& req) {

}

//------------------------------------------------------------------------------
std::string smf_context::toString() const {
  std::unique_lock<std::recursive_mutex> lock(m_context);
  std::string s = {};
  s.append("\n");
  s.append("FLEXCN CONTEXT:\n");
  s.append("\tSUPI:\t\t\t\t")
      .append(smf_supi_to_string(supi).c_str())
      .append("\n");
  return s;
}

//------------------------------------------------------------------------------
void smf_context::get_default_qos(
    const snssai_t& snssai, const std::string& dnn,
    subscribed_default_qos_t& default_qos) {
  
}

//------------------------------------------------------------------------------
void smf_context::insert_dnn_subscription(
    const snssai_t& snssai,
    std::shared_ptr<session_management_subscription>& ss) {
  std::unique_lock<std::recursive_mutex> lock(m_context);

  dnn_subscriptions[(uint8_t) snssai.sST] = ss;
  Logger::flexcn_app().info(
      "Inserted DNN Subscription, key: %d", (uint8_t) snssai.sST);
}

//------------------------------------------------------------------------------
void smf_context::insert_dnn_subscription(
    const snssai_t& snssai, const std::string& dnn,
    std::shared_ptr<session_management_subscription>& ss) {
  
}

//-----------------------------------------------------------------------------
supi_t smf_context::get_supi() const {
  return supi;
}

//-----------------------------------------------------------------------------
void smf_context::set_supi(const supi_t& s) {
  supi = s;
}

//-----------------------------------------------------------------------------
void smf_context::set_scid(const scid_t& id) {
  scid = id;
}

//-----------------------------------------------------------------------------
scid_t smf_context::get_scid() const {
  return scid;
}

//-----------------------------------------------------------------------------
void smf_context::get_supi_prefix(std::string& prefix) const {
  prefix = supi_prefix;
}

//-----------------------------------------------------------------------------
void smf_context::set_supi_prefix(std::string const& prefix) {
  supi_prefix = prefix;
}

//------------------------------------------------------------------------------
void smf_context::set_amf_addr(const std::string& addr) {
  amf_addr = addr;
}

//------------------------------------------------------------------------------
void smf_context::get_amf_addr(std::string& addr) const {
  addr = amf_addr;
}

//------------------------------------------------------------------------------
void smf_context::set_plmn(const plmn_t& plmn) {
  this->plmn = plmn;
}

//------------------------------------------------------------------------------
void smf_context::get_plmn(plmn_t& plmn) const {
  plmn = this->plmn;
}
