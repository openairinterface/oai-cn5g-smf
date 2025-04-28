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

#include "session_handler.hpp"

#include <math.h>

#include "FlowDirection.h"
#include "QosFlowDescriptionParameter.hpp"
#include "3gpp_commons.h"
#include "Struct.hpp"
#include "conversions.h"
#include "conversions.hpp"
#include "smf_config.hpp"

using namespace smf;
using namespace oai::model::pcf;
using namespace oai::utils;
using namespace oai::utils::sdf_conversions;
extern std::unique_ptr<oai::config::smf::smf_config> smf_cfg;

const uint8_t NAS_PACKET_FILTER_DOWNLINK_DIRECTION = 0b01;
const uint8_t NAS_PACKET_FILTER_UPLINK_DIRECTION   = 0b10;
const uint8_t NAS_PACKET_FILTER_BIDIRECTIONAL      = 0b11;

//------------------------------------------------------------------------------
void session_handler::set_session_graph(
    const std::shared_ptr<upf_graph>& upf_graph) {
  m_session_graph = upf_graph;
}

std::shared_ptr<upf_graph> session_handler::get_session_graph() const {
  return m_session_graph;
}

//------------------------------------------------------------------------------
bool session_handler::has_session_graph() {
  return m_session_graph != nullptr;
}

//------------------------------------------------------------------------------
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
    set_default_qos_parameters(flow.qos_profile);
    flow.cause_value = static_cast<uint8_t>(m_cause_value);
    flow.set_dl_fteid(edge->next_hop_fteid);
    flow.set_ul_fteid(edge->fteid);

    // add QoS rule to flow
    auto rule = qos_rule_from_edge(edge);
    if (rule.GetNumberOfPacketFilters() != 0) {
      flow.add_qos_rule(rule);
    } else {
      Logger::smf_n1().warn(
          "QoS rule %u does not have a packet filter. This rule is not sent to "
          "UE (NAS). Please set packetFilterUsage to true in the PCC rules.",
          edge->qos_rule_id);
    }

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

//------------------------------------------------------------------------------
std::vector<::smf::qos_flow_context_updated>
session_handler::get_qos_flows_context_updated() {
  std::vector<::smf::qos_flow_context_updated> flows;
  flows.reserve(m_qfis_to_be_updated.size());
  for (const auto& qfi : m_qfis_to_be_updated) {
    flows.push_back(get_qos_flow_context_updated(qfi));
  }

  return flows;
}

//------------------------------------------------------------------------------
void session_handler::set_qfis_to_be_updated(
    const std::vector<pfcp::qfi_t>& qfis) {
  m_qfis_to_be_updated = qfis;
}

//------------------------------------------------------------------------------
void session_handler::set_cause(const cause_value_5gsm_e& cause) {
  m_cause_value = cause;
}

