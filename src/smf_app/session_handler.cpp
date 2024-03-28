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
#include "conversions.hpp"

using namespace smf;
using namespace oai::model::pcf;
using namespace oai::utils::conversions;

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

    // add flow_description_content
    flow.set_qos_flow_descriptions(qos_flow_description_from_edge(edge));

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

  //-----------------------------------------------------------------------------------------------------------------
  // Hardcoded second flow TODO remove after smf-pcf communication is done
  //

  /*
  auto flow             = get_qos_flow_context_updated(m_qfis_to_be_updated[0]);
  flow.qfi.qfi          = 2;
  flow.qos_profile._5qi = 2;
  flow.qos_profile.arp.priority_level = 1;
  flow.qos_profile.profile_type       = qos_profile_type_e::GBR;
  flow.qos_profile.parameter.qos_profile_gbr.mfbr.uplink.value   = 20;
  flow.qos_profile.parameter.qos_profile_gbr.mfbr.uplink.unit    = 6;
  flow.qos_profile.parameter.qos_profile_gbr.mfbr.donwlink.value = 60;
  flow.qos_profile.parameter.qos_profile_gbr.mfbr.donwlink.unit  = 6;
  flow.qos_profile.parameter.qos_profile_gbr.gfbr.uplink.value   = 15;
  flow.qos_profile.parameter.qos_profile_gbr.gfbr.uplink.unit    = 6;
  flow.qos_profile.parameter.qos_profile_gbr.gfbr.donwlink.value = 15;
  flow.qos_profile.parameter.qos_profile_gbr.gfbr.donwlink.unit  = 6;

  flow.qos_rules.clear();
  QOSRulesIE qos_rule;
  qos_rule.qosruleidentifer      = 2;
  qos_rule.ruleoperationcode     = CREATE_NEW_QOS_RULE;
  qos_rule.dqrbit                = THE_QOS_RULE_IS_NOT_THE_DEFAULT_QOS_RULE;
  qos_rule.numberofpacketfilters = 0;
  qos_rule.qosruleprecedence     = 0;
  qos_rule.segregation           = SEGREGATION_NOT_REQUESTED;
  qos_rule.qosflowidentifer      = 2;
  flow.qos_rules.insert(std::pair<uint8_t, QOSRulesIE>(2, qos_rule));

  flow.qos_flow_description_content.qfi = 2;

  flow.qos_flow_description_content.numberofparameters = 5;
  flow.qos_flow_description_content.parameterslist =
      (ParametersList*) calloc(5, sizeof(ParametersList));
  flow.qos_flow_description_content.parameterslist[0].parameteridentifier =
      PARAMETER_IDENTIFIER_5QI;
  flow.qos_flow_description_content.parameterslist[0].parametercontents._5qi =
      2;

  flow.qos_flow_description_content.parameterslist[1].parameteridentifier =
      PARAMETER_IDENTIFIER_GFBR_UPLINK;
  flow.qos_flow_description_content.parameterslist[1]
      .parametercontents.gfbrormfbr_uplinkordownlink.uint =
      flow.qos_profile.parameter.qos_profile_gbr.gfbr.uplink.unit;
  flow.qos_flow_description_content.parameterslist[1]
      .parametercontents.gfbrormfbr_uplinkordownlink.value =
      flow.qos_profile.parameter.qos_profile_gbr.gfbr.uplink.value;

  // TODO: If more than 2 parameters are set, the gnbsim crashes.
  flow.qos_flow_description_content.parameterslist[2].parameteridentifier =
  PARAMETER_IDENTIFIER_GFBR_DOWNLINK;
  flow.qos_flow_description_content.parameterslist[2].parametercontents.gfbrormfbr_uplinkordownlink.uint
  = flow.qos_profile.parameter.qos_profile_gbr.gfbr.donwlink.unit;
  flow.qos_flow_description_content.parameterslist[2].parametercontents.gfbrormfbr_uplinkordownlink.value
  = flow.qos_profile.parameter.qos_profile_gbr.gfbr.donwlink.value;

  flow.qos_flow_description_content.parameterslist[3].parameteridentifier =
  PARAMETER_IDENTIFIER_MFBR_UPLINK;
  flow.qos_flow_description_content.parameterslist[3].parametercontents.gfbrormfbr_uplinkordownlink.uint
  = flow.qos_profile.parameter.qos_profile_gbr.mfbr.uplink.unit;
  flow.qos_flow_description_content.parameterslist[3].parametercontents.gfbrormfbr_uplinkordownlink.value
  = flow.qos_profile.parameter.qos_profile_gbr.mfbr.uplink.value;

  flow.qos_flow_description_content.parameterslist[4].parameteridentifier =
  PARAMETER_IDENTIFIER_MFBR_DOWNLINK;
  flow.qos_flow_description_content.parameterslist[4].parametercontents.gfbrormfbr_uplinkordownlink.uint
  = flow.qos_profile.parameter.qos_profile_gbr.mfbr.donwlink.unit;
  flow.qos_flow_description_content.parameterslist[4].parametercontents.gfbrormfbr_uplinkordownlink.value
  = flow.qos_profile.parameter.qos_profile_gbr.mfbr.donwlink.value;

  flows.push_back(flow);*/
  //------------------------------------------------------------------------------------------------------------------

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
  if (flow.getFlowDescription().empty() || !ue_rule) {
    Logger::smf_app().warn(
        "Flow description is empty for rule: %s", flow.getFlowDescription());
    return;
  }

  auto parsed_filter = sdf_filter::from_string(flow.getFlowDescription());

  // TODO really not nice to do calloc in this deep service layer, that should
  // all be part of protocol
  // TODO also, free on this is never called as far as I can see that!!!
  if (edge->default_qos) {
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
  } else if (!parsed_filter.default_filter) {
    // TODO we should take into account the max number of supported packet
    // filters from UE
    qos_rule.packetfilterlist.create_modifyandadd_modifyandreplace =
        (Create_ModifyAndAdd_ModifyAndReplace*) calloc(
            parsed_filter.filter_components,
            sizeof(Create_ModifyAndAdd_ModifyAndReplace));
    qos_rule.numberofpacketfilters = parsed_filter.filter_components;
    int filter_id                  = 1;
    if (parsed_filter.use_protocol_identifier) {
      set_protocol_filter(
          filter_id,
          qos_rule.packetfilterlist
              .create_modifyandadd_modifyandreplace[filter_id - 1],
          parsed_filter.protocol_identifier);
      filter_id++;
    }
    if (parsed_filter.src_ip_range.use_ip_range) {
      set_ip_filter(
          filter_id,
          qos_rule.packetfilterlist
              .create_modifyandadd_modifyandreplace[filter_id - 1],
          parsed_filter.src_ip_range);
      filter_id++;
    }
    if (!parsed_filter.src_port_ranges.empty()) {
      for (const auto& port : parsed_filter.src_port_ranges) {
        set_port_filter(
            filter_id,
            qos_rule.packetfilterlist
                .create_modifyandadd_modifyandreplace[filter_id - 1],
            port);
        filter_id++;
      }
    }
  }
}

