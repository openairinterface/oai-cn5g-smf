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

/*! \file session_handler.cpp
 \brief
 \author  Lukas ROTHENEDER, Stefan SPETTEL
 \company phine.tech
 \date 2024
 \email:  lukas.rotheneder@phine.tech, stefan.spettel@phine.tech
 */

#include "session_handler.hpp"
#include "FlowDirection.h"

using namespace smf;
using namespace oai::model::pcf;
using namespace oai::utils;

void session_handler::set_session_graph(
    const std::shared_ptr<upf_graph>& upf_graph) {
  m_session_graph = upf_graph;
}

std::shared_ptr<upf_graph> session_handler::get_session_graph() const {
  return m_session_graph;
}

bool session_handler::has_session_graph() {
  return m_session_graph != nullptr;
}

qos_flow_context_updated session_handler::get_qos_flow_context_updated(
    const pfcp::qfi_t& qfi) {
  if (!has_session_graph()) {
    Logger::smf_app().error(
        "Cannot receive QoS flow because UPF graph does not exist");
    return qos_flow_context_updated{};
  }

  auto edge = get_edge_for_qfi(qfi.qfi);
  if (edge) {
    qos_flow_context_updated flow;
    flow.qfi         = edge->qfi;
    flow.qos_profile = edge->qos_profile;
    flow.cause_value = static_cast<uint8_t>(m_cause_value);
    flow.set_dl_fteid(edge->next_hop_fteid);
    flow.set_ul_fteid(edge->fteid);

    // add QoS rule to flow
    flow.add_qos_rule(qos_rule_from_edge(edge));

    return flow;
  }
  Logger::smf_app().error(
      "Cannot receive QoS flow for QFI %u, it does not exist", qfi.qfi);
  qos_flow_context_updated flow;
  flow.cause_value = static_cast<uint8_t>(
      cause_value_5gsm_e::CAUSE_31_REQUEST_REJECTED_UNSPECIFIED);
  return qos_flow_context_updated{};
}

std::vector<::smf::qos_flow_context_updated>
session_handler::get_qos_flows_context_updated() {
  std::vector<::smf::qos_flow_context_updated> flows;
  flows.reserve(m_qfis_to_be_updated.size());
  for (const auto& qfi : m_qfis_to_be_updated) {
    flows.push_back(get_qos_flow_context_updated(qfi));
  }

  return flows;
}

void session_handler::set_qfis_to_be_updated(
    const std::vector<pfcp::qfi_t>& qfis) {
  m_qfis_to_be_updated = qfis;
}

void session_handler::set_cause(const cause_value_5gsm_e& cause) {
  m_cause_value = cause;
}

// this code is really ugly, as soon as we refactor NAS, we have to refactor
// this as well
void session_handler::set_nas_filter_from_edge(
    const shared_ptr<qos_upf_edge>& edge, QOSRulesIE& qos_rule) {
  auto flow                      = edge->flow_information;
  qos_rule.numberofpacketfilters = 0;

  if (!flow.flowDescriptionIsSet() || !flow.isPacketFilterUsage()) {
    return;
  }
  bool ue_rule;

  switch (flow.getFlowDirection().getEnumValue()) {
    case FlowDirection_anyOf::eFlowDirection_anyOf::
        INVALID_VALUE_OPENAPI_GENERATED:
    case FlowDirection_anyOf::eFlowDirection_anyOf::NULL_VALUE:
    case FlowDirection_anyOf::eFlowDirection_anyOf::DOWNLINK:
    case FlowDirection_anyOf::eFlowDirection_anyOf::UNSPECIFIED:
      ue_rule = false;
      break;
    case FlowDirection_anyOf::eFlowDirection_anyOf::UPLINK:
    case FlowDirection_anyOf::eFlowDirection_anyOf::BIDIRECTIONAL:
      ue_rule = true;
      break;
  }
  if (!ue_rule) return;

  // TODO for real QoS, we should parse SDF filter here
  // Note: work already in progress, but let's merge this first

  // TODO really not nice to do calloc in this deep service layer, that should
  // all be part of protocol
  // TODO also, free on this is never called as far as I can see that!!!
  // Design decision: We always only supply one packet filter for now
  qos_rule.numberofpacketfilters = 1;
  qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace =
      (Create_ModifyAndAdd_ModifyAndReplace*) calloc(
          1, sizeof(Create_ModifyAndAdd_ModifyAndReplace));
  qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace[0]
      .packetfilterdirection = 0b10;  // always uplink
  qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace[0]
      .packetfilteridentifier = 1;
  qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace[0]
      .packetfiltercontents.component_type = QOS_RULE_MATCHALL_TYPE;
  // qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace[0].packetfiltercontents.component_value
  // = bfromcstralloc(2, "\0");
}