//------------------------------------------------------------------------------
// this code is really ugly, as soon as we refactor NAS, we have to refactor
// this as well
void session_handler::set_nas_filter_from_edge(
    const shared_ptr<qos_upf_edge>& edge, oai::nas::QosRule& qos_rule) {
  auto flow = edge->flow_information;
  // qos_rule.numberofpacketfilters = 0;
  qos_rule.SetNumberOfPacketFilters(0);

  if (!flow.flowDescriptionIsSet() || flow.getFlowDescription().empty()) {
    Logger::smf_app().warn(
        "Flow description is empty for rule: %ud. Not signaled towards UE",
        qos_rule.GetQosRuleId());
    return;
  }

  if (!flow.isPacketFilterUsage()) {
    Logger::smf_app().debug(
        "Flow %s is not signaled to UE as packetFilterUsage is disabled",
        flow.getFlowDescription());
    return;
  }

  auto parsed_filter = sdf_filter::from_string(flow.getFlowDescription());

  // TODO really not nice to do calloc in this deep service layer, that should
  // all be part of protocol
  // TODO also, free on this is never called as far as I can see that!!!
  if (edge->default_qos) {
    qos_rule.SetNumberOfPacketFilters(1);
    std::vector<oai::nas::PacketFilterCreateAndModifyAndReplace>
        packet_filter_list;
    oai::nas::PacketFilterCreateAndModifyAndReplace packet_filter = {};
    packet_filter.packet_filter_direction = NAS_PACKET_FILTER_UPLINK_DIRECTION;
    packet_filter.packet_filter_id        = 1;

    oai::nas::PacketFilterComponent packet_filter_component = {};
    packet_filter_component.type = oai::nas::kQosRulePfctiMatchAllType;
    packet_filter.content.packet_filter_components.push_back(
        packet_filter_component);
    packet_filter.content.length = 1;  // for QoS Rule Matchall_type
    packet_filter_list.push_back(packet_filter);
    qos_rule.SetPacketFilterCreateAndModifyAndReplaceList(packet_filter_list);
  } else if (!parsed_filter.default_filter) {
    // TODO we should take into account the max number of supported packet
    // filters from UE
    qos_rule.SetNumberOfPacketFilters(
        1);  // Must be dynamic for multiple FlowDescriptions TODO: set to
             // number of flows
    oai::nas::PacketFilterCreateAndModifyAndReplace packet_filter = {};
    packet_filter.packet_filter_id                                = 1;

    // Set packet filter direction based on flow direction
    if (flow.flowDirectionIsSet()) {
      auto flow_direction = flow.getFlowDirection().getEnumValue();
      switch (flow_direction) {
        case FlowDirection_anyOf::eFlowDirection_anyOf::DOWNLINK:
          packet_filter.packet_filter_direction =
              NAS_PACKET_FILTER_DOWNLINK_DIRECTION;
          break;
        case FlowDirection_anyOf::eFlowDirection_anyOf::UPLINK:
          packet_filter.packet_filter_direction =
              NAS_PACKET_FILTER_UPLINK_DIRECTION;
          break;
        case FlowDirection_anyOf::eFlowDirection_anyOf::BIDIRECTIONAL:
        default:
          packet_filter.packet_filter_direction =
              NAS_PACKET_FILTER_BIDIRECTIONAL;
      }
    } else {
      Logger::smf_n1().info(
          "Flow Direction of flow %s is not set, will be set to BIDIRECTIONAL",
          flow.getFlowDescription());
      packet_filter.packet_filter_direction = NAS_PACKET_FILTER_BIDIRECTIONAL;
    }

    if (parsed_filter.use_protocol_identifier) {
      set_protocol_filter(packet_filter, parsed_filter.protocol_identifier);
    }

    if (parsed_filter.src_ip_range.use_ip_range) {
      set_ip_filter(packet_filter, parsed_filter.src_ip_range, true);
    }

    if (!parsed_filter.src_port_ranges.empty()) {
      for (const auto& port : parsed_filter.src_port_ranges) {
        set_port_filter(packet_filter, port, true);
      }
    }

    if (parsed_filter.dst_ip_range.use_ip_range) {
      set_ip_filter(packet_filter, parsed_filter.dst_ip_range, false);
    }

    if (!parsed_filter.dst_port_ranges.empty()) {
      for (const auto& port : parsed_filter.dst_port_ranges) {
        set_port_filter(packet_filter, port, false);
      }
    }

    qos_rule.AddPacketFilterCreateAndModifyAndReplace(packet_filter);
  }
}