void session_handler::set_port_filter(
    int filter_id, Create_ModifyAndAdd_ModifyAndReplace& nas_filter,
    const port_range& port_range) {
  nas_filter.packetfilteridentifier = filter_id;
  nas_filter.packetfilterdirection  = 0b11;  // always uplink
  uint16_t port_low                 = htons(port_range.start);
  uint16_t port_high                = htons(port_range.end);

  if (port_range.is_range) {
    uint32_t int_range = 0;
    int_range          = ((uint32_t) port_high << 16) | port_low;
    nas_filter.packetfiltercontents.component_type =
        QOS_RULE_REMOTE_PORT_RANGE_TYPE;
    nas_filter.packetfiltercontents.component_value = blk2bstr(&int_range, 4);
  } else {
    nas_filter.packetfiltercontents.component_type =
        QOS_RULE_SINGLE_REMOTE_PORT_TYPE;
    nas_filter.packetfiltercontents.component_value = blk2bstr(&port_low, 2);
  }
}

void session_handler::set_ip_filter(
    int filter_id, Create_ModifyAndAdd_ModifyAndReplace& nas_filter,
    const ip_range& ip_range) {
  nas_filter.packetfilteridentifier = filter_id;
  nas_filter.packetfilterdirection  = 0b11;  // always uplink
  uint64_t ip_snm =
      ((uint64_t) ip_range.snm.s_addr << 32) | ip_range.ip_addr.s_addr;
  nas_filter.packetfiltercontents.component_type =
      QOS_RULE_IPV4_REMOTE_ADDRESS_TYPE;
  nas_filter.packetfiltercontents.component_value = blk2bstr(&ip_snm, 8);
}