// Comments about architecture
// IMO this part should not be here at all, we should have some "common" DTO
// that is used between all layers and the low-level malloc/free stuff here
// should be handled on protocol-level (N1/N2) but the refactor would go too
// far, so we keep this QOSRuleIE here also, the mapping between SDF filter and
// the packet filter here should not be here
//------------------------------------------------------------------------------
QOSRulesIE session_handler::qos_rule_from_edge(
    const shared_ptr<qos_upf_edge>& edge) {
  QOSRulesIE qos_rule;

  if (edge->qos_rule_id == 0) {
    edge->qos_rule_id = generate_qos_rule_id();
  }

  // TODO check that number of QoS rules does not exceed what UE can do
  // see section 5.7.1.4 @ 3GPP TS 23.501
  qos_rule.qosruleidentifer  = edge->qos_rule_id;
  qos_rule.ruleoperationcode = CREATE_NEW_QOS_RULE;
  if (edge->default_qos) {
    qos_rule.dqrbit = THE_QOS_RULE_IS_DEFAULT_QOS_RULE;
  } else {
    qos_rule.dqrbit = THE_QOS_RULE_IS_NOT_THE_DEFAULT_QOS_RULE;
  }

  if (m_pdu_session_type != PDU_SESSION_TYPE_E_UNSTRUCTURED) {
    set_nas_filter_from_edge(edge, qos_rule);
  } else {
    qos_rule.numberofpacketfilters = 0;
  }

  Logger::smf_n1().debug(
      "Created new QoS rule with ID %u and %u packet filters",
      edge->qos_rule_id, qos_rule.numberofpacketfilters);

  qos_rule.qosruleprecedence = edge->precedence;
  qos_rule.segregation       = SEGREGATION_NOT_REQUESTED;
  qos_rule.qosflowidentifer  = edge->qfi.qfi;

  return qos_rule;
}

pfcp::urr_id_t session_handler::generate_urr_id() {
  pfcp::urr_id_t urr_id;
  urr_id.urr_id = m_urr_id_generator.get_uid();
  return urr_id;
}

//------------------------------------------------------------------------------
void session_handler::release_urr_id(const pfcp::urr_id_t& urr_id) {
  m_urr_id_generator.free_uid(urr_id.urr_id);
}

//------------------------------------------------------------------------------
pfcp::far_id_t session_handler::generate_far_id() {
  pfcp::far_id_t far_id;
  far_id.far_id = m_far_id_generator.get_uid();
  return far_id;
}

//------------------------------------------------------------------------------
void session_handler::release_far_id(const pfcp::far_id_t& far_id) {
  m_far_id_generator.free_uid(far_id.far_id);
}

pfcp::pdr_id_t session_handler::generate_pdr_id() {
  pfcp::pdr_id_t pdr_id;
  pdr_id.rule_id = m_pdr_id_generator.get_uid();
  return pdr_id;
}

void session_handler::release_pdr_id(const pfcp::pdr_id_t& pdr_id) {
  m_pdr_id_generator.free_uid(pdr_id.rule_id);
}

uint8_t session_handler::generate_qos_rule_id() {
  return m_qos_rule_id_generator.get_uid();
}

void session_handler::release_qos_rule_id(const uint8_t& rule_id) {
  m_qos_rule_id_generator.free_uid(rule_id);
}

// TODO unify with QoS handling here, we should also update the edges and QoS
// flows
void session_handler::add_qos_rule(const QOSRulesIE& qos_rule) {
  std::unique_lock lock(
      m_session_handler_mutex,
      std::defer_lock);  // Do not lock it first
  Logger::smf_app().info(
      "Add QoS Rule with Rule Id %d", (uint8_t) qos_rule.qosruleidentifer);
  uint8_t rule_id = qos_rule.qosruleidentifer;

  if ((rule_id >= QOS_RULE_IDENTIFIER_FIRST) and
      (rule_id <= QOS_RULE_IDENTIFIER_LAST)) {
    if (m_qos_rules.count(rule_id) > 0) {
      Logger::smf_app().error(
          "Failed to add rule (Id %d), rule existed", rule_id);
    } else {
      lock.lock();  // Lock it here
      m_qos_rules.insert(std::pair<uint8_t, QOSRulesIE>(rule_id, qos_rule));
      Logger::smf_app().trace(
          "Rule (Id %d) has been added successfully", rule_id);
    }

  } else {
    Logger::smf_app().error(
        "Failed to add rule (Id %d) failed: invalid rule Id", rule_id);
  }
}