//------------------------------------------------------------------------------
void session_handler::set_port_filter(
    oai::nas::PacketFilterCreateAndModifyAndReplace& nas_filter,
    const port_range& port_range, bool remote) {
  uint16_t port_low  = htons(port_range.start);
  uint16_t port_high = htons(port_range.end);

  oai::nas::PacketFilterComponent packet_filter_component = {};

  if (port_range.is_range) {
    uint32_t int_range = 0;
    int_range          = ((uint32_t) port_high << 16) | port_low;
    packet_filter_component.type =
        (remote) ? oai::nas::kQosRulePfctiRemotePortRangeType :
                   oai::nas::kQosRulePfctiLocalPortRangeType;
    // sequence of a two octet port range low limit field and a two octet port
    // range high limit field
    packet_filter_component.value = blk2bstr(&int_range, 4);
    nas_filter.content.packet_filter_components.push_back(
        packet_filter_component);
    nas_filter.content.length += blength(packet_filter_component.value) +
                                 1;  // 1 for packet filter component type
  } else {
    packet_filter_component.type =
        (remote) ? oai::nas::kQosRulePfctiSingleRemotePortType :
                   oai::nas::kQosRulePfctiSingleLocalPortType;
    // two octets which specify a port number
    packet_filter_component.value = blk2bstr(&port_low, 2);
    nas_filter.content.packet_filter_components.push_back(
        packet_filter_component);
    nas_filter.content.length += blength(packet_filter_component.value) +
                                 1;  // 1 for packet filter component type
  }
}

//------------------------------------------------------------------------------
void session_handler::set_ip_filter(
    oai::nas::PacketFilterCreateAndModifyAndReplace& nas_filter,
    const ip_range& ip_range, bool remote) {
  oai::nas::PacketFilterComponent packet_filter_component = {};
  packet_filter_component.type =
      (remote) ? oai::nas::kQosRulePfctiIpv4RemoteAddressType :
                 oai::nas::kQosRulePfctiIpv4LocalAddressType;
  // Sequence of a four octet IPv4 address field and a four octet IPv4 address
  // mask field
  // TODO: verify the order
  uint64_t ip_snm =
      ((uint64_t) ip_range.snm.s_addr << 32) | ip_range.ip_addr.s_addr;
  packet_filter_component.value = blk2bstr(&ip_snm, 8);

  nas_filter.content.packet_filter_components.push_back(
      packet_filter_component);
  nas_filter.content.length += blength(packet_filter_component.value) +
                               1;  // 1 for packet filter component type
}

//------------------------------------------------------------------------------
void session_handler::set_protocol_filter(
    oai::nas::PacketFilterCreateAndModifyAndReplace& nas_filter,
    uint8_t protocol_id) {
  oai::nas::PacketFilterComponent packet_filter_component = {};
  packet_filter_component.type =
      oai::nas::kQosRulePfctiProtocolIdentifierOrNextHeaderType;
  packet_filter_component.value = blk2bstr(&protocol_id, 1);

  nas_filter.content.packet_filter_components.push_back(
      packet_filter_component);
  nas_filter.content.length += blength(packet_filter_component.value) +
                               1;  // 1 for packet filter component type
}

// Comments about architecture
// IMO this part should not be here at all, we should have some "common" DTO
// that is used between all layers and the low-level malloc/free stuff here
// should be handled on protocol-level (N1/N2) but the refactor would go too
// far, so we keep this QOSRuleIE here also, the mapping between SDF filter and
// the packet filter here should not be here
//------------------------------------------------------------------------------
oai::nas::QosRule session_handler::qos_rule_from_edge(
    const shared_ptr<qos_upf_edge>& edge) {
  oai::nas::QosRule qos_rule;

  if (edge->qos_rule_id == 0) {
    edge->qos_rule_id = generate_qos_rule_id();
  }

  // TODO check that number of QoS rules does not exceed what UE can do
  // see section 5.7.1.4 @ 3GPP TS 23.501
  /*  qos_rule.qosruleidentifer  = edge->qos_rule_id;
    qos_rule.ruleoperationcode = CREATE_NEW_QOS_RULE;
    if (edge->default_qos) {
      qos_rule.dqrbit = THE_QOS_RULE_IS_DEFAULT_QOS_RULE;
    } else {
      qos_rule.dqrbit = THE_QOS_RULE_IS_NOT_THE_DEFAULT_QOS_RULE;
    }
  */

  qos_rule.SetQosRuleId(edge->qos_rule_id);
  qos_rule.SetRuleOperationCode(
      oai::nas::kQosRuleRuleOperationCodeCreateNewQosRule);
  if (edge->default_qos) {
    qos_rule.SetDqrBit(oai::nas::kQosRuleTheQosRuleIsTheDefaultQosRule);
  } else {
    qos_rule.SetDqrBit(oai::nas::kQosRuleTheQosRuleIsNotTheDefaultQosRule);
  }

  if (m_pdu_session_type != PDU_SESSION_TYPE_E_UNSTRUCTURED) {
    set_nas_filter_from_edge(edge, qos_rule);
  } else {
    // qos_rule.numberofpacketfilters = 0;
    qos_rule.SetNumberOfPacketFilters(0);
  }

  Logger::smf_n1().debug(
      "Created new QoS rule with ID %u and %u packet filters",
      edge->qos_rule_id, qos_rule.GetNumberOfPacketFilters());

  qos_rule.SetPrecedence(edge->precedence);
  qos_rule.SetSegregation(oai::nas::kQosRuleSegregationNotRequested);
  qos_rule.SetQfi(edge->qfi.qfi);
  // qos_rule.qosruleprecedence = edge->precedence;
  // qos_rule.segregation       = SEGREGATION_NOT_REQUESTED;
  // qos_rule.qosflowidentifer  = edge->qfi.qfi;

  return qos_rule;
}

