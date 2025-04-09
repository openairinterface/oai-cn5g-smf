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

#include <memory>

#include <smf_msg.hpp>
#include <smf_pfcp_association.hpp>
#include "sdf_conversions.hpp"
#include "QosRule.hpp"
#include "QosFlowDescription.hpp"

#pragma once

namespace smf {

class session_handler {
 public:
  explicit session_handler(const pdu_session_type_e& type) {
    m_pdu_session_type = type;
  }

  void set_session_graph(const std::shared_ptr<upf_graph>& upf_graph);

  [[nodiscard]] std::shared_ptr<upf_graph> get_session_graph() const;

  bool has_session_graph();

  /**
   * Should fill the qos_flow_context_updated object for the passed qfi
   * old code on smf_context:
   * qcu.set_qfi(qfi);
   * qcu.set_ul_fteid(flow.ul_fteid);
   * qcu.set_qos_profile(flow.qos_profile);
   *
   * @param qfi
   * @param qos_flow_context_updated
   * @return
   */
  ::smf::qos_flow_context_updated get_qos_flow_context_updated(
      const pfcp::qfi_t& qfi);

  /**
   * Get QoS flows to be updated based on QFIs to be updated, you should call
   * set_qfis_to_be_updated before
   * @return
   */
  std::vector<::smf::qos_flow_context_updated> get_qos_flows_context_updated();

  /**
   * Set the list of all QFIs that need to be updated towards N1/N2 and N4
   * @param qfis
   */
  void set_qfis_to_be_updated(const std::vector<pfcp::qfi_t>& qfis);

  /**
   *
   * @return all qfis of the session
   */
  std::vector<pfcp::qfi_t> get_all_qfis();

  /**
   * Checks if a QoS flow with this QFI exists
   * @param qfi to check
   * @return true if QFI exists, false otherwise
   */
  bool qfi_exists(const pfcp::qfi_t& qfi);

  /**
   * Creates a list of JSON representations of all QoS flows
   * @return
   */
  std::vector<nlohmann::json> create_qos_flows_json();

  /**
   * Add a QoS Rule
   * @param qos_rule
   */
  void add_qos_rule(const oai::nas::QosRule& qos_rule);

  /**
   * should  store the QoS Rule and create a news QOS_FLOW / EDGE  based on
   * QOSFlowDescriptionsContents and QosRule. returns a
   * qos_flow_context_updated
   *
   * @param qos_flow_description
   * @return qos_flow_context_updated
   */
  qos_flow_context_updated create_new_qos_rule(
      oai::nas::QosRule& qos_rules_ie,
      const oai::nas::QosFlowDescription& qos_flow_description);

  /**
   * Update QoS Rules (TODO description because what is exactly the point of
   * this function)
   * @param qos_rules_ie
   * @return
   */
  ::smf::qos_flow_context_updated update_qos_rule(
      oai::nas::QosRule qos_rules_ie);

  /**
   * Get all QoS rules that need to be updated with UE. Are based on the set
   * QFIs to be updated
   * @return
   */
  std::vector<oai::nas::QosRule> get_qos_rules();

  //
  // General
  //

  /**
   * Deallocates all resources
   */
  void deallocate_resources();

  void set_cause(const cause_value_5gsm_e& cause);

  /**
   * Generates a unique URR ID
   * @return URR ID
   */
  pfcp::urr_id_t generate_urr_id();

  /**
   * Releases URR ID so that it can be re-used
   * @param urr_id
   */
  void release_urr_id(const pfcp::urr_id_t& urr_id);

  /**
   * Generates a unique QER ID
   * @return QER ID
   */
  pfcp::qer_id_t generate_qer_id();

  /**
   * Releases QER ID so that it can be re-used
   * @param qer_id
   */
  void release_qer_id(const pfcp::qer_id_t& qer_id);

  /**
   * Generates a unique FAR ID
   * @return FAR ID
   */
  pfcp::far_id_t generate_far_id();

  /**
   * Releases FAR ID so that it can be re-used
   * @param far_id
   */
  void release_far_id(const pfcp::far_id_t& far_id);

  /**
   * Generates a unique PDR ID
   * @return PDR ID
   */
  pfcp::pdr_id_t generate_pdr_id();

  /**
   * Releases PDR ID so that it can be re-used
   * @param pdr_id
   */
  void release_pdr_id(const pfcp::pdr_id_t& pdr_id);

  static uint64_t parse_nas_value_unit_to_bps(
      const uint16_t& value, const uint8_t& unit);

  static bool is_uplink_flow_direction(
      const oai::model::pcf::FlowInformation& flow_direction);
  static bool is_downlink_flow_direction(
      const oai::model::pcf::FlowInformation& flow_direction);

 private:
  std::shared_ptr<upf_graph> m_session_graph;
  std::vector<pfcp::qfi_t> m_qfis_to_be_updated;
  cause_value_5gsm_e m_cause_value =
      cause_value_5gsm_e::CAUSE_255_REQUEST_ACCEPTED;  // for NGAP cause
  pdu_session_type_e m_pdu_session_type;
  oai::utils::uint_generator<uint32_t> m_qos_rule_id_generator;
  oai::utils::uint_generator<uint32_t> m_qfi_generator;

  oai::utils::uint_generator<uint16_t> m_pdr_id_generator;
  oai::utils::uint_generator<uint32_t> m_qer_id_generator;
  oai::utils::uint_generator<uint32_t> m_far_id_generator;
  oai::utils::uint_generator<uint32_t> m_urr_id_generator;