qos_flow_context_updated session_handler::create_new_qos_rule(
    QOSRulesIE& qos_rules_ie,
    const QOSFlowDescriptionsContents& qos_flow_description_content) {
  qos_flow_context_updated qos_flow;

  // Add a new QoS Flow
  if (qos_rules_ie.qosruleidentifer == NO_QOS_RULE_IDENTIFIER_ASSIGNED) {
    // Generate a new QoS rule
    uint8_t rule_id = generate_qos_rule_id();
    Logger::smf_app().info("Create a new QoS rule (rule Id %d)", rule_id);
    qos_rules_ie.qosruleidentifer = rule_id;
  }
  add_qos_rule(qos_rules_ie);
  // TODO unify with generated_qfi, hardcode for now (like it used to be)

  qos_flow.qfi = (uint8_t) 60;

  // set qos_profile from qos_flow_description_content
  qos_flow.qos_profile = {};

  for (int j = 0; j < qos_flow_description_content.numberofparameters; j++) {
    if (qos_flow_description_content.parameterslist[j].parameteridentifier ==
        PARAMETER_IDENTIFIER_5QI) {
      qos_flow.qos_profile._5qi =
          qos_flow_description_content.parameterslist[j].parametercontents._5qi;
    } else if (
        qos_flow_description_content.parameterslist[j].parameteridentifier ==
        PARAMETER_IDENTIFIER_GFBR_UPLINK) {
      qos_flow.qos_profile.parameter.qos_profile_gbr.gfbr.uplink.unit =
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.uint;
      qos_flow.qos_profile.parameter.qos_profile_gbr.gfbr.uplink.value =
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.value;
    } else if (
        qos_flow_description_content.parameterslist[j].parameteridentifier ==
        PARAMETER_IDENTIFIER_GFBR_DOWNLINK) {
      qos_flow.qos_profile.parameter.qos_profile_gbr.gfbr.donwlink.unit =
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.uint;
      qos_flow.qos_profile.parameter.qos_profile_gbr.gfbr.donwlink.value =
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.value;
    } else if (
        qos_flow_description_content.parameterslist[j].parameteridentifier ==
        PARAMETER_IDENTIFIER_MFBR_UPLINK) {
      qos_flow.qos_profile.parameter.qos_profile_gbr.mfbr.uplink.unit =
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.uint;
      qos_flow.qos_profile.parameter.qos_profile_gbr.mfbr.uplink.value =
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.value;
    } else if (
        qos_flow_description_content.parameterslist[j].parameteridentifier ==
        PARAMETER_IDENTIFIER_MFBR_DOWNLINK) {
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
  update_qos_rule(qos_rules_ie);
  // Add new QoS flow
  // TODO here interact with graph
  // or add a new QFI to be updated or whatever, that should be done out from
  // the context and start a graph- related procedure
  // sp->get_sessions_graph()->add_qos_flow(qos_flow);
  qos_flow.set_qfi(pfcp::qfi_t(qos_flow.qfi));

  return qos_flow;
}

std::vector<nlohmann::json> session_handler::create_qos_flows_json() {
  std::vector<nlohmann::json> qos_flows;
  for (const auto& qfi : get_all_qfis()) {
    auto edge = get_edge_for_qfi(qfi.qfi);
    if (edge) {
      nlohmann::json qos_flow_json = {};
      qos_flow_json["qfi"]         = qfi.qfi;
      // access edge is always downlink, so we know this is the DL FTEID
      if (edge->fteid.v4) {
        qos_flow_json["upf_addr"]["ipv4"] =
            conv::toString(edge->fteid.ipv4_address);
      }
      if (edge->fteid.v6) {
        qos_flow_json["upf_addr"]["ipv6"] =
            conv::toString(edge->fteid.ipv6_address);
      }
      // copy-pasta, not nice
      auto other_edge = edge->associated_edge;
      if (other_edge->fteid.v4) {
        qos_flow_json["an_addr"]["ipv4"] =
            conv::toString(other_edge->fteid.ipv4_address);
      }
      if (other_edge->fteid.v6) {
        qos_flow_json["an_addr"]["ipv6"] =
            conv::toString(other_edge->fteid.ipv6_address);
      }
      qos_flows.push_back(qos_flow_json);
    }
  }
  return qos_flows;
}

std::shared_ptr<qos_upf_edge> session_handler::get_edge_for_qfi(uint8_t qfi) {
  auto n3_edges = m_session_graph->get_access_edges();

  for (const auto& edge : n3_edges) {
    if (qfi == edge->qfi.qfi) {
      return edge;
    }
  }

  return nullptr;
}

void session_handler::deallocate_resources() {
  for (const auto& edge : m_session_graph->get_access_edges()) {
    release_pdr_id(edge->pdr_id);
    release_far_id(edge->far_id);
    release_urr_id(edge->urr_id);
    release_qos_rule_id(edge->qos_rule_id);
    edge->clear_session();
  }
}
std::vector<pfcp::qfi_t> session_handler::get_all_qfis() {
  std::set<uint8_t> qfis;
  std::vector<pfcp::qfi_t> pfcp_qfis;
  // this may be called when session graph is not set due to error procedure
  if (!m_session_graph) {
    return pfcp_qfis;
  }
  for (const auto& edge : m_session_graph->get_access_edges()) {
    // in case of double edges with same QFI (e.g. UL CL or redundant
    // transport), set eliminates duplicates
    if (edge->qfi.qfi != 0) {
      qfis.insert(edge->qfi.qfi);
    }
  }
  for (const auto& qfi : qfis) {
    pfcp::qfi_t pfcp_qfi;
    pfcp_qfi.qfi = qfi;
    pfcp_qfis.push_back(pfcp_qfi);
  }
  return pfcp_qfis;
}

bool session_handler::qfi_exists(const pfcp::qfi_t& qfi) {
  auto all_qfis = get_all_qfis();
  auto it       = std::find(all_qfis.begin(), all_qfis.end(), qfi);
  return it != all_qfis.end();
}

qos_flow_context_updated session_handler::update_qos_rule(
    QOSRulesIE qos_rules_ie) {
  qos_flow_context_updated flow{};

  std::unique_lock lock(
      m_session_handler_mutex,
      std::defer_lock);  // Do not lock it first

  auto edge = get_edge_for_qfi(qos_rules_ie.qosruleidentifer);

  // TODO there is absolutely no link between this and the QoS rules
  flow = get_qos_flow_context_updated(edge->qfi);

  if (edge) {
    Logger::smf_app().debug(
        "Update existing QRI %d", qos_rules_ie.qosruleidentifer);

    uint8_t rule_id = qos_rules_ie.qosruleidentifer;
    if ((rule_id >= QOS_RULE_IDENTIFIER_FIRST) and
        (rule_id <= QOS_RULE_IDENTIFIER_LAST)) {
      if (m_qos_rules.count(rule_id) > 0) {
        lock.lock();  // Lock it here
        m_qos_rules.erase(rule_id);
        m_qos_rules.insert(
            std::pair<uint8_t, QOSRulesIE>(rule_id, qos_rules_ie));
        // marked to be synchronised with UE
        m_qos_rules_to_be_synchronised.push_back(rule_id);
        Logger::smf_app().trace("Update QoS rule (%d) success", rule_id);
      } else {
        Logger::smf_app().error(
            "Update QoS Rule (%d) failed, rule does not existed", rule_id);
      }

    } else {
      Logger::smf_app().error(
          "Update QoS rule (%d) failed, invalid Rule Id", rule_id);
    }
  } else {
    Logger::smf_app().error(
        "Want to update QoS rule ID %ud for QFI %ud, but QFI does not exist",
        qos_rules_ie.qosruleidentifer, qos_rules_ie.qosflowidentifer);
  }

  return flow;
}

std::vector<QOSRulesIE> session_handler::get_qos_rules() {
  std::vector<QOSRulesIE> qos_rules;
  for (const auto& flow : get_qos_flows_context_updated()) {
    for (const auto& [rule_id, rule] : flow.qos_rules) {
      qos_rules.push_back(rule);
    }
  }
  return qos_rules;
}
