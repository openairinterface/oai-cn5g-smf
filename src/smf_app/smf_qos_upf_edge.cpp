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

/*! \file smf_qos_upf_edge.cpp
 \brief
 \author  Stefan Spettel
 \company phine.tech
 \date    2024
 \email:  stefan.spettel@phine.tech
 */

#include "smf_qos_upf_edge.hpp"

using namespace oai::config::smf;
using namespace oai::model::smf;
using namespace oai::model::nrf;
using namespace oai::model::pcf;
using namespace smf;

//------------------------------------------------------------------------------
std::string upf_selection_criteria::to_string(int level) const {
  std::string fmt_title = oai::config::get_title_formatter(level);
  std::string fmt_value = oai::config::get_value_formatter(level + 1);

  std::string output = fmt::format(fmt_title, "UPF Selection Criteria");
  output.append(snssai.to_string(1));
  output.append(fmt::format(fmt_value, "DNN", dnn));
  for (const auto& dnai : dnais) {
    output.append(fmt::format(fmt_value, "DNAI", dnai));
  }
  return output;
}

//------------------------------------------------------------------------------
std::string qos_upf_edge::to_string(int level) const {
  std::string fmt_title = oai::config::get_title_formatter(level);
  std::string fmt_value = oai::config::get_value_formatter(level + 1);

  std::string output = fmt::format(fmt_title, "UPF Graph Edge");
  output.append(fmt::format(fmt_value, "Interface Type", type.getEnumString()));
  output.append(fmt::format(fmt_value, "NWI", nw_instance));
  output.append(fmt::format(fmt_value, "Uplink", uplink ? "Yes" : "No"));
  if (pdr_id.rule_id != 0) {
    output.append(fmt::format(fmt_value, "PDR ID", pdr_id.rule_id));
  }
  if (far_id.far_id != 0) {
    output.append(fmt::format(fmt_value, "FAR ID", far_id.far_id));
  }

  // TODO DNAI
  if (destination_upf) {
    output.append(fmt::format(
        fmt_value, "Next UPF Node: ", destination_upf->get_printable_name()));
  }
  return output;
}

//------------------------------------------------------------------------------
std::vector<std::shared_ptr<qos_upf_edge>> qos_upf_edge::create_n6_edges(
    const std::shared_ptr<pfcp_association>& this_upf) {
  UPInterfaceType n6;
  n6.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6);
  return create_edges(n6, this_upf, nullptr);
}

//------------------------------------------------------------------------------
std::vector<std::shared_ptr<qos_upf_edge>> qos_upf_edge::create_n3_edges(
    const std::shared_ptr<pfcp_association>& this_upf) {
  UPInterfaceType n3;
  n3.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3);
  return create_edges(n3, this_upf, nullptr);
}

//------------------------------------------------------------------------------
std::vector<std::shared_ptr<qos_upf_edge>> qos_upf_edge::create_n9_edges(
    const std::shared_ptr<pfcp_association>& this_upf,
    const std::shared_ptr<pfcp_association>& other_upf) {
  UPInterfaceType n9;
  n9.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N9);
  return create_edges(n9, this_upf, other_upf);
}

//------------------------------------------------------------------------------
std::vector<std::shared_ptr<qos_upf_edge>> qos_upf_edge::create_edges(
    const UPInterfaceType& type,
    const std::shared_ptr<pfcp_association>& this_upf,
    const std::shared_ptr<pfcp_association>& other_upf) {
  upf this_upf_cfg = this_upf->get_upf_config();
  std::vector<std::shared_ptr<qos_upf_edge>> edges;

  UPInterfaceType n9_type;
  n9_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N9);

  // Because interfaceUpfInfoList is optional in TS 29.510 (why even?), we
  // just guess that this UPF has a N3 or N6 interface
  if (!this_upf_cfg.get_upf_info().interfaceUpfInfoListIsSet() ||
      this_upf_cfg.get_upf_info().getInterfaceUpfInfoList().empty()) {
    // We cannot just assume that we have an N9 interface, because we would make
    // links where non are
    if (type == n9_type) {
      return edges;
    }
    Logger::smf_app().debug(
        "UPF Interface list is empty: Assume that the UPF has an %s interface.",
        type.getEnumString());
    qos_upf_edge edge;
    edge.type            = type;
    edge.source_upf      = this_upf;
    edge.destination_upf = other_upf;
    edges.emplace_back(std::make_shared<qos_upf_edge>(edge));
    return edges;
  }

  for (const auto& iface :
       this_upf_cfg.get_upf_info().getInterfaceUpfInfoList()) {
    if (type != iface.getInterfaceType()) {
      continue;
    }
    qos_upf_edge edge;
    edge.type            = type;
    edge.nw_instance     = iface.getNetworkInstance();
    edge.source_upf      = this_upf;
    edge.destination_upf = other_upf;

    // Associate DNAIs with this NW instance
    edge.associate_dnai(this_upf_cfg);

    if (type.getEnumValue() ==
        UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N9) {
      if (other_upf &&
          find_upf_edge_from_interface(iface, other_upf->get_upf_config())) {
        edge.destination_upf = other_upf;
        edges.emplace_back(std::make_shared<qos_upf_edge>(edge));
      }
    } else {
      edges.emplace_back(std::make_shared<qos_upf_edge>(edge));
    }
  }
  return edges;
}