//------------------------------------------------------------------------------
oai::nas::QosFlowDescription session_handler::qos_flow_description_from_edge(
    const shared_ptr<qos_upf_edge>& edge) {
  oai::nas::QosFlowDescription qos_flow_description = {};
  qos_flow_description.SetQfi(edge->qfi.qfi);
  qos_flow_description.SetOperationCode(
      oai::nas::
          kQosFlowDescriptionRuleOperationCodeCreateNewQosFlowDescription);
  qos_flow_description.SetEBit(
      oai::nas::kQosFlowDescriptionEBitParametersListIsIncluded);

  std::vector<oai::nas::QosFlowDescriptionParameter>
      qos_flow_description_parameter_list;

  if (edge->qos_profile.gbrUlIsSet()) {
    oai::nas::QosFlowDescriptionParameter qos_flow_description_parameter = {};
    BitRate bit_rate                                                     = {};
    bit_rate.unit  = kBitRateUnitValueIsIncrementedInMultiplesOf1Mbps;
    bit_rate.value = 1000;
    if (!parse_bitrate_string(edge->qos_profile.getGbrUl(), bit_rate)) {
      Logger::smf_app().warn("Cannot parse bitrate string, use default value");
    }
    qos_flow_description_parameter.SetGfbrUplink(bit_rate);
    qos_flow_description_parameter_list.push_back(
        qos_flow_description_parameter);
  }

  if (edge->qos_profile.gbrDlIsSet()) {
    oai::nas::QosFlowDescriptionParameter qos_flow_description_parameter = {};
    BitRate bit_rate                                                     = {};
    bit_rate.unit  = kBitRateUnitValueIsIncrementedInMultiplesOf1Mbps;
    bit_rate.value = 1000;
    if (!parse_bitrate_string(edge->qos_profile.getGbrDl(), bit_rate)) {
      Logger::smf_app().warn("Cannot parse bitrate string, use default value");
    }
    qos_flow_description_parameter.SetGfbrDownlink(bit_rate);
    qos_flow_description_parameter_list.push_back(
        qos_flow_description_parameter);
  }

  if (edge->qos_profile.maxbrUlIsSet()) {
    oai::nas::QosFlowDescriptionParameter qos_flow_description_parameter = {};
    BitRate bit_rate                                                     = {};
    bit_rate.unit  = kBitRateUnitValueIsIncrementedInMultiplesOf1Mbps;
    bit_rate.value = 1000;
    if (!parse_bitrate_string(edge->qos_profile.getMaxbrUl(), bit_rate)) {
      Logger::smf_app().warn("Cannot parse bitrate string, use default value");
    }
    qos_flow_description_parameter.SetMfbrUplink(bit_rate);
    qos_flow_description_parameter_list.push_back(
        qos_flow_description_parameter);
  }

  if (edge->qos_profile.maxbrDlIsSet()) {
    oai::nas::QosFlowDescriptionParameter qos_flow_description_parameter = {};
    BitRate bit_rate                                                     = {};
    bit_rate.unit  = kBitRateUnitValueIsIncrementedInMultiplesOf1Mbps;
    bit_rate.value = 1000;
    if (!parse_bitrate_string(edge->qos_profile.getMaxbrDl(), bit_rate)) {
      Logger::smf_app().warn("Cannot parse bitrate string, use default value");
    }
    qos_flow_description_parameter.SetMfbrDownlink(bit_rate);
    qos_flow_description_parameter_list.push_back(
        qos_flow_description_parameter);
  }

  oai::nas::QosFlowDescriptionParameter qos_flow_description_parameter = {};
  qos_flow_description_parameter.Set5qi(edge->qos_profile.getR5qi());

  qos_flow_description_parameter_list.push_back(qos_flow_description_parameter);
  qos_flow_description.SetParametersList(qos_flow_description_parameter_list);
  return qos_flow_description;
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
pfcp::qer_id_t session_handler::generate_qer_id() {
  pfcp::qer_id_t qer_id;
  qer_id.qer_id = m_qer_id_generator.get_uid();
  return qer_id;
}

//------------------------------------------------------------------------------
void session_handler::release_qer_id(const pfcp::qer_id_t& qer_id) {
  m_qer_id_generator.free_uid(qer_id.qer_id);
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

//------------------------------------------------------------------------------
pfcp::pdr_id_t session_handler::generate_pdr_id() {
  pfcp::pdr_id_t pdr_id;
  pdr_id.rule_id = m_pdr_id_generator.get_uid();
  return pdr_id;
}

//------------------------------------------------------------------------------
void session_handler::release_pdr_id(const pfcp::pdr_id_t& pdr_id) {
  m_pdr_id_generator.free_uid(pdr_id.rule_id);
}

//------------------------------------------------------------------------------
uint8_t session_handler::generate_qos_rule_id() {
  return m_qos_rule_id_generator.get_uid();
}

//------------------------------------------------------------------------------
void session_handler::release_qos_rule_id(const uint8_t& rule_id) {
  m_qos_rule_id_generator.free_uid(rule_id);
}

//------------------------------------------------------------------------------
// TODO unify with QoS handling here, we should also update the edges and QoS
// flows
void session_handler::add_qos_rule(const oai::nas::QosRule& qos_rule) {
  std::unique_lock lock(
      m_session_handler_mutex,
      std::defer_lock);  // Do not lock it first
  Logger::smf_app().info(
      "Add QoS Rule with Rule Id %d", (uint8_t) qos_rule.GetQosRuleId());
  uint8_t rule_id = qos_rule.GetQosRuleId();

  if ((rule_id >= QOS_RULE_IDENTIFIER_FIRST) and
      (rule_id <= QOS_RULE_IDENTIFIER_LAST)) {
    if (m_qos_rules.count(rule_id) > 0) {
      Logger::smf_app().error(
          "Failed to add rule (Id %d), rule existed", rule_id);
    } else {
      lock.lock();  // Lock it here
      m_qos_rules.insert(
          std::pair<uint8_t, oai::nas::QosRule>(rule_id, qos_rule));
      Logger::smf_app().trace(
          "Rule (Id %d) has been added successfully", rule_id);
    }

  } else {
    Logger::smf_app().error(
        "Failed to add rule (Id %d) failed: invalid rule Id", rule_id);
  }
}

//------------------------------------------------------------------------------
qos_flow_context_updated session_handler::create_new_qos_rule(
    oai::nas::QosRule& qos_rules_ie,
    const oai::nas::QosFlowDescription& qos_flow_description) {
  qos_flow_context_updated qos_flow;

  // Add a new QoS Flow
  if (qos_rules_ie.GetQosRuleId() == NO_QOS_RULE_IDENTIFIER_ASSIGNED) {
    // Generate a new QoS rule
    uint8_t rule_id = generate_qos_rule_id();
    Logger::smf_app().info("Create a new QoS rule (rule Id %d)", rule_id);
    qos_rules_ie.SetQosRuleId(rule_id);
  }
  add_qos_rule(qos_rules_ie);
  // TODO unify with generated_qfi, hardcode for now (like it used to be)

  qos_flow.qfi = (uint8_t) DEFAULT_QFI;

  // set qos_profile from qos_flow_description_content
  qos_flow.qos_profile = {};

  std::optional<std::vector<oai::nas::QosFlowDescriptionParameter>>
      parameter_list = qos_flow_description.GetParametersList();

  if (parameter_list.has_value()) {
    for (int i = 0; i < parameter_list.value().size(); i++) {
      uint8_t parameter_id = (parameter_list.value())[i].GetIdentifier();
      switch (parameter_id) {
        case oai::nas::kQosFlowDescriptionParameterIdentifier5qi: {
          std::optional<uint8_t> _5qi = (parameter_list.value())[i].Get5qi();
          if (_5qi.has_value()) qos_flow.qos_profile.setR5qi(_5qi.value());
        } break;

        case oai::nas::kQosFlowDescriptionParameterIdentifierGfbrUplink: {
          std::optional<BitRate> gfbr_uplink =
              (parameter_list.value())[i].GetGfbrUplink();
          if (gfbr_uplink.has_value()) {
            std::string gfbr_uplink_str =
                std::to_string(gfbr_uplink.value().value)
                    .append(std::to_string(gfbr_uplink.value().unit));
            qos_flow.qos_profile.setGbrUl(gfbr_uplink_str);
          }
        } break;

        case oai::nas::kQosFlowDescriptionParameterIdentifierGfbrDownlink: {
          std::optional<BitRate> gfbr_downlink =
              (parameter_list.value())[i].GetGfbrDownlink();
          if (gfbr_downlink.has_value()) {
            std::string gfbr_downlink_str =
                std::to_string(gfbr_downlink.value().value)
                    .append(std::to_string(gfbr_downlink.value().unit));
            qos_flow.qos_profile.setGbrDl(gfbr_downlink_str);
          }
        } break;

        case oai::nas::kQosFlowDescriptionParameterIdentifierMfbrUplink: {
          std::optional<BitRate> mfbr_uplink =
              (parameter_list.value())[i].GetMfbrUplink();
          if (mfbr_uplink.has_value()) {
            std::string mfbr_uplink_str =
                std::to_string(mfbr_uplink.value().value)
                    .append(std::to_string(mfbr_uplink.value().unit));
            qos_flow.qos_profile.setMaxbrUl(mfbr_uplink_str);
          }
        } break;

        case oai::nas::kQosFlowDescriptionParameterIdentifierMfbrDownlink: {
          std::optional<BitRate> mfbr_downlink =
              (parameter_list.value())[i].GetMfbrDownlink();
          if (mfbr_downlink.has_value()) {
            std::string mfbr_downlink_str =
                std::to_string(mfbr_downlink.value().value)
                    .append(std::to_string(mfbr_downlink.value().unit));
            qos_flow.qos_profile.setMaxbrDl(mfbr_downlink_str);
          }
        } break;

        default: {
        }
      }
    }
  }
  Logger::smf_app().debug(
      "Add new QoS Flow with new QRI %d", qos_rules_ie.GetQosRuleId());

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

//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
std::shared_ptr<qos_upf_edge> session_handler::get_edge_for_qfi(uint8_t qfi) {
  auto n3_edges = m_session_graph->get_access_edges();

  std::vector<std::shared_ptr<qos_upf_edge>> qfi_edges;
  for (const auto& edge : n3_edges) {
    if (qfi == edge->qfi.qfi) {
      qfi_edges.push_back(edge);
    }
  }

  if (qfi_edges.size() == 1) {
    return qfi_edges[0];
  }

  // if we have more than one edge for the same QFI, we use the default QoS
  // (e.g. in a UL CL scenario)
  for (const auto& edge : qfi_edges) {
    if (edge->default_qos) {
      return edge;
    }
  }

  return nullptr;
}

//------------------------------------------------------------------------------
void session_handler::deallocate_resources() {
  for (const auto& edge : m_session_graph->get_access_edges()) {
    release_pdr_id(edge->pdr_id);
    release_qer_id(edge->qer_id);
    release_far_id(edge->far_id);
    release_urr_id(edge->urr_id);
    release_qos_rule_id(edge->qos_rule_id);
    edge->clear_session();
  }
}

//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
bool session_handler::qfi_exists(const pfcp::qfi_t& qfi) {
  auto all_qfis = get_all_qfis();
  auto it       = std::find(all_qfis.begin(), all_qfis.end(), qfi);
  return it != all_qfis.end();
}

//------------------------------------------------------------------------------
qos_flow_context_updated session_handler::update_qos_rule(
    oai::nas::QosRule qos_rules_ie) {
  qos_flow_context_updated flow{};

  std::unique_lock lock(
      m_session_handler_mutex,
      std::defer_lock);  // Do not lock it first

  auto edge = get_edge_for_qfi(
      qos_rules_ie.GetQosRuleId());  // TODO: verify Rule Id or QFI Id?

  // TODO there is absolutely no link between this and the QoS rules
  flow = get_qos_flow_context_updated(edge->qfi);

  if (edge) {
    Logger::smf_app().debug(
        "Update existing QRI %d", qos_rules_ie.GetQosRuleId());

    uint8_t rule_id = qos_rules_ie.GetQosRuleId();
    if ((rule_id >= QOS_RULE_IDENTIFIER_FIRST) and
        (rule_id <= QOS_RULE_IDENTIFIER_LAST)) {
      if (m_qos_rules.count(rule_id) > 0) {
        lock.lock();  // Lock it here
        m_qos_rules.erase(rule_id);
        m_qos_rules.insert(
            std::pair<uint8_t, oai::nas::QosRule>(rule_id, qos_rules_ie));
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
        qos_rules_ie.GetQosRuleId(), qos_rules_ie.GetQfi().value_or(0));
  }

  return flow;
}

//------------------------------------------------------------------------------
std::vector<oai::nas::QosRule> session_handler::get_qos_rules() {
  std::vector<oai::nas::QosRule> qos_rules;
  for (const auto& flow : get_qos_flows_context_updated()) {
    for (const auto& [rule_id, rule] : flow.qos_rules) {
      qos_rules.push_back(rule);
    }
  }
  return qos_rules;
}

//------------------------------------------------------------------------------
uint64_t session_handler::parse_nas_value_unit_to_bps(
    const uint16_t& value, const uint8_t& unit) {
  uint64_t bit_rate_value;

  uint8_t unit_value = unit;
  if (unit > kBitRateUnitValueIsIncrementedInMultiplesOf256Pbps)
    unit_value = kBitRateUnitValueIsIncrementedInMultiplesOf256Pbps;

  if (unit_value > 0) {
    bit_rate_value = value * pow(2, 10) * pow(2, (unit_value - 1) * 2);
  } else {
    bit_rate_value = value;
  }

  return bit_rate_value;
}

//------------------------------------------------------------------------------
bool session_handler::is_uplink_flow_direction(
    const FlowInformation& flow_info) {
  return is_flow_direction(true, flow_info);
}

//------------------------------------------------------------------------------
bool session_handler::is_downlink_flow_direction(
    const FlowInformation& flow_info) {
  return is_flow_direction(false, flow_info);
}

//------------------------------------------------------------------------------
bool session_handler::is_flow_direction(
    bool uplink, const FlowInformation& flow_info) {
  if (!flow_info.flowDescriptionIsSet() ||
      flow_info.getFlowDescription().empty()) {
    return false;
  }
  FlowDirection_anyOf::eFlowDirection_anyOf flow_direction;
  if (!flow_info.flowDirectionIsSet()) {
    Logger::smf_app().info(
        "Flow Direction of flow %s is not set, assume it is BIDIRECTIONAL",
        flow_info.getFlowDescription());
    flow_direction = FlowDirection_anyOf::eFlowDirection_anyOf::BIDIRECTIONAL;
  } else {
    flow_direction = flow_info.getFlowDirection().getEnumValue();
  }

  switch (flow_direction) {
    case FlowDirection_anyOf::eFlowDirection_anyOf::DOWNLINK:
      return !uplink;
    case FlowDirection_anyOf::eFlowDirection_anyOf::UPLINK:
      return uplink;
      // all from here are automatically true, either bidirectional or handled
      // like bidirectional
      // * Design Decision: to have null values as true so that
      // the filter is set, otherwise this could lead to issues
    case FlowDirection_anyOf::eFlowDirection_anyOf::BIDIRECTIONAL:
      return true;
    case FlowDirection_anyOf::eFlowDirection_anyOf::UNSPECIFIED:
      /*
       * 3GPP TS 29.512
       * The corresponding filter applies for traffic to the UE (downlink), but
       * has no specific direction declared. The service data flow detection
       * shall apply the filter for uplink traffic as if the filter was
       * bidirectional.
       */
    case FlowDirection_anyOf::eFlowDirection_anyOf::
        INVALID_VALUE_OPENAPI_GENERATED:
    case FlowDirection_anyOf::eFlowDirection_anyOf::NULL_VALUE:
      Logger::smf_app().info(
          "Flow Direction of flow %s is UNSPECIFIED or NULL, assume it is "
          "BIDIRECTIONAL",
          flow_info.getFlowDescription());
      return true;
  }

  return false;
}

//------------------------------------------------------------------------------
void session_handler::set_default_qos_parameters(QosData& qos_data) {
  try {
    uint8_t _5qi             = qos_data.getR5qi();
    auto priority_level      = qos_priority_map.at(_5qi);
    auto packet_delay_budget = qos_packet_delay_budget.at(_5qi);
    auto packet_error_rate   = qos_packet_error_rate.at(_5qi);
    auto max_burst_volume    = qos_max_burst_volume.at(_5qi);
    auto averaging_window    = qos_averaging_window.at(_5qi);

    // Here, we check for all the values if
    // Characteristic not set, and we should send default values -> SET
    // Characteristic set and the value is the same sa the default -> UNSET

    bool send_values =
        smf_cfg->smf()->get_ngap().send_default_qos_characteristics();

    if (!qos_data.priorityLevelIsSet() && priority_level != 0 && send_values) {
      qos_data.setPriorityLevel(priority_level);
    } else if (qos_data.getPriorityLevel() == priority_level && !send_values) {
      qos_data.unsetPriorityLevel();
    }
    if (!qos_data.packetDelayBudgetIsSet() && packet_delay_budget != 0 &&
        send_values) {
      qos_data.setPacketDelayBudget(packet_delay_budget);
    } else if (
        qos_data.getPacketDelayBudget() == packet_delay_budget &&
        !send_values) {
      qos_data.unsetPacketDelayBudget();
    }
    if (!qos_data.packetErrorRateIsSet() && !packet_error_rate.empty() &&
        send_values) {
      qos_data.setPacketErrorRate(packet_error_rate);
    } else if (
        qos_data.getPacketErrorRate() == packet_error_rate && !send_values) {
      qos_data.unsetPacketErrorRate();
    }
    if (!qos_data.maxDataBurstVolIsSet() && max_burst_volume != 0 &&
        send_values) {
      qos_data.setMaxDataBurstVol(max_burst_volume);
    } else if (
        qos_data.getMaxDataBurstVol() == max_burst_volume && !send_values) {
      qos_data.unsetMaxDataBurstVol();
    }
    if (!qos_data.averWindowIsSet() && averaging_window != 0 && send_values) {
      qos_data.setAverWindow(averaging_window);
    } else if (qos_data.getAverWindow() == averaging_window && !send_values) {
      qos_data.unsetAverWindow();
    }

  } catch (std::out_of_range&) {
    Logger::smf_app().error(
        "5QI %d is not in the QoS characteristics map, we don't set the "
        "default QoS characteristics",
        qos_data.getR5qi());
  }
}
