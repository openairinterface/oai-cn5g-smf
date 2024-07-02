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

/*! \file session_handler.hpp
 \brief
 \author  Lukas ROTHENEDER, Stefan SPETTEL
 \company phine.tech
 \date 2024
 \email:  lukas.rotheneder@phine.tech, stefan.spettel@phine.tech
 */

#include <memory>

#include <smf_msg.hpp>
#include <smf_pfcp_association.hpp>
#include <QOSFlowDescriptions.h>

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
  void add_qos_rule(const QOSRulesIE& qos_rule);

  /**
   * should  store the QoS Rule and create a news QOS_FLOW / EDGE  based on
   * QOSFlowDescriptionsContents and QOSRulesIE. returns a
   * qos_flow_context_updated
   *
   * @param qos_flow_description_content
   * @return qos_flow_context_updated
   */

  ::smf::qos_flow_context_updated create_new_qos_rule(
      QOSRulesIE& qos_rule,
      const QOSFlowDescriptionsContents& qos_flow_description_content);

  /**
   * Update QoS Rules (TODO description because what is exactly the point of
   * this function)
   * @param qos_rules_ie
   * @return
   */
  ::smf::qos_flow_context_updated update_qos_rule(QOSRulesIE qos_rules_ie);

  /**
   * Get all QoS rules that need to be updated with UE. Are based on the set
   * QFIs to be updated
   * @return
   */
  std::vector<QOSRulesIE> get_qos_rules();

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

 private:
  std::shared_ptr<upf_graph> m_session_graph;
  std::vector<pfcp::qfi_t> m_qfis_to_be_updated;
  cause_value_5gsm_e m_cause_value =
      cause_value_5gsm_e::CAUSE_255_REQUEST_ACCEPTED;  // for NGAP cause
  pdu_session_type_e m_pdu_session_type;
  oai::utils::uint_generator<uint32_t> m_qos_rule_id_generator;
  oai::utils::uint_generator<uint32_t> m_qfi_generator;

  oai::utils::uint_generator<uint16_t> m_pdr_id_generator;
  oai::utils::uint_generator<uint32_t> m_far_id_generator;
  oai::utils::uint_generator<uint32_t> m_urr_id_generator;

  // TODO all of this is out-of-sync with new QoS handling, should update all in
  // UPF graph
  std::map<uint8_t, QOSRulesIE> m_qos_rules;  // QRI <-> QoS Rules
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
      const std::shared_ptr<qos_upf_edge>& edge, QOSRulesIE& qos_rule);

  QOSRulesIE qos_rule_from_edge(const std::shared_ptr<qos_upf_edge>& edge);

  std::shared_ptr<qos_upf_edge> get_edge_for_qfi(uint8_t qfi);
};

}  // namespace smf