//------------------------------------------------------------------------------
bool qos_upf_edge::find_upf_edge_from_interface(
    const InterfaceUpfInfoItem& iface, const upf& other_upf_cfg) {
  Logger::smf_app().debug(
      "Check if UPF %s has N9 connections to existing UPFs",
      other_upf_cfg.get_host());

  if (!other_upf_cfg.get_upf_info().interfaceUpfInfoListIsSet() ||
      other_upf_cfg.get_upf_info().getInterfaceUpfInfoList().empty()) {
    Logger::smf_app().debug(
        "Did not find N9 connection: UPF interface list is empty");
    return false;
  }

  if (iface.getEndpointFqdn() == other_upf_cfg.get_host()) {
    Logger::smf_app().debug(
        "Successfully found N9 connection for UPF %s",
        other_upf_cfg.get_host());
    return true;
  }
  for (const auto& current_ip : iface.getIpv4EndpointAddresses()) {
    if (current_ip == other_upf_cfg.get_host()) {
      Logger::smf_app().debug(
          "Successfully found N9 connection for UPF %s",
          other_upf_cfg.get_host());
      return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------------
void qos_upf_edge::associate_dnai(const upf& upf_cfg) {
  for (const auto& snssai : upf_cfg.get_upf_info().getSNssaiUpfInfoList()) {
    for (const auto& dnn : snssai.getDnnUpfInfoList()) {
      auto dnai_instances = dnn.getDnaiNwInstanceList();
      for (const auto& dnai : dnn.getDnaiList()) {
        const auto find = dnai_instances.find(dnai);
        if (find != dnn.getDnaiNwInstanceList().end() &&
            find->second == nw_instance) {
          dnai_dnn_slices.push_back(
              dnai_dnn_slice{dnai, snssai.getSNssai(), dnn.getDnn()});
        }
      }
    }
  }
}

//------------------------------------------------------------------------------
bool qos_upf_edge::serves_network(const upf_selection_criteria& criteria) {
  Logger::smf_app().info("Verifying if UPF edge serves network");
  Logger::smf_app().debug("UPF Edge: %s", to_string(0));
  Logger::smf_app().debug("Selection Criteria: %s", criteria.to_string(0));

  bool serves = source_upf->serves_network(criteria.snssai, criteria.dnn);
  // we set the filter independently of success
  flow_information     = criteria.flow_information;
  redirect_information = criteria.redirect_information;
  qos_profile          = criteria.qos_profile;
  precedence           = criteria.precedence;
  qfi.qfi              = criteria.qfi;

  if (criteria.dnais.empty()) {
    return serves;
  }

  for (const auto& dnai_dnn_slice : dnai_dnn_slices) {
    auto found = criteria.dnais.find(dnai_dnn_slice.dnai);
    if (found != criteria.dnais.end() && criteria.dnn == dnai_dnn_slice.dnn &&
        criteria.snssai == dnai_dnn_slice.snssai) {
      Logger::smf_app().debug("UPF edge serves this network");
      used_dnai = dnai_dnn_slice.dnai;
      return true;
    }
  }
  // Level is here only debug because it is normal that this happens during
  // graph traversal
  Logger::smf_app().debug("UPF Edge does NOT serve request network");
  return false;
}

pfcp::redirect_information_t qos_upf_edge::get_pfcp_redirect_information() {
  pfcp::redirect_information_t pfcp_redirect_information;

  if (redirect_information.isRedirectEnabled()) {
    switch (redirect_information.getRedirectAddressType().getEnumValue()) {
      case RedirectAddressType_anyOf::eRedirectAddressType_anyOf::
          INVALID_VALUE_OPENAPI_GENERATED:
        break;
      case RedirectAddressType_anyOf::eRedirectAddressType_anyOf::IPV4_ADDR:
        pfcp_redirect_information.redirect_address_type = 0;
        break;
      case RedirectAddressType_anyOf::eRedirectAddressType_anyOf::IPV6_ADDR:
        pfcp_redirect_information.redirect_address_type = 1;
        break;
      case RedirectAddressType_anyOf::eRedirectAddressType_anyOf::URL:
        pfcp_redirect_information.redirect_address_type = 2;
        break;
      case RedirectAddressType_anyOf::eRedirectAddressType_anyOf::SIP_URI:
        pfcp_redirect_information.redirect_address_type = 3;
        break;
    }

    pfcp_redirect_information.redirect_server_address =
        redirect_information.getRedirectServerAddress();
    pfcp_redirect_information.redirect_server_address_length =
        pfcp_redirect_information.redirect_server_address.size();
  }
  return pfcp_redirect_information;
}

void qos_upf_edge::clear_session() {
  pdr_id         = pfcp::pdr_id_t{};
  far_id         = pfcp::far_id_t{};
  urr_id         = pfcp::urr_id_t{};
  fteid          = pfcp::fteid_t{};
  next_hop_fteid = pfcp::fteid_t{};
  qos_rule_id    = 0;
}