void session_handler::set_protocol_filter(
    int filter_id, Create_ModifyAndAdd_ModifyAndReplace& nas_filter,
    uint8_t protocol_id) {
  nas_filter.packetfilteridentifier = filter_id;
  nas_filter.packetfilterdirection  = 0b11;  // always uplink
  nas_filter.packetfiltercontents.component_type =
      QOS_RULE_PROTOCOL_IDENTIFIERORNEXT_HEADER_TYPE;
  nas_filter.packetfiltercontents.component_value = blk2bstr(&protocol_id, 1);
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

//------------------------------------------------------------------------------
QOSFlowDescriptionsContents session_handler::qos_flow_description_from_edge(
    const shared_ptr<qos_upf_edge>& edge) {
  QOSFlowDescriptionsContents flow_description_content;

  flow_description_content.qfi           = edge->qfi.qfi;
  flow_description_content.operationcode = CREATE_NEW_QOS_FLOW_DESCRIPTION;
  flow_description_content.e             = PARAMETERS_LIST_IS_INCLUDED;

  if (edge->qos_profile.gbrUlIsSet() && edge->qos_profile.gbrDlIsSet() &&
      edge->qos_profile.maxbrUlIsSet() && edge->qos_profile.maxbrDlIsSet()) {
#if 1
    flow_description_content.numberofparameters = 5;
    flow_description_content.parameterslist     = (ParametersList*) calloc(
        7, sizeof(ParametersList));  // TODO: is 7 right?
    flow_description_content.parameterslist[0].parameteridentifier =
        PARAMETER_IDENTIFIER_5QI;
    flow_description_content.parameterslist[0].parametercontents._5qi =
        edge->qos_profile.getR5qi();

    flow_description_content.parameterslist[1].parameteridentifier =
        PARAMETER_IDENTIFIER_GFBR_UPLINK;
    flow_description_content.parameterslist[1]
        .parametercontents.gfbrormfbr_uplinkordownlink.uint =
        GFBRORMFBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
    // edge->qos_profile.parameter.qos_profile_gbr.gfbr.uplink.unit;
    flow_description_content.parameterslist[1]
        .parametercontents.gfbrormfbr_uplinkordownlink.value =
        std::stoi(edge->qos_profile.getGbrUl());
    // edge->qos_profile.parameter.qos_profile_gbr.gfbr.uplink.value;

    flow_description_content.parameterslist[2].parameteridentifier =
        PARAMETER_IDENTIFIER_GFBR_DOWNLINK;
    flow_description_content.parameterslist[2]
        .parametercontents.gfbrormfbr_uplinkordownlink.uint =
        GFBRORMFBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
    // edge->qos_profile.parameter.qos_profile_gbr.gfbr.donwlink.unit;
    flow_description_content.parameterslist[2]
        .parametercontents.gfbrormfbr_uplinkordownlink.value =
        std::stoi(edge->qos_profile.getGbrDl());
    // edge->qos_profile.parameter.qos_profile_gbr.gfbr.donwlink.value;

    flow_description_content.parameterslist[3].parameteridentifier =
        PARAMETER_IDENTIFIER_MFBR_UPLINK;
    flow_description_content.parameterslist[3]
        .parametercontents.gfbrormfbr_uplinkordownlink.uint =
        GFBRORMFBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
    // edge->qos_profile.parameter.qos_profile_gbr.mfbr.uplink.unit;
    flow_description_content.parameterslist[3]
        .parametercontents.gfbrormfbr_uplinkordownlink.value =
        std::stoi(edge->qos_profile.getMaxbrUl());
    // edge->qos_profile.parameter.qos_profile_gbr.mfbr.uplink.value;

    flow_description_content.parameterslist[4].parameteridentifier =
        PARAMETER_IDENTIFIER_MFBR_DOWNLINK;
    flow_description_content.parameterslist[4]
        .parametercontents.gfbrormfbr_uplinkordownlink.uint =
        GFBRORMFBR_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1MBPS;
    // edge->qos_profile.parameter.qos_profile_gbr.mfbr.donwlink.unit;
    flow_description_content.parameterslist[4]
        .parametercontents.gfbrormfbr_uplinkordownlink.value =
        std::stoi(edge->qos_profile.getMaxbrDl());
    // edge->qos_profile.parameter.qos_profile_gbr.mfbr.donwlink.value;
#endif
  } else {
#if 1
    flow_description_content.numberofparameters = 1;
    flow_description_content.parameterslist =
        (ParametersList*) calloc(3, sizeof(ParametersList));  // TODO: Why 3?
    flow_description_content.parameterslist[0].parameteridentifier =
        PARAMETER_IDENTIFIER_5QI;
    flow_description_content.parameterslist[0].parametercontents._5qi =
        edge->qos_profile.getR5qi();
#endif
  }

  return flow_description_content;
}

//------------------------------------------------------------------------------
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
      qos_flow.qos_profile.setR5qi(
          qos_flow_description_content.parameterslist[j]
              .parametercontents._5qi);
    } else if (
        qos_flow_description_content.parameterslist[j].parameteridentifier ==
        PARAMETER_IDENTIFIER_GFBR_UPLINK) {
      std::string gbrUlValue = std::to_string(
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.uint);
      qos_flow.qos_profile.setGbrUl(gbrUlValue);
      std::string gbrValue = std::to_string(
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.value);
      qos_flow.qos_profile.setGbrUl(gbrValue);
    } else if (
        qos_flow_description_content.parameterslist[j].parameteridentifier ==
        PARAMETER_IDENTIFIER_GFBR_DOWNLINK) {
      std::string gbrDlValue = std::to_string(
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.uint);
      qos_flow.qos_profile.setGbrDl(gbrDlValue);
      std::string gbrValue = std::to_string(
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.value);
      qos_flow.qos_profile.setGbrDl(gbrValue);
    } else if (
        qos_flow_description_content.parameterslist[j].parameteridentifier ==
        PARAMETER_IDENTIFIER_MFBR_UPLINK) {
      std::string mbrUlValue = std::to_string(
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.uint);
      qos_flow.qos_profile.setMaxbrUl(mbrUlValue);
      std::string mbrValue = std::to_string(
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.value);
      qos_flow.qos_profile.setMaxbrUl(mbrValue);
    } else if (
        qos_flow_description_content.parameterslist[j].parameteridentifier ==
        PARAMETER_IDENTIFIER_MFBR_DOWNLINK) {
      std::string mbrDlValue = std::to_string(
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.uint);
      qos_flow.qos_profile.setMaxbrDl(mbrDlValue);
      std::string mbrValue = std::to_string(
          qos_flow_description_content.parameterslist[j]
              .parametercontents.gfbrormfbr_uplinkordownlink.value);
      qos_flow.qos_profile.setMaxbrDl(mbrValue);
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
