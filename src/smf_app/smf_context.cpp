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

#include "3gpp_24.501.h"
#include "3gpp_29.500.h"
#include "3gpp_29.502.h"
#include "SmContextCreatedData.h"
#include "itti.hpp"
#include "logger.hpp"
#include "smf_app.hpp"
#include "smf_config.hpp"
#include "smf_event.hpp"
#include "smf_n1.hpp"
#include "smf_sbi.hpp"
#include "smf_n2.hpp"
#include "smf_paa_dynamic.hpp"
#include "smf_pfcp_association.hpp"
#include "smf_procedure.hpp"
#include "3gpp_conversions.hpp"
#include "string.hpp"

extern "C" {
#include "Ngap_AssociatedQosFlowItem.h"
#include "Ngap_GTPTunnel.h"
#include "Ngap_PDUSessionResourceModifyResponseTransfer.h"
#include "Ngap_PDUSessionResourceReleaseResponseTransfer.h"
#include "Ngap_PDUSessionResourceSetupResponseTransfer.h"
#include "Ngap_PDUSessionResourceSetupUnsuccessfulTransfer.h"
#include "Ngap_QosFlowAddOrModifyResponseItem.h"
#include "Ngap_QosFlowAddOrModifyResponseList.h"
#include "dynamic_memory_check.h"
}

using namespace smf;

extern itti_mw* itti_inst;
extern smf::smf_app* smf_app_inst;
extern smf::smf_config smf_cfg;

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
  Logger::smf_app().info(
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
      Logger::smf_app().error(
          "smf_pdu_session::set(paa_t) Unknown PDN type %d",
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
      Logger::smf_app().error(
          "smf_pdu_session::get_paa (paa_t) Unknown PDN type %d",
          pdu_session_type.pdu_session_type);
  }
  paa.pdu_session_type.pdu_session_type = pdu_session_type.pdu_session_type;
}

//------------------------------------------------------------------------------
void smf_pdu_session::add_qos_flow(const smf_qos_flow& flow) {
  if ((flow.qfi.qfi >= QOS_FLOW_IDENTIFIER_FIRST) and
      (flow.qfi.qfi <= QOS_FLOW_IDENTIFIER_LAST)) {
    Logger::smf_app().trace(
        "QoS Flow (flow Id %d) has been added successfully", flow.qfi.qfi);
    std::unique_lock lock(m_pdu_session_mutex);
    qos_flows.erase(flow.qfi.qfi);
    qos_flows.insert(
        std::pair<uint8_t, smf_qos_flow>((uint8_t) flow.qfi.qfi, flow));
  } else {
    Logger::smf_app().error(
        "Failed to add QoS flow (flow Id %d), invalid QFI", flow.qfi.qfi);
  }
}

