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
  return false;
}

//------------------------------------------------------------------------------
void smf_pdu_session::update_qos_rule(const QOSRulesIE& qos_rule) {

}

//------------------------------------------------------------------------------
void smf_pdu_session::mark_qos_rule_to_be_synchronised(const uint8_t rule_id) {

}

//------------------------------------------------------------------------------
void smf_pdu_session::add_qos_rule(const QOSRulesIE& qos_rule) {

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
  
}

//------------------------------------------------------------------------------
void smf_context::get_default_qos_rule(
    QOSRulesIE& qos_rule, uint8_t pdu_session_type) {
  
}

//------------------------------------------------------------------------------
void smf_context::get_default_qos_flow_description(
    QOSFlowDescriptionsContents& qos_flow_description, uint8_t pdu_session_type,
    const pfcp::qfi_t& qfi) {
  
}

//------------------------------------------------------------------------------
void smf_context::get_session_ambr(
    SessionAMBR& session_ambr, const snssai_t& snssai, const std::string& dnn) {
  
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
