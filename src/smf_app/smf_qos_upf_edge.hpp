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

/*! \file smf_qos_upf_edge.hpp
 \brief
 \author  Stefan Spettel
 \company phine.tech
 \date    2024
 \email:  stefan.spettel@phine.tech
 */

#pragma once

#include "3gpp_29.244.h"
#include "UPInterfaceType.h"
#include "RedirectInformation.h"
#include "smf.h"
#include "smf_pfcp_association.hpp"
#include "config.hpp"

namespace smf {

class pfcp_association;

const std::string DEFAULT_FLOW_DESCRIPTION =
    "permit out ip from any to assigned";

struct upf_selection_criteria {
  oai::model::common::Snssai snssai{};
  std::string dnn{};
  std::unordered_set<std::string> dnais;
  qos_profile_t qos_profile{};  // QoS profile

  // internal information used and set within graph
  oai::model::pcf::FlowInformation flow_information =
      get_default_flow_information();
  oai::model::pcf::RedirectInformation redirect_information{};
  unsigned int precedence{};
  uint8_t qfi{};

  [[nodiscard]] std::string to_string(int level) const;

  static oai::model::pcf::FlowInformation get_default_flow_information() {
    oai::model::pcf::FlowInformation flow;
    flow.setFlowDescription(DEFAULT_FLOW_DESCRIPTION);
    oai::model::pcf::FlowDirectionRm flow_direction;
    flow_direction.setEnumValue(oai::model::pcf::FlowDirection_anyOf::
                                    eFlowDirection_anyOf::BIDIRECTIONAL);
    flow.setFlowDirection(flow_direction);
    flow.setPacketFilterUsage(true);  // so that we send it to UE always
    return flow;
  }
};

/**
 * Stores which DNAI is associated to an edge with specific DNN and SNSSAI
 */
struct dnai_dnn_slice {
  std::string dnai;
  oai::model::common::Snssai snssai;
  std::string dnn;
};

class qos_upf_edge {
 public:
  // General infos, from PCF
  unsigned int precedence{};
  oai::model::nrf::UPInterfaceType type{};
  oai::model::pcf::FlowInformation flow_information{};
  oai::model::pcf::RedirectInformation redirect_information{};
  std::string correlation_id{};
  // UPF/PFCP specific infos
  pfcp::pdr_id_t pdr_id{};
  pfcp::far_id_t far_id{};
  pfcp::urr_id_t urr_id{};
  uint8_t qos_rule_id{};

  std::string nw_instance{};
  std::string used_dnai{};

  std::vector<dnai_dnn_slice> dnai_dnn_slices{};

  bool uplink              = false;
  bool n4_update_necessary = false;
  bool default_qos         = true;

  // QoS information
  pfcp::qfi_t qfi{};
  pfcp::fteid_t fteid{};
  // for N3 or N9
  pfcp::fteid_t next_hop_fteid{};
  // we could also use QoSData from models, but this qos_profile is really used
  // at many places
  qos_profile_t qos_profile{};  // QoS profile

  std::shared_ptr<qos_upf_edge> associated_edge{};
  std::shared_ptr<pfcp_association> destination_upf{};
  std::shared_ptr<pfcp_association> source_upf{};

  bool operator==(const qos_upf_edge& other) const {
    return nw_instance == other.nw_instance && type == other.type &&
           uplink == other.uplink && destination_upf == other.destination_upf &&
           source_upf == other.source_upf && qfi == other.qfi;
  }

  [[nodiscard]] std::string to_string(int level) const;

  [[nodiscard]] bool serves_network(const upf_selection_criteria& criteria);

  static std::vector<std::shared_ptr<qos_upf_edge>> create_n6_edges(
      const std::shared_ptr<pfcp_association>& this_upf);

  static std::vector<std::shared_ptr<qos_upf_edge>> create_n3_edges(
      const std::shared_ptr<pfcp_association>& this_upf);

  static std::vector<std::shared_ptr<qos_upf_edge>> create_n9_edges(
      const std::shared_ptr<pfcp_association>& this_upf,
      const std::shared_ptr<pfcp_association>& other_upf);

  pfcp::redirect_information_t get_pfcp_redirect_information();

  void clear_session();

 private:
  static std::vector<std::shared_ptr<qos_upf_edge>> create_edges(
      const oai::model::nrf::UPInterfaceType& type,
      const std::shared_ptr<pfcp_association>& this_upf,
      const std::shared_ptr<pfcp_association>& other_upf);

  static bool find_upf_edge_from_interface(
      const oai::model::nrf::InterfaceUpfInfoItem& iface,
      const oai::config::smf::upf& other_upf_cfg);

  void associate_dnai(const oai::config::smf::upf& upf_cfg);
};

}  // namespace smf