  // TODO all of this is out-of-sync with new QoS handling, should update all in
  // UPF graph
  std::map<uint8_t, oai::nas::QosRule> m_qos_rules;  // QRI <-> QoS Rules
  std::vector<uint8_t> m_qos_rules_to_be_synchronised;
  std::vector<uint8_t> m_qos_rules_to_be_removed;

  mutable std::shared_mutex m_session_handler_mutex;

  /**
   * Generate a QoS Rule ID
   * @return ruleid
   */
  uint8_t generate_qos_rule_id();

  /**
   * Release a QoS Rule ID
   * @param [uint8_t &]: rule_id: QoS Rule ID to be released
   * @return void
   */
  void release_qos_rule_id(const uint8_t& rule_id);

  void set_nas_filter_from_edge(
      const std::shared_ptr<qos_upf_edge>& edge, oai::nas::QosRule& qos_rule);

  void set_port_filter(
      oai::nas::PacketFilterCreateAndModifyAndReplace& nas_filter,
      const oai::utils::sdf_conversions::port_range& port_range, bool remote);

  void set_ip_filter(
      oai::nas::PacketFilterCreateAndModifyAndReplace& nas_filter,
      const oai::utils::sdf_conversions::ip_range& port_range, bool remote);

  void set_protocol_filter(
      oai::nas::PacketFilterCreateAndModifyAndReplace& nas_filter,
      uint8_t protocol_id);

  oai::nas::QosRule qos_rule_from_edge(
      const std::shared_ptr<qos_upf_edge>& edge);

  static uint8_t nas_unit_from_bitrate_unit(
      const oai::utils::sdf_conversions::bitrate_unit_e& bitrate_unit);

  oai::nas::QosFlowDescription qos_flow_description_from_edge(
      const std::shared_ptr<qos_upf_edge>& edge);

  std::shared_ptr<qos_upf_edge> get_edge_for_qfi(uint8_t qfi);

  static bool is_flow_direction(
      bool uplink, const oai::model::pcf::FlowInformation& flow_direction);

  void set_default_qos_parameters(oai::model::pcf::QosData& qos_data);

  // Values are from 3GPP TS 23.501 Release 17.6.0 Table 5.7.4-1
  std::map<uint8_t, uint8_t> qos_priority_map = {
      {1, 20},  {2, 40},  {3, 30},  {4, 50},  {65, 7},  {66, 20}, {67, 15},
      {75, 0},  {71, 56}, {72, 56}, {73, 56}, {74, 56}, {76, 56}, {5, 10},
      {6, 60},  {7, 70},  {8, 80},  {9, 90},  {10, 90}, {69, 5},  {70, 55},
      {79, 65}, {80, 68}, {82, 19}, {83, 22}, {84, 24}, {85, 21}, {86, 18},
      {87, 25}, {88, 25}, {89, 25}, {90, 25}};

  std::map<uint8_t, int> qos_packet_delay_budget = {
      {1, 100},   {2, 150}, {3, 50},   {4, 300},  {65, 75},  {66, 100},
      {67, 100},  {75, 0},  {71, 150}, {72, 300}, {73, 300}, {74, 500},
      {76, 500},  {5, 100}, {6, 300},  {7, 100},  {8, 300},  {9, 300},
      {10, 1100}, {69, 60}, {70, 200}, {79, 50},  {80, 10},  {82, 10},
      {83, 10},   {84, 30}, {85, 5},   {86, 5},   {87, 25},  {88, 25},
      {89, 25},   {90, 25}};

  std::map<uint8_t, std::string> qos_packet_error_rate = {
      {1, "1E-2"},  {2, "1E-3"},  {3, "1E-3"},  {4, "1E-6"},  {65, "1E-2"},
      {66, "1E-2"}, {67, "1E-3"}, {75, ""},     {71, "1E-6"}, {72, "1E-4"},
      {73, "1E-8"}, {74, "1E-8"}, {76, "1E-4"}, {5, "1E-6"},  {6, "1E-6"},
      {7, "1E-3"},  {8, "1E-6"},  {9, "1E-6"},  {10, "1E-6"}, {69, "1E-6"},
      {70, "1E-6"}, {79, "1E-2"}, {80, "1E-6"}, {82, "1E-4"}, {83, "1E-4"},
      {84, "1E-5"}, {85, "1E-5"}, {86, "1E-4"}, {87, "1E-3"}, {88, "1E-3"},
      {89, "1E-4"}, {90, "1E-4"}};

  std::map<uint8_t, int> qos_max_burst_volume = {
      {1, 0},      {2, 0},      {3, 0},    {4, 0},     {65, 0},   {66, 0},
      {67, 0},     {75, 0},     {71, 0},   {72, 0},    {73, 0},   {74, 0},
      {76, 0},     {5, 0},      {6, 0},    {7, 0},     {8, 0},    {9, 0},
      {10, 0},     {69, 0},     {70, 0},   {79, 0},    {80, 0},   {82, 255},
      {83, 1354},  {84, 1354},  {85, 255}, {86, 1354}, {87, 500}, {88, 1125},
      {89, 17000}, {90, 63000},
  };

  std::map<uint8_t, int> qos_averaging_window = {
      {1, 2000},  {2, 2000},  {3, 2000},  {4, 2000},  {65, 2000}, {66, 2000},
      {67, 2000}, {75, 0},    {71, 2000}, {72, 2000}, {73, 2000}, {74, 2000},
      {76, 2000}, {5, 0},     {6, 0},     {7, 0},     {8, 0},     {9, 0},
      {10, 0},    {69, 0},    {70, 0},    {79, 0},    {80, 0},    {82, 2000},
      {83, 2000}, {84, 2000}, {85, 2000}, {86, 2000}, {87, 2000}, {88, 2000},
      {89, 2000}, {90, 2000},
  };
};

}  // namespace smf