//------------------------------------------------------------------------------
bool smf_pdu_session::get_qos_flow(
    const pfcp::pdr_id_t& pdr_id, smf_qos_flow& q) {
  std::shared_lock lock(m_pdu_session_mutex);
  for (auto it : qos_flows) {
    if (it.second.pdr_id_ul.rule_id == pdr_id.rule_id) {
      q = it.second;
      return true;
    }
    if (it.second.pdr_id_dl.rule_id == pdr_id.rule_id) {
      q = it.second;
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------------
bool smf_pdu_session::get_qos_flow(
    const pfcp::far_id_t& far_id, smf_qos_flow& q) {
  std::shared_lock lock(m_pdu_session_mutex);
  for (auto it : qos_flows) {
    if ((it.second.far_id_ul.first) &&
        (it.second.far_id_ul.second.far_id == far_id.far_id)) {
      q = it.second;
      return true;
    }
    if ((it.second.far_id_dl.first) &&
        (it.second.far_id_dl.second.far_id == far_id.far_id)) {
      q = it.second;
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------------
bool smf_pdu_session::get_qos_flow(const pfcp::qfi_t& qfi, smf_qos_flow& q) {
  std::shared_lock lock(m_pdu_session_mutex);
  for (auto it : qos_flows) {
    if (it.second.qfi == qfi) {
      q = it.second;
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------------
void smf_pdu_session::set_default_qos_flow(const pfcp::qfi_t& qfi) {
  default_qfi.qfi = qfi.qfi;
}

//------------------------------------------------------------------------------
bool smf_pdu_session::get_default_qos_flow(smf_qos_flow& flow) {
  Logger::smf_app().debug("Get default QoS Flow of this PDU session.");
  return get_qos_flow(default_qfi, flow);
}

//------------------------------------------------------------------------------
void smf_pdu_session::get_qos_flows(std::vector<smf_qos_flow>& flows) {
  std::shared_lock lock(m_pdu_session_mutex);
  flows.clear();
  for (auto it : qos_flows) {
    flows.push_back(it.second);
  }
}

//------------------------------------------------------------------------------
bool smf_pdu_session::find_qos_flow(
    const pfcp::pdr_id_t& pdr_id, smf_qos_flow& flow) {
  std::shared_lock lock(m_pdu_session_mutex);
  for (std::map<uint8_t, smf_qos_flow>::iterator it = qos_flows.begin();
       it != qos_flows.end(); ++it) {
    if ((it->second.pdr_id_ul == pdr_id) || (it->second.pdr_id_dl == pdr_id)) {
      flow = it->second;
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------------
void smf_pdu_session::remove_qos_flow(const pfcp::qfi_t& qfi) {
  std::unique_lock lock(m_pdu_session_mutex);
  smf_qos_flow& flow = qos_flows[qfi.qfi];
  flow.deallocate_ressources();
  qos_flows.erase(qfi.qfi);
}

//------------------------------------------------------------------------------
void smf_pdu_session::remove_qos_flow(smf_qos_flow& flow) {
  std::unique_lock lock(m_pdu_session_mutex);
  pfcp::qfi_t qfi = {.qfi = flow.qfi.qfi};
  flow.deallocate_ressources();
  qos_flows.erase(qfi.qfi);
}

//------------------------------------------------------------------------------
void smf_pdu_session::remove_qos_flows() {
  std::unique_lock lock(m_pdu_session_mutex);
  for (std::map<uint8_t, smf_qos_flow>::iterator it = qos_flows.begin();
       it != qos_flows.end(); ++it) {
    it->second.deallocate_ressources();
  }
  qos_flows.clear();
}

//------------------------------------------------------------------------------
void smf_pdu_session::deallocate_ressources(const std::string& dnn) {
  for (std::map<uint8_t, smf_qos_flow>::iterator it = qos_flows.begin();
       it != qos_flows.end(); ++it) {
    // TODO: release FAR_ID, PDR_ID
    // release_pdr_id(it->second.pdr_id_dl);
    // release_pdr_id(it->second.pdr_id_ul);
    // release_far_id(it->second.far_id_dl.second);
    // release_far_id(it->second.far_id_ul.second);
    it->second.deallocate_ressources();
  }
  if (ipv4) {
    paa_dynamic::get_instance().release_paa(dnn, ipv4_address);
  }
  clear();  // including qos_flows.clear()
  Logger::smf_app().info(
      "Resources associated with this PDU Session have been released");
}

//------------------------------------------------------------------------------
void smf_pdu_session::generate_seid() {}

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

  if (default_qfi.qfi) {
    s.append("Default ");
    for (auto it : qos_flows) {
      if (it.second.qfi == default_qfi.qfi) {
        s.append(it.second.toString());
      }
    }
  }
  return s;
}

//------------------------------------------------------------------------------
void smf_pdu_session::set_pdu_session_status(
    const pdu_session_status_e& status) {
  // TODO: Should consider congestion handling
  Logger::smf_app().info(
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
  Logger::smf_app().info(
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
void smf_pdu_session::get_qos_rules_to_be_synchronised(
    std::vector<QOSRulesIE>& rules) const {
  std::shared_lock lock(m_pdu_session_mutex);
  for (auto it : qos_rules_to_be_synchronised) {
    if (qos_rules.count(it) > 0) rules.push_back(qos_rules.at(it));
  }
}

//------------------------------------------------------------------------------
void smf_pdu_session::get_qos_rules(
    const pfcp::qfi_t& qfi, std::vector<QOSRulesIE>& rules) const {
  Logger::smf_app().info(
      "Get QoS Rules associated with Flow with QFI %d", qfi.qfi);
  std::shared_lock lock(m_pdu_session_mutex);
  for (auto it : qos_rules) {
    if (it.second.qosflowidentifer == qfi.qfi)
      rules.push_back(qos_rules.at(it.first));
  }
}

//------------------------------------------------------------------------------
bool smf_pdu_session::get_default_qos_rule(QOSRulesIE& qos_rule) const {
  Logger::smf_app().info(
      "Get default QoS Rule this PDU Session (ID %d)", pdu_session_id);
  std::shared_lock lock(m_pdu_session_mutex);
  for (auto it : qos_rules) {
    if (it.second.dqrbit == THE_QOS_RULE_IS_DEFAULT_QOS_RULE) {
      qos_rule = it.second;
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------------
bool smf_pdu_session::get_qos_rule(
    const uint8_t rule_id, QOSRulesIE& qos_rule) const {
  Logger::smf_app().info("Find QoS Rule with Rule Id %d", (uint8_t) rule_id);
  std::shared_lock lock(m_pdu_session_mutex);
  if (qos_rules.count(rule_id) > 0) {
    qos_rule = qos_rules.at(rule_id);
  }
  return false;
}

//------------------------------------------------------------------------------
void smf_pdu_session::update_qos_rule(const QOSRulesIE& qos_rule) {
  std::unique_lock lock(
      m_pdu_session_mutex,
      std::defer_lock);  // Do not lock it first

  Logger::smf_app().info(
      "Update QoS Rule with Rule Id %d", (uint8_t) qos_rule.qosruleidentifer);
  uint8_t rule_id = qos_rule.qosruleidentifer;
  if ((rule_id >= QOS_RULE_IDENTIFIER_FIRST) and
      (rule_id <= QOS_RULE_IDENTIFIER_LAST)) {
    if (qos_rules.count(rule_id) > 0) {
      lock.lock();  // Lock it here
      qos_rules.erase(rule_id);
      qos_rules.insert(std::pair<uint8_t, QOSRulesIE>(rule_id, qos_rule));
      // marked to be synchronised with UE
      qos_rules_to_be_synchronised.push_back(rule_id);
      Logger::smf_app().trace("Update QoS rule (%d) success", rule_id);
    } else {
      Logger::smf_app().error(
          "Update QoS Rule (%d) failed, rule does not existed", rule_id);
    }

  } else {
    Logger::smf_app().error(
        "Update QoS rule (%d) failed, invalid Rule Id", rule_id);
  }
}

//------------------------------------------------------------------------------
void smf_pdu_session::mark_qos_rule_to_be_synchronised(const uint8_t rule_id) {
  std::unique_lock lock(
      m_pdu_session_mutex,
      std::defer_lock);  // Do not lock it first
  if ((rule_id >= QOS_RULE_IDENTIFIER_FIRST) and
      (rule_id <= QOS_RULE_IDENTIFIER_LAST)) {
    if (qos_rules.count(rule_id) > 0) {
      lock.lock();  // Lock it here
      qos_rules_to_be_synchronised.push_back(rule_id);
      Logger::smf_app().trace(
          "smf_pdu_session::mark_qos_rule_to_be_synchronised(%d) success",
          rule_id);
    } else {
      Logger::smf_app().error(
          "smf_pdu_session::mark_qos_rule_to_be_synchronised(%d) failed, rule "
          "does not existed",
          rule_id);
    }

  } else {
    Logger::smf_app().error(
        "smf_pdu_session::mark_qos_rule_to_be_synchronised(%d) failed, invalid "
        "Rule Id",
        rule_id);
  }
}

//------------------------------------------------------------------------------
void smf_pdu_session::add_qos_rule(const QOSRulesIE& qos_rule) {
  std::unique_lock lock(
      m_pdu_session_mutex,
      std::defer_lock);  // Do not lock it first
  Logger::smf_app().info(
      "Add QoS Rule with Rule Id %d", (uint8_t) qos_rule.qosruleidentifer);
  uint8_t rule_id = qos_rule.qosruleidentifer;

  if ((rule_id >= QOS_RULE_IDENTIFIER_FIRST) and
      (rule_id <= QOS_RULE_IDENTIFIER_LAST)) {
    if (qos_rules.count(rule_id) > 0) {
      Logger::smf_app().error(
          "Failed to add rule (Id %d), rule existed", rule_id);
    } else {
      lock.lock();  // Lock it here
      qos_rules.insert(std::pair<uint8_t, QOSRulesIE>(rule_id, qos_rule));
      Logger::smf_app().trace(
          "Rule (Id %d) has been added successfully", rule_id);
    }

  } else {
    Logger::smf_app().error(
        "Failed to add rule (Id %d) failed: invalid rule Id", rule_id);
  }
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
  Logger::smf_app().info("Find DNN configuration with DNN %s", dnn.c_str());
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
  std::unique_lock<std::recursive_mutex> lock(m_context);
  auto found = std::find_if(
      pending_procedures.begin(), pending_procedures.end(),
      [proc](const std::shared_ptr<smf_procedure>& i) {
        return i.get() == proc;
      });
  if (found != pending_procedures.end()) {
    pending_procedures.erase(found);
  }
}

//------------------------------------------------------------------------------
void smf_context::handle_itti_msg(
    itti_n4_session_establishment_response& seresp) {
  std::shared_ptr<smf_procedure> proc = {};
  if (find_procedure(seresp.trxn_id, proc)) {
    Logger::smf_app().debug(
        "Received N4 Session Establishment Response sender teid " TEID_FMT
        "  pfcp_tx_id %" PRIX64 "",
        seresp.seid, seresp.trxn_id);
    proc->handle_itti_msg(seresp, shared_from_this());
    remove_procedure(proc.get());
  } else {
    Logger::smf_app().debug(
        "Received N4 Session Establishment Response sender teid " TEID_FMT
        "  pfcp_tx_id %" PRIX64 ", smf_procedure not found, discarded!",
        seresp.seid, seresp.trxn_id);
  }
}

//------------------------------------------------------------------------------
void smf_context::handle_itti_msg(
    itti_n4_session_modification_response& smresp) {
  std::shared_ptr<smf_procedure> proc = {};
  if (find_procedure(smresp.trxn_id, proc)) {
    Logger::smf_app().debug(
        "Received N4 Session Modification Response sender teid " TEID_FMT
        "  pfcp_tx_id %" PRIX64 " ",
        smresp.seid, smresp.trxn_id);
    proc->handle_itti_msg(smresp, shared_from_this());
    remove_procedure(proc.get());
  } else {
    Logger::smf_app().debug(
        "Received N4 Session Modification Response sender teid " TEID_FMT
        "  pfcp_tx_id %" PRIX64 ", smf_procedure not found, discarded!",
        smresp.seid, smresp.trxn_id);
  }
  Logger::smf_app().info(
      "Handle N4 Session Modification Response with SMF context %s",
      toString().c_str());
}

//------------------------------------------------------------------------------
void smf_context::handle_itti_msg(itti_n4_session_deletion_response& sdresp) {
  std::shared_ptr<smf_procedure> proc = {};
  if (find_procedure(sdresp.trxn_id, proc)) {
    Logger::smf_app().debug(
        "Received N4 Session Deletion Response sender teid " TEID_FMT
        "  pfcp_tx_id %" PRIX64 " ",
        sdresp.seid, sdresp.trxn_id);
    proc->handle_itti_msg(sdresp, shared_from_this());
    remove_procedure(proc.get());
  } else {
    Logger::smf_app().debug(
        "Received N4 Session Deletion Response sender teid " TEID_FMT
        "  pfcp_tx_id %" PRIX64 ", smf_procedure not found, discarded!",
        sdresp.seid, sdresp.trxn_id);
  }

  Logger::smf_app().info(
      "Handle N4 Session Deletion Response with SMF context %s",
      toString().c_str());
}

//------------------------------------------------------------------------------
void smf_context::handle_itti_msg(
    std::shared_ptr<itti_n4_session_report_request>& req) {
  pfcp::report_type_t report_type;
  if (req->pfcp_ies.get(report_type)) {
    pfcp::pdr_id_t pdr_id;
    // Downlink Data Report
    if (report_type.dldr) {
      pfcp::downlink_data_report data_report;
      if (req->pfcp_ies.get(data_report)) {
        pfcp::pdr_id_t pdr_id;
        if (data_report.get(pdr_id)) {
          std::shared_ptr<dnn_context> sd     = {};
          std::shared_ptr<smf_pdu_session> sp = {};
          pfcp::qfi_t qfi                     = {};
          if (find_pdu_session(pdr_id, qfi, sd, sp)) {
            // Step 1. send N4 Data Report Ack to UPF
            pfcp::node_id_t up_node_id = {};
            scid_t scid                = get_scid();
            // Get UPF node
            std::shared_ptr<smf_context_ref> scf = {};
            if (smf_app_inst->is_scid_2_smf_context(scid)) {
              scf        = smf_app_inst->scid_2_smf_context(scid);
              up_node_id = scf.get()->upf_node_id;
            } else {
              Logger::smf_app().warn(
                  "SM Context associated with this id " SCID_FMT
                  " does not exit!",
                  scid);
              return;
            }

            itti_n4_session_report_response* n4_ser =
                new itti_n4_session_report_response(TASK_SMF_APP, TASK_SMF_N4);
            n4_ser->seid    = req->seid;
            n4_ser->trxn_id = req->trxn_id;
            n4_ser->r_endpoint =
                endpoint(up_node_id.u1.ipv4_address, pfcp::default_port);
            std::shared_ptr<itti_n4_session_report_response> n4_report_ack =
                std::shared_ptr<itti_n4_session_report_response>(n4_ser);

            Logger::smf_app().info(
                "Sending ITTI message %s to task TASK_SMF_N4",
                n4_ser->get_msg_name());
            int ret = itti_inst->send_msg(n4_report_ack);
            if (RETURNok != ret) {
              Logger::smf_app().error(
                  "Could not send ITTI message %s to task TASK_SMF_N4",
                  n4_ser->get_msg_name());
              return;
            }

            // Step 2. Send N1N2MessageTranfer to AMF
            pdu_session_report_response session_report_msg = {};
            // set the required IEs
            session_report_msg.set_supi(supi);
            session_report_msg.set_snssai(sd.get()->nssai);
            session_report_msg.set_dnn(sd.get()->dnn_in_use);
            session_report_msg.set_pdu_session_type(
                sp.get()->get_pdu_session_type().pdu_session_type);
            // get supi and put into URL
            std::string supi_prefix = {};
            get_supi_prefix(supi_prefix);
            std::string supi_str = supi_prefix + "-" + smf_supi_to_string(supi);
            std::string url =
                // std::string(inet_ntoa(
                //    *((struct in_addr*) &smf_cfg.amf_addr.ipv4_addr))) +
                //":" + std::to_string(smf_cfg.amf_addr.port) +
                sp.get()->get_amf_addr() + NAMF_COMMUNICATION_BASE +
                smf_cfg.amf_addr.api_version +
                fmt::format(
                    NAMF_COMMUNICATION_N1N2_MESSAGE_TRANSFER_URL,
                    supi_str.c_str());
            session_report_msg.set_amf_url(url);
            // seid and trxn_id to be used in Failure indication
            session_report_msg.set_seid(req->seid);
            session_report_msg.set_trxn_id(req->trxn_id);

            // QFIs, QoS profiles, CN Tunnel
            smf_qos_flow flow = {};
            sp.get()->get_qos_flow(qfi, flow);
            // ADD QoS Flow to be updated
            qos_flow_context_updated qcu = {};
            qcu.set_qfi(qfi);
            qcu.set_ul_fteid(flow.ul_fteid);
            qcu.set_qos_profile(flow.qos_profile);
            session_report_msg.add_qos_flow_context_updated(qcu);

            // Create N2 SM Information: PDU Session Resource Setup Request
            // Transfer IE
            // N2 SM Information
            std::string n2_sm_info, n2_sm_info_hex;
            smf_n2::get_instance()
                .create_n2_pdu_session_resource_setup_request_transfer(
                    session_report_msg, n2_sm_info_type_e::PDU_RES_SETUP_REQ,
                    n2_sm_info);

            smf_app_inst->convert_string_2_hex(n2_sm_info, n2_sm_info_hex);
            session_report_msg.set_n2_sm_information(n2_sm_info_hex);

            // Fill the json part
            nlohmann::json json_data = {};
            json_data["n2InfoContainer"]["n2InformationClass"] =
                N1N2_MESSAGE_CLASS;
            json_data["n2InfoContainer"]["smInfo"]["PduSessionId"] =
                session_report_msg.get_pdu_session_id();
            // N2InfoContent (section 6.1.6.2.27@3GPP TS 29.518)
            json_data["n2InfoContainer"]["smInfo"]["n2InfoContent"]
                     ["ngapIeType"] = "PDU_RES_SETUP_REQ";  // NGAP message type
            json_data["n2InfoContainer"]["smInfo"]["n2InfoContent"]["ngapData"]
                     ["contentId"] = N2_SM_CONTENT_ID;  // NGAP part
            json_data["n2InfoContainer"]["smInfo"]["sNssai"]["sst"] =
                session_report_msg.get_snssai().sST;
            json_data["n2InfoContainer"]["smInfo"]["sNssai"]["sd"] =
                session_report_msg.get_snssai().sD;

            session_report_msg.set_json_data(json_data);

            itti_n11_session_report_request* itti_n11 =
                new itti_n11_session_report_request(TASK_SMF_APP, TASK_SMF_SBI);
            itti_n11->http_version = 1;  // use HTTPv1 for the moment
            std::shared_ptr<itti_n11_session_report_request> itti_n11_report =
                std::shared_ptr<itti_n11_session_report_request>(itti_n11);
            itti_n11_report->res = session_report_msg;
            // send ITTI message to N11 interface to trigger N1N2MessageTransfer
            // towards AMFs
            Logger::smf_app().info(
                "Sending ITTI message %s to task TASK_SMF_SBI",
                itti_n11_report->get_msg_name());

            ret = itti_inst->send_msg(itti_n11_report);
            if (RETURNok != ret) {
              Logger::smf_app().error(
                  "Could not send ITTI message %s to task TASK_SMF_SBI",
                  itti_n11_report->get_msg_name());
            }
          }
        }
      }
    }
    // Usage Report
    if (report_type.usar) {
      // TODO
      Logger::smf_app().debug("PFCP_SESSION_REPORT_REQUEST/Usage Report");
    }
    // Error Indication Report
    if (report_type.erir) {
      // TODO
      Logger::smf_app().debug(
          "PFCP_SESSION_REPORT_REQUEST/Error Indication Report");
    }
    // User Plane Inactivity Report
    if (report_type.upir) {
      // TODO
      Logger::smf_app().debug(
          "PFCP_SESSION_REPORT_REQUEST/User Plane Inactivity Report");
    }
  }
}

//------------------------------------------------------------------------------
std::string smf_context::toString() const {
  std::unique_lock<std::recursive_mutex> lock(m_context);
  std::string s = {};
  s.append("\n");
  s.append("SMF CONTEXT:\n");
  s.append("\tSUPI:\t\t\t\t")
      .append(smf_supi_to_string(supi).c_str())
      .append("\n");
  for (auto it : dnns) {
    s.append(it->toString());
  }
  return s;
}

//------------------------------------------------------------------------------
void smf_context::get_default_qos(
    const snssai_t& snssai, const std::string& dnn,
    subscribed_default_qos_t& default_qos) {
  Logger::smf_app().info(
      "Get default QoS for a PDU Session, key %d", (uint8_t) snssai.sST);
  // get the default QoS profile
  std::shared_ptr<session_management_subscription> ss = {};
  std::shared_ptr<dnn_configuration_t> sdc            = {};
  find_dnn_subscription(snssai, ss);

  if (nullptr != ss.get()) {
    ss.get()->find_dnn_configuration(dnn, sdc);
    if (nullptr != sdc.get()) {
      default_qos = sdc.get()->_5g_qos_profile;
    }
  }
}

//------------------------------------------------------------------------------
void smf_context::get_default_qos_rule(
    QOSRulesIE& qos_rule, uint8_t pdu_session_type) {
  // TODO, update according to PDU Session type
  Logger::smf_app().info(
      "Get default QoS rule for a PDU Session (PDU session type %d)",
      pdu_session_type);
  // see section 9.11.4.13 @ 3GPP TS 24.501 and section 5.7.1.4 @ 3GPP TS 23.501
  qos_rule.qosruleidentifer  = 0x01;  // be updated later on
  qos_rule.ruleoperationcode = CREATE_NEW_QOS_RULE;
  qos_rule.dqrbit            = THE_QOS_RULE_IS_DEFAULT_QOS_RULE;
  if ((pdu_session_type == PDU_SESSION_TYPE_E_IPV4) or
      (pdu_session_type == PDU_SESSION_TYPE_E_IPV4V6) or
      (pdu_session_type == PDU_SESSION_TYPE_E_IPV6) or
      (pdu_session_type == PDU_SESSION_TYPE_E_ETHERNET)) {
    qos_rule.numberofpacketfilters = 1;
    qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace =
        (Create_ModifyAndAdd_ModifyAndReplace*) calloc(
            1, sizeof(Create_ModifyAndAdd_ModifyAndReplace));
    qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace[0]
        .packetfilterdirection = 0b11;  // bi-directional
    qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace[0]
        .packetfilteridentifier = 1;
    qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace[0]
        .packetfiltercontents.component_type = QOS_RULE_MATCHALL_TYPE;
    // qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace[0].packetfiltercontents.component_value
    // = bfromcstralloc(2, "\0");
    qos_rule.qosruleprecedence = 0xff;
  }

  if (pdu_session_type == PDU_SESSION_TYPE_E_UNSTRUCTURED) {
    qos_rule.numberofpacketfilters = 0;
    qos_rule.qosruleprecedence     = 0xff;
  }

  qos_rule.segregation      = SEGREGATION_NOT_REQUESTED;
  qos_rule.qosflowidentifer = DEFAULT_QFI;

  Logger::smf_app().debug(
      "Default QoSRules: %x %x %x %x %x %x %x %x %x", qos_rule.qosruleidentifer,
      qos_rule.ruleoperationcode, qos_rule.dqrbit,
      qos_rule.numberofpacketfilters,
      qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace
          ->packetfilterdirection,
      qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace
          ->packetfilteridentifier,
      qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace
          ->packetfiltercontents.component_type,
      qos_rule.qosruleprecedence, qos_rule.segregation,
      qos_rule.qosflowidentifer);
}

//------------------------------------------------------------------------------
void smf_context::get_default_qos_flow_description(
    QOSFlowDescriptionsContents& qos_flow_description, uint8_t pdu_session_type,
    const pfcp::qfi_t& qfi) {
  // TODO, update according to PDU Session type
  Logger::smf_app().info(
      "Get default QoS Flow Description (PDU session type %d)",
      pdu_session_type);
  qos_flow_description.qfi                = qfi.qfi;
  qos_flow_description.operationcode      = CREATE_NEW_QOS_FLOW_DESCRIPTION;
  qos_flow_description.e                  = PARAMETERS_LIST_IS_INCLUDED;
  qos_flow_description.numberofparameters = 1;
  qos_flow_description.parameterslist =
      (ParametersList*) calloc(3, sizeof(ParametersList));
  qos_flow_description.parameterslist[0].parameteridentifier =
      PARAMETER_IDENTIFIER_5QI;
  qos_flow_description.parameterslist[0].parametercontents._5qi = qfi.qfi;
  /*
   qos_flow_description.parameterslist[1].parameteridentifier =
   PARAMETER_IDENTIFIER_GFBR_UPLINK;
   qos_flow_description.parameterslist[1].parametercontents
   .gfbrormfbr_uplinkordownlink.uint =
   GFBRORMFBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
   qos_flow_description.parameterslist[1].parametercontents
   .gfbrormfbr_uplinkordownlink.value = 0x10;
   qos_flow_description.parameterslist[2].parameteridentifier =
   PARAMETER_IDENTIFIER_GFBR_DOWNLINK;
   qos_flow_description.parameterslist[2].parametercontents
   .gfbrormfbr_uplinkordownlink.uint =
   GFBRORMFBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
   qos_flow_description.parameterslist[2].parametercontents
   .gfbrormfbr_uplinkordownlink.value = 0x10;
   */

  Logger::smf_app().debug(
      "Default Qos Flow Description: %x %x %x %x %x %x",
      qos_flow_description.qfi, qos_flow_description.operationcode,
      qos_flow_description.e, qos_flow_description.numberofparameters,
      qos_flow_description.parameterslist[0].parameteridentifier,
      qos_flow_description.parameterslist[0].parametercontents._5qi
      /*      qos_flow_description.parameterslist[1].parameteridentifier,
       qos_flow_description.parameterslist[1].parametercontents
       .gfbrormfbr_uplinkordownlink.uint,
       qos_flow_description.parameterslist[1].parametercontents
       .gfbrormfbr_uplinkordownlink.value,
       qos_flow_description.parameterslist[2].parameteridentifier,
       qos_flow_description.parameterslist[2].parametercontents
       .gfbrormfbr_uplinkordownlink.uint,
       qos_flow_description.parameterslist[2].parametercontents
       .gfbrormfbr_uplinkordownlink.value
       */);
}

//------------------------------------------------------------------------------
void smf_context::get_session_ambr(
    SessionAMBR& session_ambr, const snssai_t& snssai, const std::string& dnn) {
  Logger::smf_app().debug(
      "Get AMBR info from the subscription information (DNN %s)", dnn.c_str());

  std::shared_ptr<session_management_subscription> ss = {};
  std::shared_ptr<dnn_configuration_t> sdc            = {};
  find_dnn_subscription(snssai, ss);
  if (nullptr != ss.get()) {
    ss.get()->find_dnn_configuration(dnn, sdc);
    if (nullptr != sdc.get()) {
      Logger::smf_app().debug(
          "Default AMBR info from the subscription information, downlink %s, "
          "uplink %s",
          (sdc.get()->session_ambr).downlink.c_str(),
          (sdc.get()->session_ambr).uplink.c_str());

      // Downlink
      size_t leng_of_session_ambr_dl =
          (sdc.get()->session_ambr).downlink.length();
      try {
        std::string session_ambr_dl_unit =
            (sdc.get()->session_ambr)
                .downlink.substr(
                    leng_of_session_ambr_dl -
                    4);  // 4 last characters stand for mbps, kbps, ..
        if (session_ambr_dl_unit.compare("Kbps") == 0)
          session_ambr.uint_for_session_ambr_for_downlink =
              AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1KBPS;
        if (session_ambr_dl_unit.compare("Mbps") == 0)
          session_ambr.uint_for_session_ambr_for_downlink =
              AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
        if (session_ambr_dl_unit.compare("Gbps") == 0)
          session_ambr.uint_for_session_ambr_for_downlink =
              AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1GBPS;
        if (session_ambr_dl_unit.compare("Tbps") == 0)
          session_ambr.uint_for_session_ambr_for_downlink =
              AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1TBPS;
        if (session_ambr_dl_unit.compare("Pbps") == 0)
          session_ambr.uint_for_session_ambr_for_downlink =
              AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1PBPS;

        session_ambr.session_ambr_for_downlink =
            std::stoi((sdc.get()->session_ambr)
                          .downlink.substr(0, leng_of_session_ambr_dl - 4));
      } catch (const std::exception& e) {
        Logger::smf_app().warn("Undefined error: %s", e.what());
        // assign default value
        session_ambr.session_ambr_for_downlink = 1;
        session_ambr.uint_for_session_ambr_for_downlink =
            AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
      }

      // Uplink
      size_t leng_of_session_ambr_ul =
          (sdc.get()->session_ambr).uplink.length();
      try {
        std::string session_ambr_ul_unit =
            (sdc.get()->session_ambr)
                .uplink.substr(
                    leng_of_session_ambr_ul -
                    4);  // 4 last characters stand for mbps, kbps, ..
        if (session_ambr_ul_unit.compare("Kbps") == 0)
          session_ambr.uint_for_session_ambr_for_uplink =
              AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1KBPS;
        if (session_ambr_ul_unit.compare("Mbps") == 0)
          session_ambr.uint_for_session_ambr_for_uplink =
              AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
        if (session_ambr_ul_unit.compare("Gbps") == 0)
          session_ambr.uint_for_session_ambr_for_uplink =
              AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1GBPS;
        if (session_ambr_ul_unit.compare("Tbps") == 0)
          session_ambr.uint_for_session_ambr_for_uplink =
              AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1TBPS;
        if (session_ambr_ul_unit.compare("Pbps") == 0)
          session_ambr.uint_for_session_ambr_for_uplink =
              AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1PBPS;

        session_ambr.session_ambr_for_uplink =
            std::stoi((sdc.get()->session_ambr)
                          .uplink.substr(0, leng_of_session_ambr_ul - 4));
      } catch (const std::exception& e) {
        Logger::smf_app().warn("Undefined error: %s", e.what());
        // assign default value
        session_ambr.session_ambr_for_uplink = 1;
        session_ambr.uint_for_session_ambr_for_uplink =
            AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
      }
    }
  } else {
    Logger::smf_app().debug(
        "Could not get default info from the subscription information, use "
        "default value instead.");
    // use default value
    session_ambr.session_ambr_for_downlink = 1;
    session_ambr.uint_for_session_ambr_for_downlink =
        AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
    session_ambr.session_ambr_for_uplink = 1;
    session_ambr.uint_for_session_ambr_for_uplink =
        AMBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
  }
}

//------------------------------------------------------------------------------
void smf_context::get_session_ambr(
    Ngap_PDUSessionAggregateMaximumBitRate_t& session_ambr,
    const snssai_t& snssai, const std::string& dnn) {
  std::shared_ptr<session_management_subscription> ss = {};
  std::shared_ptr<dnn_configuration_t> sdc            = {};
  find_dnn_subscription(snssai, ss);

  uint32_t bit_rate_dl = {110000000};  // TODO: to be updated
  uint32_t bit_rate_ul = {110000000};  // TODO: to be updated

  session_ambr.pDUSessionAggregateMaximumBitRateDL.size = 4;
  session_ambr.pDUSessionAggregateMaximumBitRateDL.buf =
      (uint8_t*) calloc(4, sizeof(uint8_t));
  session_ambr.pDUSessionAggregateMaximumBitRateUL.size = 4;
  session_ambr.pDUSessionAggregateMaximumBitRateUL.buf =
      (uint8_t*) calloc(4, sizeof(uint8_t));

  if (nullptr != ss.get()) {
    ss.get()->find_dnn_configuration(dnn, sdc);

    if (nullptr != sdc.get()) {
      Logger::smf_app().debug(
          "Default AMBR info from the DNN configuration, uplink %s, downlink "
          "%s",
          (sdc.get()->session_ambr).uplink.c_str(),
          (sdc.get()->session_ambr).downlink.c_str());
      // Downlink
      size_t leng_of_session_ambr_dl =
          (sdc.get()->session_ambr).downlink.length();
      try {
        bit_rate_dl =
            std::stoi((sdc.get()->session_ambr)
                          .downlink.substr(0, leng_of_session_ambr_dl - 4));
        std::string session_ambr_dl_unit =
            (sdc.get()->session_ambr)
                .downlink.substr(
                    leng_of_session_ambr_dl -
                    4);  // 4 last characters stand for mbps, kbps, ..
        if (session_ambr_dl_unit.compare("Kbps") == 0) bit_rate_dl *= 1000;
        if (session_ambr_dl_unit.compare("Mbps") == 0) bit_rate_dl *= 1000000;
        if (session_ambr_dl_unit.compare("Gbps") == 0)
          bit_rate_dl *= 1000000000;
        INT32_TO_BUFFER(
            bit_rate_dl, session_ambr.pDUSessionAggregateMaximumBitRateDL.buf);
      } catch (const std::exception& e) {
        Logger::smf_app().warn("Undefined error: %s", e.what());
        // assign default value
        bit_rate_dl = 1;
        INT32_TO_BUFFER(
            bit_rate_dl, session_ambr.pDUSessionAggregateMaximumBitRateDL.buf);
      }

      // Uplink
      size_t leng_of_session_ambr_ul =
          (sdc.get()->session_ambr).uplink.length();
      try {
        bit_rate_ul =
            std::stoi((sdc.get()->session_ambr)
                          .uplink.substr(0, leng_of_session_ambr_ul - 4));
        std::string session_ambr_ul_unit =
            (sdc.get()->session_ambr)
                .uplink.substr(
                    leng_of_session_ambr_ul -
                    4);  // 4 last characters stand for mbps, kbps, ..
        if (session_ambr_ul_unit.compare("Kbps") == 0) bit_rate_ul *= 1000;
        if (session_ambr_ul_unit.compare("Mbps") == 0) bit_rate_ul *= 1000000;
        if (session_ambr_ul_unit.compare("Gbps") == 0)
          bit_rate_ul *= 1000000000;
        INT32_TO_BUFFER(
            bit_rate_ul, session_ambr.pDUSessionAggregateMaximumBitRateUL.buf);
      } catch (const std::exception& e) {
        Logger::smf_app().warn("Undefined error: %s", e.what());
        // assign default value
        bit_rate_ul = 1;
        INT32_TO_BUFFER(
            bit_rate_ul, session_ambr.pDUSessionAggregateMaximumBitRateUL.buf);
      }
    }
  } else {
    INT32_TO_BUFFER(
        bit_rate_dl, session_ambr.pDUSessionAggregateMaximumBitRateDL.buf);
    INT32_TO_BUFFER(
        bit_rate_ul, session_ambr.pDUSessionAggregateMaximumBitRateUL.buf);
  }

  Logger::smf_app().debug(
      "Get AMBR info from the subscription information (DNN %s), uplink %d "
      "downlink %d",
      dnn.c_str(), bit_rate_ul, bit_rate_dl);
}

//-------------------------------------------------------------------------------------
bool smf_context::handle_pdu_session_modification_request(
    nas_message_t& nas_msg,
    std::shared_ptr<itti_n11_update_sm_context_request>& sm_context_request,
    std::shared_ptr<itti_n11_update_sm_context_response>& sm_context_resp,
    std::shared_ptr<smf_pdu_session>& sp) {
  return true;
}

//-------------------------------------------------------------------------------------
bool smf_context::handle_pdu_session_modification_complete(
    nas_message_t& nas_msg,
    std::shared_ptr<itti_n11_update_sm_context_request>& sm_context_request,
    std::shared_ptr<itti_n11_update_sm_context_response>& sm_context_resp,
    std::shared_ptr<smf_pdu_session>& sp) {
  
  }

//-------------------------------------------------------------------------------------
bool smf_context::handle_pdu_session_modification_command_reject(
    nas_message_t& nas_msg,
    std::shared_ptr<itti_n11_update_sm_context_request>& sm_context_request,
    std::shared_ptr<itti_n11_update_sm_context_response>& sm_context_resp,
    std::shared_ptr<smf_pdu_session>& sp) {
  return true;
}

//-------------------------------------------------------------------------------------
bool smf_context::handle_pdu_session_release_request(
    nas_message_t& nas_msg,
    std::shared_ptr<itti_n11_update_sm_context_request>& sm_context_request,
    std::shared_ptr<itti_n11_update_sm_context_response>& sm_context_resp,
    std::shared_ptr<smf_pdu_session>& sp) {
  return true;
}

//-------------------------------------------------------------------------------------
bool smf_context::handle_pdu_session_release_complete(
    nas_message_t& nas_msg,
    std::shared_ptr<itti_n11_update_sm_context_request>& sm_context_request,
    std::shared_ptr<itti_n11_update_sm_context_response>& sm_context_resp,
    std::shared_ptr<smf_pdu_session>& sp) {
  return true;
}

//-------------------------------------------------------------------------------------
bool smf_context::handle_pdu_session_resource_setup_response_transfer(
    std::string& n2_sm_information,
    std::shared_ptr<itti_n11_update_sm_context_request>& sm_context_request) {
  return true;
}
//-------------------------------------------------------------------------------------
bool smf_context::handle_pdu_session_resource_setup_unsuccessful_transfer(
    std::string& n2_sm_information,
    std::shared_ptr<itti_n11_update_sm_context_request>& sm_context_request) {
  return true;
}

//-------------------------------------------------------------------------------------
bool smf_context::handle_pdu_session_resource_modify_response_transfer(
    std::string& n2_sm_information,
    std::shared_ptr<itti_n11_update_sm_context_request>& sm_context_request) {
  
  return true;
}

//-------------------------------------------------------------------------------------
bool smf_context::handle_pdu_session_resource_release_response_transfer(
    std::string& n2_sm_information,
    std::shared_ptr<itti_n11_update_sm_context_request>& sm_context_request) {
  return true;
}

//-------------------------------------------------------------------------------------
bool smf_context::handle_service_request(
    std::string& n2_sm_information,
    std::shared_ptr<itti_n11_update_sm_context_request>& sm_context_request,
    std::shared_ptr<itti_n11_update_sm_context_response>& sm_context_resp,
    std::shared_ptr<smf_pdu_session>& sp) {
  std::string n2_sm_info, n2_sm_info_hex;

  // Update upCnxState
  sp.get()->set_upCnx_state(upCnx_state_e::UPCNX_STATE_ACTIVATING);

  // get QFIs associated with PDU session ID
  std::vector<smf_qos_flow> qos_flows = {};
  sp.get()->get_qos_flows(qos_flows);
  for (auto i : qos_flows) {
    sm_context_request.get()->req.add_qfi(i.qfi.qfi);

    qos_flow_context_updated qcu = {};
    qcu.set_cause(
        static_cast<uint8_t>(cause_value_5gsm_e::CAUSE_255_REQUEST_ACCEPTED));
    qcu.set_qfi(i.qfi);
    qcu.set_ul_fteid(i.ul_fteid);
    qcu.set_qos_profile(i.qos_profile);
    sm_context_resp.get()->res.add_qos_flow_context_updated(qcu);
  }

  sm_context_resp.get()->session_procedure_type =
      session_management_procedures_type_e::SERVICE_REQUEST_UE_TRIGGERED_STEP1;

  // Create N2 SM Information: PDU Session Resource Setup Request Transfer IE
  // N2 SM Information
  smf_n2::get_instance().create_n2_pdu_session_resource_setup_request_transfer(
      sm_context_resp.get()->res, n2_sm_info_type_e::PDU_RES_SETUP_REQ,
      n2_sm_info);

  smf_app_inst->convert_string_2_hex(n2_sm_info, n2_sm_info_hex);
  sm_context_resp.get()->res.set_n2_sm_information(n2_sm_info_hex);

  // fill the content of SmContextUpdatedData
  nlohmann::json json_data                           = {};
  json_data["n2InfoContainer"]["n2InformationClass"] = N1N2_MESSAGE_CLASS;
  json_data["n2InfoContainer"]["smInfo"]["PduSessionId"] =
      sm_context_resp.get()->res.get_pdu_session_id();
  json_data["n2InfoContainer"]["smInfo"]["n2InfoContent"]["ngapData"]
           ["contentId"] = N2_SM_CONTENT_ID;
  json_data["n2InfoContainer"]["smInfo"]["n2InfoContent"]["ngapIeType"] =
      "PDU_RES_SETUP_REQ";  // NGAP message
  json_data["upCnxState"] = "ACTIVATING";
  sm_context_resp.get()->res.set_json_data(json_data);

  // Update upCnxState to ACTIVATING
  sp.get()->set_upCnx_state(upCnx_state_e::UPCNX_STATE_ACTIVATING);

  // TODO: If new UPF is used, need to send N4 Session Modification
  // Request/Response to new/old UPF

  // Accept the activation of UP connection and continue to using the current
  // UPF
  // TODO: Accept the activation of UP connection and select a new UPF
  // Reject the activation of UP connection
  // SMF fails to find a suitable I-UPF: i) trigger re-establishment of PDU
  // Session;  or ii) keep PDU session but reject the activation of UP
  // connection;  or iii) release PDU session

  return true;
}

//-------------------------------------------------------------------------------------
void smf_context::handle_pdu_session_update_sm_context_request(
    std::shared_ptr<itti_n11_update_sm_context_request> smreq) {

}

//-------------------------------------------------------------------------------------
void smf_context::handle_pdu_session_release_sm_context_request(
    std::shared_ptr<itti_n11_release_sm_context_request> smreq) {
  
}

//------------------------------------------------------------------------------
void smf_context::handle_pdu_session_modification_network_requested(
    std::shared_ptr<itti_nx_trigger_pdu_session_modification> itti_msg) {
  
}

//------------------------------------------------------------------------------
void smf_context::insert_dnn_subscription(
    const snssai_t& snssai,
    std::shared_ptr<session_management_subscription>& ss) {
  std::unique_lock<std::recursive_mutex> lock(m_context);

  dnn_subscriptions[(uint8_t) snssai.sST] = ss;
  Logger::smf_app().info(
      "Inserted DNN Subscription, key: %d", (uint8_t) snssai.sST);
}

//------------------------------------------------------------------------------
void smf_context::insert_dnn_subscription(
    const snssai_t& snssai, const std::string& dnn,
    std::shared_ptr<session_management_subscription>& ss) {
  
}

//------------------------------------------------------------------------------
bool smf_context::is_dnn_snssai_subscription_data(
    const std::string& dnn, const snssai_t& snssai) {
  std::unique_lock<std::recursive_mutex> lock(m_context);
  if (dnn_subscriptions.count((uint8_t) snssai.sST) > 0) {
    std::shared_ptr<session_management_subscription> ss =
        dnn_subscriptions.at((uint8_t) snssai.sST);
    if (ss.get()->dnn_configuration(dnn))
      return true;
    else
      return false;
  }
  return false;
}

//------------------------------------------------------------------------------
bool smf_context::find_dnn_subscription(
    const snssai_t& snssai,
    std::shared_ptr<session_management_subscription>& ss) {
  Logger::smf_app().info(
      "Find a DNN Subscription with key: %d, map size %d", (uint8_t) snssai.sST,
      dnn_subscriptions.size());
  std::unique_lock<std::recursive_mutex> lock(m_context);
  if (dnn_subscriptions.count((uint8_t) snssai.sST) > 0) {
    ss = dnn_subscriptions.at((uint8_t) snssai.sST);
    return true;
  }

  Logger::smf_app().info(
      "DNN subscription (SNSSAI %d) not found", (uint8_t) snssai.sST);
  return false;
}

//------------------------------------------------------------------------------
bool smf_context::find_dnn_context(
    const snssai_t& nssai, const std::string& dnn,
    std::shared_ptr<dnn_context>& dnn_context) {
  std::unique_lock<std::recursive_mutex> lock(m_context);
  for (auto it : dnns) {
    if ((0 == dnn.compare(it->dnn_in_use)) and
        ((uint8_t) nssai.sST) == (uint8_t)(it->nssai.sST)) {
      dnn_context = it;
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------------
void smf_context::insert_dnn(std::shared_ptr<dnn_context>& sd) {
  std::unique_lock<std::recursive_mutex> lock(m_context);
  dnns.push_back(sd);
}

//------------------------------------------------------------------------------
bool smf_context::verify_sm_context_request(
    std::shared_ptr<itti_n11_create_sm_context_request> smreq) {
  // check the validity of the UE request according to the user subscription or
  // local policies
  // TODO: need to be implemented
  return true;
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
std::size_t smf_context::get_number_dnn_contexts() const {
  return dnns.size();
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

//-----------------------------------------------------------------------------
bool smf_context::find_pdu_session(
    const pfcp::pdr_id_t& pdr_id, pfcp::qfi_t& qfi,
    std::shared_ptr<dnn_context>& sd, std::shared_ptr<smf_pdu_session>& sp) {
  std::unique_lock<std::recursive_mutex> lock(m_context);
  for (auto it : dnns) {
    for (auto session : it.get()->pdu_sessions) {
      smf_qos_flow flow = {};
      if (session->find_qos_flow(pdr_id, flow)) {
        qfi.qfi = flow.qfi.qfi;
        sp      = session;
        sd      = it;
        return true;
      }
    }
  }
  return false;
}

//------------------------------------------------------------------------------
void smf_context::handle_sm_context_status_change(
    scid_t scid, const std::string& status, uint8_t http_version) {
  
}

//------------------------------------------------------------------------------
void smf_context::handle_ee_pdu_session_release(
    supi64_t supi, pdu_session_id_t pdu_session_id, uint8_t http_version) {
  
}

//------------------------------------------------------------------------------
void smf_context::update_qos_info(
    std::shared_ptr<smf_pdu_session>& sp,
    smf::pdu_session_update_sm_context_response& res,
    const nas_message_t& nas_msg) {
  // Process QoS rules and Qos Flow descriptions
  uint16_t length_of_rule_ie = nas_msg.plain.sm.pdu_session_modification_request
                                   .qosrules.lengthofqosrulesie;

  pfcp::qfi_t generated_qfi = {.qfi = 0};

  // QOSFlowDescriptions
  uint8_t number_of_flow_descriptions =
      nas_msg.plain.sm.pdu_session_modification_request.qosflowdescriptions
          .qosflowdescriptionsnumber;
  QOSFlowDescriptionsContents qos_flow_description_content = {};

  // Only one flow description for new requested QoS Flow
  if (number_of_flow_descriptions > 0) {
    for (int i = 0; i < number_of_flow_descriptions; i++) {
      if (nas_msg.plain.sm.pdu_session_modification_request.qosflowdescriptions
              .qosflowdescriptionscontents[i]
              .qfi == NO_QOS_FLOW_IDENTIFIER_ASSIGNED) {
        // TODO: generate new QFI
        generated_qfi.qfi = (uint8_t) 60;  // hardcoded for now
        qos_flow_description_content =
            nas_msg.plain.sm.pdu_session_modification_request
                .qosflowdescriptions.qosflowdescriptionscontents[i];
        qos_flow_description_content.qfi = generated_qfi.qfi;
        break;
      }
    }
  }

  int i              = 0;
  int length_of_rule = 0;
  while (length_of_rule_ie > 0) {
    QOSRulesIE qos_rules_ie = {};
    qos_rules_ie = nas_msg.plain.sm.pdu_session_modification_request.qosrules
                       .qosrulesie[i];
    uint8_t rule_id       = {0};
    pfcp::qfi_t qfi       = {};
    smf_qos_flow qos_flow = {};
    length_of_rule        = qos_rules_ie.LengthofQoSrule;

    // If UE requested a new GBR flow
    if ((qos_rules_ie.ruleoperationcode == CREATE_NEW_QOS_RULE) and
        (qos_rules_ie.segregation == SEGREGATION_REQUESTED)) {
      // Add a new QoS Flow
      if (qos_rules_ie.qosruleidentifer == NO_QOS_RULE_IDENTIFIER_ASSIGNED) {
        // Generate a new QoS rule
        sp.get()->generate_qos_rule_id(rule_id);
        Logger::smf_app().info("Create a new QoS rule (rule Id %d)", rule_id);
        qos_rules_ie.qosruleidentifer = rule_id;
      }
      sp.get()->add_qos_rule(qos_rules_ie);

      qfi.qfi      = generated_qfi.qfi;
      qos_flow.qfi = generated_qfi.qfi;

      // set qos_profile from qos_flow_description_content
      qos_flow.qos_profile = {};

      for (int j = 0; j < qos_flow_description_content.numberofparameters;
           j++) {
        if (qos_flow_description_content.parameterslist[j]
                .parameteridentifier == PARAMETER_IDENTIFIER_5QI) {
          qos_flow.qos_profile._5qi =
              qos_flow_description_content.parameterslist[j]
                  .parametercontents._5qi;
        } else if (
            qos_flow_description_content.parameterslist[j]
                .parameteridentifier == PARAMETER_IDENTIFIER_GFBR_UPLINK) {
          qos_flow.qos_profile.parameter.qos_profile_gbr.gfbr.uplink.unit =
              qos_flow_description_content.parameterslist[j]
                  .parametercontents.gfbrormfbr_uplinkordownlink.uint;
          qos_flow.qos_profile.parameter.qos_profile_gbr.gfbr.uplink.value =
              qos_flow_description_content.parameterslist[j]
                  .parametercontents.gfbrormfbr_uplinkordownlink.value;
        } else if (
            qos_flow_description_content.parameterslist[j]
                .parameteridentifier == PARAMETER_IDENTIFIER_GFBR_DOWNLINK) {
          qos_flow.qos_profile.parameter.qos_profile_gbr.gfbr.donwlink.unit =
              qos_flow_description_content.parameterslist[j]
                  .parametercontents.gfbrormfbr_uplinkordownlink.uint;
          qos_flow.qos_profile.parameter.qos_profile_gbr.gfbr.donwlink.value =
              qos_flow_description_content.parameterslist[j]
                  .parametercontents.gfbrormfbr_uplinkordownlink.value;
        } else if (
            qos_flow_description_content.parameterslist[j]
                .parameteridentifier == PARAMETER_IDENTIFIER_MFBR_UPLINK) {
          qos_flow.qos_profile.parameter.qos_profile_gbr.mfbr.uplink.unit =
              qos_flow_description_content.parameterslist[j]
                  .parametercontents.gfbrormfbr_uplinkordownlink.uint;
          qos_flow.qos_profile.parameter.qos_profile_gbr.mfbr.uplink.value =
              qos_flow_description_content.parameterslist[j]
                  .parametercontents.gfbrormfbr_uplinkordownlink.value;
        } else if (
            qos_flow_description_content.parameterslist[j]
                .parameteridentifier == PARAMETER_IDENTIFIER_MFBR_DOWNLINK) {
          qos_flow.qos_profile.parameter.qos_profile_gbr.mfbr.donwlink.unit =
              qos_flow_description_content.parameterslist[j]
                  .parametercontents.gfbrormfbr_uplinkordownlink.uint;
          qos_flow.qos_profile.parameter.qos_profile_gbr.mfbr.donwlink.value =
              qos_flow_description_content.parameterslist[j]
                  .parametercontents.gfbrormfbr_uplinkordownlink.value;
        }
      }

      Logger::smf_app().debug(
          "Add new QoS Flow with new QRI %d", qos_rules_ie.qosruleidentifer);
      // mark this rule to be synchronised with the UE
      sp.get()->update_qos_rule(qos_rules_ie);
      // Add new QoS flow
      sp.get()->add_qos_flow(qos_flow);

      // ADD QoS Flow to be updated
      qos_flow_context_updated qcu = {};
      qcu.set_qfi(pfcp::qfi_t(qos_flow.qfi));
      // qcu.set_ul_fteid(flow.ul_fteid);
      // qcu.set_dl_fteid(flow.dl_fteid);
      qcu.set_qos_profile(qos_flow.qos_profile);
      res.add_qos_flow_context_updated(qcu);

    } else {  // update existing QRI
      Logger::smf_app().debug(
          "Update existing QRI %d", qos_rules_ie.qosruleidentifer);
      qfi.qfi = qos_rules_ie.qosflowidentifer;
      if (sp.get()->get_qos_flow(qfi, qos_flow)) {
        sp.get()->update_qos_rule(qos_rules_ie);
        // update QoS flow
        sp.get()->add_qos_flow(qos_flow);

        // ADD QoS Flow to be updated
        qos_flow_context_updated qcu = {};
        qcu.set_qfi(pfcp::qfi_t(qos_flow.qfi));
        qcu.set_ul_fteid(qos_flow.ul_fteid);
        qcu.set_dl_fteid(qos_flow.dl_fteid);
        qcu.set_qos_profile(qos_flow.qos_profile);
        res.add_qos_flow_context_updated(qcu);
      }
    }
    length_of_rule_ie -= (length_of_rule + 3);  // 2 for Length of QoS
                                                // rules IE and 1 for QoS
                                                // rule identifier
    i++;
  }
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

//------------------------------------------------------------------------------
bool dnn_context::find_pdu_session(
    const uint32_t pdu_session_id,
    std::shared_ptr<smf_pdu_session>& pdu_session) {
  pdu_session = {};
  std::shared_lock lock(m_context);
  for (auto it : pdu_sessions) {
    if (pdu_session_id == it->pdu_session_id) {
      pdu_session = it;
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------------
void dnn_context::insert_pdu_session(std::shared_ptr<smf_pdu_session>& sp) {
  std::unique_lock lock(m_context);
  pdu_sessions.push_back(sp);
}

//------------------------------------------------------------------------------
bool dnn_context::remove_pdu_session(const uint32_t pdu_session_id) {
  std::unique_lock lock(m_context);
  for (auto it = pdu_sessions.begin(); it != pdu_sessions.end(); ++it) {
    if (pdu_session_id == (*it).get()->pdu_session_id) {
      (*it).get()->remove_qos_flows();
      (*it).get()->clear();
      pdu_sessions.erase(it);
      return true;
    }
  }
  return false;
}
//------------------------------------------------------------------------------
size_t dnn_context::get_number_pdu_sessions() const {
  std::shared_lock lock(m_context);
  return pdu_sessions.size();
}

//------------------------------------------------------------------------------
std::string dnn_context::toString() const {
  std::string s = {};
  s.append("DNN CONTEXT:\n");
  s.append("\tIn use:\t\t\t\t").append(std::to_string(in_use)).append("\n");
  s.append("\tDNN:\t\t\t\t").append(dnn_in_use).append("\n");
  for (auto it : pdu_sessions) {
    s.append(it->toString());
  }
  return s;
}
