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

/*! \file smf_pfcp_association.cpp
 \brief
 \author  Lionel GAUTHIER
 \date 2019
 \email: lionel.gauthier@eurecom.fr
 */

#include "smf_pfcp_association.hpp"

#include "common_defs.h"
#include "logger.hpp"
#include "smf_n4.hpp"
#include "smf_procedure.hpp"
#include "smf_config.hpp"
#include "fqdn.hpp"

using namespace smf;
using namespace std;
using namespace oai::model::pcf;

extern itti_mw* itti_inst;
extern smf_n4* smf_n4_inst;
extern std::unique_ptr<oai::config::smf::smf_config> smf_cfg;

//---------------------------------------------------------------------------------------------
edge edge::from_upf_info(const oai::model::nrf::UpfInfo& upf_info) {
  edge e                             = {};
  snssai_upf_info_item_s snssai_item = {};

  Logger::smf_app().debug(
      "Adding edge for UPF with UPF Info: \n %s", upf_info.to_string(0));
  // TODO temporary refactor, this is not really necessary anymore

  for (auto& snssai : upf_info.getSNssaiUpfInfoList()) {
    snssai_item.snssai =
        snssai_t(snssai.getSNssai().getSst(), snssai.getSNssai().getSd());
    for (const auto& dnn_item : snssai.getDnnUpfInfoList()) {
      dnn_upf_info_item_t dnn_info_item;
      dnn_info_item.dnn = dnn_item.getDnn();
      for (const auto& dnai : dnn_item.getDnaiList()) {
        dnn_info_item.dnai_list.insert(dnai);
      }
      dnn_info_item.dnai_nw_instance_list = dnn_item.getDnaiNwInstanceList();
      snssai_item.dnn_upf_info_list.insert(dnn_info_item);
    }

    e.snssai_dnns.insert(snssai_item);
  }

  if (!e.snssai_dnns.empty()) {
    Logger::smf_app().debug("UPF link info:");
    for (const auto s : e.snssai_dnns) {
      Logger::smf_app().debug("%s", s.to_string().c_str());
    }
  } else {
    Logger::smf_app().debug("UPF link info empty");
  }
  return e;
}

//---------------------------------------------------------------------------------------------
edge edge::from_upf_info(
    const oai::model::nrf::UpfInfo& upf_info,
    const oai::model::nrf::InterfaceUpfInfoItem& interface) {
  // TODO: Bring updates from the previous funtion to this one
  edge e = {};
  // TODO temporary refactor, this is not really necessary anymore
  e.type = pfcp_association::iface_type_from_string(
      interface.getInterfaceType().getEnumString());
  if (interface.ipv4EndpointAddressesIsSet()) {
    e.ip_addr = conv::fromString(interface.getIpv4EndpointAddresses()[0]);
  }
  // e.ip6_addr    = interface.ipv6_addresses[0];
  e.nw_instance = interface.getNetworkInstance();

  // we filter out the DNAIs which do not map to the given NW interface
  for (const auto& snssai_item : upf_info.getSNssaiUpfInfoList()) {
    snssai_upf_info_item_s new_snssai_item;
    new_snssai_item.snssai = snssai_t(
        snssai_item.getSNssai().getSst(), snssai_item.getSNssai().getSd());

    for (const auto& dnn_item : snssai_item.getDnnUpfInfoList()) {
      dnn_upf_info_item_s new_dnn_item;
      new_dnn_item.dnn           = dnn_item.getDnn();
      auto dnai_nw_instance_list = dnn_item.getDnaiNwInstanceList();
      if (!dnn_item.getDnaiList().empty() && !dnai_nw_instance_list.empty()) {
        for (const auto& dnai : dnn_item.getDnaiList()) {
          auto dnai_it = dnai_nw_instance_list.find(dnai);
          if (dnai_it != dnai_nw_instance_list.end()) {
            if (dnai_it->second == e.nw_instance) {
              new_dnn_item.dnai_list.insert(dnai);
              break;
            }
          }
        }
      } else {
        Logger::smf_app().debug(
            "DNAI List or DNAI NW Instance List is empty for this UPF.");
      }
      new_snssai_item.dnn_upf_info_list.insert(new_dnn_item);
    }
    e.snssai_dnns.insert(new_snssai_item);
  }

  return e;
}

//---------------------------------------------------------------------------------------------
bool edge::serves_network(
    const std::string& dnn, const snssai_t& snssai,
    const std::unordered_set<std::string>& dnais,
    std::string& matched_dnai) const {
  // Logger::smf_app().debug(
  //    "Serves Network, DNN %s, S-NSSAI %s", dnn.c_str(),
  //    snssai.toString().c_str());
  // just create a snssai_upf_info_item for fast lookup
  snssai_upf_info_item_s snssai_item = {};
  snssai_item.snssai                 = snssai;
  // For debugging purpose
  if (!snssai_dnns.empty()) {
    Logger::smf_app().debug("S-NSSAI UPF info list: ");
    for (const auto& s : snssai_dnns) {
      Logger::smf_app().debug("%s", s.to_string().c_str());
    }
  }

  for (const auto& snssai_it : snssai_dnns) {
    if (snssai_it.snssai == snssai_item.snssai) {
      // create temp item for fast lookup
      dnn_upf_info_item_s dnn_item = {};
      dnn_item.dnn                 = dnn;
      auto dnn_it                  = snssai_it.dnn_upf_info_list.find(dnn_item);
      if (dnn_it != snssai_it.dnn_upf_info_list.end()) {
        if (!dnais.empty()) {
          // should be only 1 DNAI
          for (const auto& dnai : dnn_it->dnai_list) {
            // O(1)
            auto found_dnai = dnais.find(dnai);
            if (found_dnai != dnais.end()) {
              matched_dnai = dnai;
              return true;
            }
          }
        } else {
          return true;
        }
      }
    }
  }

  Logger::smf_app().debug("Could not serve network, return false");
  return false;
}

//---------------------------------------------------------------------------------------------
bool edge::serves_network(
    const std::string& dnn, const snssai_t& snssai) const {
  std::unordered_set<string> set;
  std::string s = {};
  return serves_network(dnn, snssai, set, s);
}

//---------------------------------------------------------------------------------------------
bool edge::get_qos_flows(std::vector<std::shared_ptr<smf_qos_flow>>& flows) {
  flows.clear();
  for (const auto& flow : this->qos_flows) {
    flows.push_back(flow);
  }
  return flows.size() > 0;
}

//---------------------------------------------------------------------------------------------
bool edge::get_qos_flows(
    pdu_session_id_t pid, std::vector<std::shared_ptr<smf_qos_flow>>& flows) {
  flows.clear();
  for (const auto& flow : this->qos_flows) {
    if (flow->pdu_session_id == pid) flows.push_back(flow);
  }
  return flows.size() > 0;
}

//---------------------------------------------------------------------------------------------
std::shared_ptr<smf_qos_flow> edge::get_qos_flow(const pfcp::pdr_id_t& pdr_id) {
  // it may happen that 2 qos flows have the same PDR ID e.g. in an
  // UL CL scenario, but then they will also have the same FTEID
  for (auto& flow_it : qos_flows) {
    if (flow_it->pdr_id_ul == pdr_id || flow_it->pdr_id_dl == pdr_id) {
      return flow_it;
    }
  }
  return {};
}

//---------------------------------------------------------------------------------------------
std::shared_ptr<smf_qos_flow> edge::get_qos_flow(const pfcp::qfi_t& qfi) {
  for (auto& flow_it : qos_flows) {
    if (flow_it->qfi == qfi) {
      return flow_it;
    }
  }
  return {};
}

//---------------------------------------------------------------------------------------------
std::shared_ptr<smf_qos_flow> edge::get_qos_flow(const pfcp::far_id_t& far_id) {
  for (auto& flow_it : qos_flows) {
    if (flow_it->far_id_ul.second == far_id ||
        flow_it->far_id_dl.second == far_id) {
      return flow_it;
    }
  }
  return {};
}

//------------------------------------------------------------------------------
void smf_qos_flow::mark_as_released() {
  released = true;
}

std::string smf_qos_flow::toString(const std::string& indent) const {
  std::string s = {};
  s.append(indent).append("QoS Flow:\n");
  s.append(indent)
      .append("\tQFI:\t\t")
      .append(std::to_string((uint8_t) qfi.qfi))
      .append("\n");
  s.append(indent)
      .append("\tUL FTEID:\t")
      .append(ul_fteid.toString())
      .append("\n");
  s.append(indent)
      .append("\tDL FTEID:\t")
      .append(dl_fteid.toString())
      .append("\n");
  s.append(indent)
      .append("\tPDR ID UL:\t")
      .append(std::to_string(pdr_id_ul.rule_id))
      .append("\n");
  s.append(indent)
      .append("\tPDR ID DL:\t")
      .append(std::to_string(pdr_id_dl.rule_id))
      .append("\n");

  s.append(indent)
      .append("\tPrecedence:\t")
      .append(std::to_string(precedence.precedence))
      .append("\n");
  if (far_id_ul.first) {
    s.append(indent)
        .append("\tFAR ID UL:\t")
        .append(std::to_string(far_id_ul.second.far_id))
        .append("\n");
  }
  if (far_id_dl.first) {
    s.append(indent)
        .append("\tFAR ID DL:\t")
        .append(std::to_string(far_id_dl.second.far_id))
        .append("\n");
  }
  if (urr_id.urr_id != 0) {
    s.append(indent)
        .append("\tURR ID:\t")
        .append(std::to_string(urr_id.urr_id))
        .append("\n");
  }
  return s;
}

//------------------------------------------------------------------------------
std::string smf_qos_flow::toString() const {
  return toString("\t\t");
}
//------------------------------------------------------------------------------
void smf_qos_flow::deallocate_ressources() {
  clear();
  Logger::smf_app().info(
      "Resources associated with this QoS Flow (%d) have been released",
      (uint8_t) qfi.qfi);
}

//------------------------------------------------------------------------------
iface_type pfcp_association::iface_type_from_string(const std::string& input) {
  iface_type type_tmp = {};
  if (input == "N3") {
    type_tmp = iface_type::N3;
  } else if (input == "N6") {
    type_tmp = iface_type::N6;
  } else if (input == "N9") {
    type_tmp = iface_type::N9;
  } else {
    Logger::smf_app().error("Interface Type %s undefined!", input.c_str());
  }

  return type_tmp;
}
//------------------------------------------------------------------------------
std::string pfcp_association::string_from_iface_type(const iface_type& type) {
  std::string output = {};
  switch (type) {
    case iface_type::N3:
      return "N3";
    case iface_type::N6:
      return "N6";
    case iface_type::N9:
      return "N9";
    default:
      return "?";
  }
}

//------------------------------------------------------------------------------
void pfcp_association::notify_add_session(const pfcp::fseid_t& cp_fseid) {
  std::unique_lock<std::mutex> l(m_sessions);
  sessions.insert(cp_fseid);
}

//------------------------------------------------------------------------------
bool pfcp_association::has_session(const pfcp::fseid_t& cp_fseid) {
  std::unique_lock<std::mutex> l(m_sessions);
  auto it = sessions.find(cp_fseid);
  if (it != sessions.end()) {
    return true;
  } else {
    return false;
  }
}
//------------------------------------------------------------------------------
void pfcp_association::notify_del_session(const pfcp::fseid_t& cp_fseid) {
  std::unique_lock<std::mutex> l(m_sessions);
  sessions.erase(cp_fseid);
}

//------------------------------------------------------------------------------
void pfcp_association::del_sessions() {
  std::unique_lock<std::mutex> l(m_sessions);
  sessions.clear();
}

//------------------------------------------------------------------------------
void pfcp_association::restore_n4_sessions() {
  std::unique_lock<std::mutex> l(m_sessions);
  if (sessions.size()) {
    is_restore_sessions_pending = true;
    std::unique_ptr<n4_session_restore_procedure> restore_proc =
        std::make_unique<n4_session_restore_procedure>(sessions);

    restore_proc->run();
  }
}

//---------------------------------------------------------------------------------------------
bool pfcp_association::find_interface_edge(
    const iface_type& type_match, std::vector<edge>& edges) {
  for (const auto& iface : m_upf_cfg.get_upf_info().getInterfaceUpfInfoList()) {
    iface_type type =
        iface_type_from_string(iface.getInterfaceType().getEnumString());
    if (type == type_match) {
      edges.emplace_back(edge::from_upf_info(m_upf_cfg.get_upf_info(), iface));
    }
  }
  // Because interfaceUpfInfoList is optional in TS 29.510 (why even?), we
  // just guess that this UPF has a N3 or N6 interface
  if (!m_upf_cfg.get_upf_info().interfaceUpfInfoListIsSet() ||
      m_upf_cfg.get_upf_info().getInterfaceUpfInfoList().empty()) {
    Logger::smf_app().info(
        "UPF Interface list is empty: Assume that the UPF has a N3 and a N6 "
        "interface.");
    edge e = edge::from_upf_info(m_upf_cfg.get_upf_info());
    e.type = type_match;
    edges.emplace_back(e);
  }

  return !edges.empty();
}

//---------------------------------------------------------------------------------------------
bool pfcp_association::find_n3_edge(std::vector<edge>& edges) {
  return find_interface_edge(iface_type::N3, edges);
}

//---------------------------------------------------------------------------------------------
bool pfcp_association::find_n6_edge(std::vector<edge>& edges) {
  bool success = find_interface_edge(iface_type::N6, edges);
  if (success) {
    for (auto& e : edges) {
      e.uplink = true;
    }
  }
  return success;
}

//------------------------------------------------------------------------------
bool pfcp_association::find_upf_edge(
    const std::shared_ptr<pfcp_association>& other_upf, edge& out_edge) {
  if (!m_upf_cfg.get_upf_info().interfaceUpfInfoListIsSet() ||
      !other_upf->get_upf_info().interfaceUpfInfoListIsSet()) {
    return false;
  }

  for (const auto& iface : m_upf_cfg.get_upf_info().getInterfaceUpfInfoList()) {
    if (iface.getEndpointFqdn() == other_upf->m_upf_cfg.get_host()) {
      out_edge = edge::from_upf_info(m_upf_cfg.get_upf_info(), iface);
      return true;
    }
    for (const auto& current_ip : iface.getIpv4EndpointAddresses()) {
      if (current_ip == other_upf->m_upf_cfg.get_host()) {
        out_edge = edge::from_upf_info(m_upf_cfg.get_upf_info(), iface);
        return true;
      }
    }
  }
  return false;
}

//------------------------------------------------------------------------------
std::string pfcp_association::get_printable_name() const {
  return m_upf_cfg.get_host();
}

//------------------------------------------------------------------------------
void pfcp_association::display() {
  std::string title_fmt = oai::config::get_title_formatter(2);
  std::string value_fmt = oai::config::get_value_formatter(3);

  std::string out = fmt::format(title_fmt, "PFCP Association");
  out.append(fmt::format(value_fmt, "Node ID: ", node_id.toString()));
  out.append(fmt::format(title_fmt, "UPF Profile"));
  out.append(m_upf_cfg.to_string("  "));

  std::stringstream ss(out);
  std::string line;

  while (std::getline(ss, line)) {
    Logger::smf_app().debug(line);
  }
}

//------------------------------------------------------------------------------
oai::model::nrf::UpfInfo pfcp_association::get_upf_info() const {
  return m_upf_cfg.get_upf_info();
}

//------------------------------------------------------------------------------
const oai::config::smf::upf& pfcp_association::get_upf_config() const {
  return m_upf_cfg;
}

//------------------------------------------------------------------------------
bool pfcp_association::serves_network(
    const oai::model::common::Snssai& snssai, const std::string& dnn) const {
  Logger::smf_app().info(
      "Verifying if UPF %s serves DNN %s and SNSSAI \n %s",
      get_printable_name(), dnn, snssai.to_string(0));
  for (const auto& snssai_item : get_upf_info().getSNssaiUpfInfoList()) {
    if (snssai_item.getSNssai() == snssai) {
      Logger::smf_app().debug(
          "UPF %s serves SNSSAI\n %s", get_printable_name(),
          snssai.to_string(0));
      for (const auto& dnn_item : snssai_item.getDnnUpfInfoList()) {
        if (dnn_item.getDnn() == dnn) {
          return true;
        }
        Logger::smf_app().debug(
            "UPF %s does NOT serve DNN %s", get_printable_name(), dnn);
      }
    } else {
      Logger::smf_app().debug(
          "UPF %s does NOT serve SNSSAI\n %s", get_printable_name(),
          snssai.to_string(0));
    }
  }

  return false;
}

/******************************************************************************/
/***************************** PFCP ASSOCIATIONS ******************************/
/******************************************************************************/

std::shared_ptr<pfcp_association> pfcp_associations::check_association_on_add(
    const pfcp::node_id_t& node_id,
    const pfcp::recovery_time_stamp_t& recovery_time_stamp,
    bool& restore_n4_sessions, bool use_function_features,
    const pfcp::up_function_features_s& function_features) {
  std::shared_ptr<pfcp_association> sa = {};
  if (get_association(node_id, sa)) {
    itti_inst->timer_remove(sa->timer_heartbeat);
    if (sa->recovery_time_stamp == recovery_time_stamp) {
      restore_n4_sessions = false;
    } else {
      restore_n4_sessions = true;
    }
    sa->recovery_time_stamp = recovery_time_stamp;
    if (use_function_features) {
      sa->function_features = {};
    } else {
      sa->function_features.first  = true;
      sa->function_features.second = function_features;
    }

    trigger_heartbeat_request_procedure(sa);
    return sa;
  }
  return {};  // empty ptr
}

//---------------------------------------------------------------------------------------------
bool pfcp_associations::resolve_upf_hostname(pfcp::node_id_t& node_id) {
  // TODO why is this even here? We can see in the logs that UPF IP is requested
  // before, at least for NRF scenario
  //  Resolve FQDN to get UPF IP address if necessary
  if (node_id.node_id_type == pfcp::NODE_ID_TYPE_FQDN) {
    Logger::smf_app().info("Node ID Type FQDN: %s", node_id.fqdn.c_str());

    std::string ip_addr = {};
    uint32_t port       = {0};
    uint8_t addr_type   = {0};

    if (!fqdn::resolve(node_id.fqdn, ip_addr, port, addr_type)) {
      Logger::smf_app().warn(
          "Add association with node (FQDN) %s: cannot resolve the hostname!",
          node_id.fqdn.c_str());
      return false;
    }
    switch (addr_type) {
      case 0:
        node_id.u1.ipv4_address = conv::fromString(ip_addr);
        return true;
      case 1:
        // TODO
        Logger::smf_app().warn(
            "Node ID Type FQDN: %s. IPv6 Addr, this mode has not been "
            "supported yet!",
            node_id.fqdn.c_str());
        return false;
      default:
        Logger::smf_app().warn("Unknown Address type");
        return false;
    }
  }
  return true;  // no FQDN so we just continue
}

//------------------------------------------------------------------------------
bool pfcp_associations::add_association(
    pfcp::node_id_t& node_id,
    const pfcp::recovery_time_stamp_t& recovery_time_stamp,
    bool& restore_n4_sessions,
    const pfcp::up_function_features_s& function_feature,
    const pfcp::enterprise_specific_s&, bool use_function_features, bool) {
  // TODO enterprise_specific is unused, also before refactor

  std::shared_ptr<pfcp_association> sa = check_association_on_add(
      node_id, recovery_time_stamp, restore_n4_sessions, use_function_features,
      function_feature);

  if (sa) return true;

  if (!resolve_upf_hostname(node_id)) return false;

  restore_n4_sessions = false;

  if (use_function_features) {
    sa = std::make_shared<pfcp_association>(
        get_upf_config(node_id), recovery_time_stamp, function_feature);
  } else {
    sa = std::make_shared<pfcp_association>(
        get_upf_config(node_id), recovery_time_stamp);
  }

  if (!associations_graph.full()) {
    associations_graph.insert_into_graph(sa);
    trigger_heartbeat_request_procedure(sa);
    Logger::smf_app().debug(
        "Added PFCP assocation with UPF config: \n %s",
        sa->get_upf_config().to_string(""));
    return true;
  } else {
    Logger::smf_app().error(
        "PFCP Association limit of %d exceed! Node %s is not added",
        node_id.toString(), PFCP_MAX_ASSOCIATIONS);
    return false;
  }
}

//------------------------------------------------------------------------------
bool pfcp_associations::add_association(
    pfcp::node_id_t& node_id,
    const pfcp::recovery_time_stamp_t& recovery_time_stamp,
    bool& restore_n4_sessions) {
  pfcp::up_function_features_s tmp_function{};
  pfcp::enterprise_specific_s tmp_enterprise{};
  return add_association(
      node_id, recovery_time_stamp, restore_n4_sessions, tmp_function,
      tmp_enterprise, false, false);
}

//------------------------------------------------------------------------------
bool pfcp_associations::add_association(
    pfcp::node_id_t& node_id,
    const pfcp::recovery_time_stamp_t& recovery_time_stamp,
    const pfcp::up_function_features_s& function_features,
    bool& restore_n4_sessions) {
  pfcp::enterprise_specific_s tmp{};
  return add_association(
      node_id, recovery_time_stamp, restore_n4_sessions, function_features, tmp,
      true, false);
}

//------------------------------------------------------------------------------
bool pfcp_associations::add_association(
    pfcp::node_id_t& node_id,
    const pfcp::recovery_time_stamp_t& recovery_time_stamp,
    const pfcp::up_function_features_s& function_features,
    const pfcp::enterprise_specific_s& enterprise_specific,
    bool& restore_n4_sessions) {
  return add_association(
      node_id, recovery_time_stamp, restore_n4_sessions, function_features,
      enterprise_specific, true, true);
}

//------------------------------------------------------------------------------
bool pfcp_associations::update_association(
    pfcp::node_id_t& node_id, pfcp::up_function_features_s& function_features) {
  std::shared_ptr<pfcp_association> sa =
      std::shared_ptr<pfcp_association>(nullptr);
  // TODO should we trigger sth in the graph as well or ongoing PDU sessions?
  if (get_association(node_id, sa)) {
    sa->function_features.first  = true;
    sa->function_features.second = function_features;
  } else {
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------
bool pfcp_associations::get_association(
    const pfcp::node_id_t& node_id, std::shared_ptr<pfcp_association>& sa) {
  std::shared_ptr<pfcp_association> association = {};
  std::size_t hash_node_id                      = {};
  pfcp::node_id_t node_id_tmp                   = node_id;

  // Resolve FQDN/IP Addr if necessary
  fqdn::resolve(node_id_tmp);

  // We suppose that by default hash map is made with node_id_type FQDN
  if (node_id_tmp.node_id_type == pfcp::NODE_ID_TYPE_FQDN) {
    hash_node_id = std::hash<pfcp::node_id_t>{}(node_id_tmp);
    association  = associations_graph.get_association(hash_node_id);
    if (association) {
      sa = association;
      return true;
    }
    node_id_tmp.node_id_type = pfcp::NODE_ID_TYPE_IPV4_ADDRESS;
  } else if (node_id_tmp.node_id_type == pfcp::NODE_ID_TYPE_IPV4_ADDRESS) {
    hash_node_id = std::hash<pfcp::node_id_t>{}(node_id_tmp);
    association  = associations_graph.get_association(hash_node_id);
    if (association) {
      sa = association;
      return true;
    }
    node_id_tmp.node_id_type = pfcp::NODE_ID_TYPE_FQDN;
  }

  // We didn't found association, may be because hash map is made with different
  // node type
  hash_node_id = std::hash<pfcp::node_id_t>{}(node_id_tmp);
  association  = associations_graph.get_association(hash_node_id);
  if (association) {
    sa = association;
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------
bool pfcp_associations::get_association(
    const pfcp::fseid_t& cp_fseid,
    std::shared_ptr<pfcp_association>& sa) const {
  std::shared_ptr<pfcp_association> association =
      associations_graph.get_association(cp_fseid);
  if (association) {
    sa = association;
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------
void pfcp_associations::restore_n4_sessions(const pfcp::node_id_t& node_id) {
  std::shared_ptr<pfcp_association> sa = {};
  if (get_association(node_id, sa)) {
    sa->restore_n4_sessions();
  }
}

//------------------------------------------------------------------------------
void pfcp_associations::trigger_heartbeat_request_procedure(
    std::shared_ptr<pfcp_association>& s) {
  s->timer_heartbeat = itti_inst->timer_setup(
      PFCP_ASSOCIATION_HEARTBEAT_INTERVAL_SEC, 0, TASK_SMF_N4,
      TASK_SMF_N4_TRIGGER_HEARTBEAT_REQUEST, s->hash_node_id);
}

//------------------------------------------------------------------------------
void pfcp_associations::initiate_heartbeat_request(
    timer_id_t timer_id, uint64_t arg2_user) {
  size_t hash_node_id = (size_t) arg2_user;
  auto association    = associations_graph.get_association(hash_node_id);
  if (association) {
    Logger::smf_n4().info(
        "PFCP HEARTBEAT PROCEDURE hash %u starting", hash_node_id);
    association->num_retries_timer_heartbeat = 0;
    smf_n4_inst->send_heartbeat_request(association);
  }
}

//------------------------------------------------------------------------------
void pfcp_associations::timeout_heartbeat_request(
    timer_id_t timer_id, uint64_t arg2_user) {
  size_t hash_node_id = (size_t) arg2_user;
  auto association    = associations_graph.get_association(hash_node_id);
  if (association) {
    if (association->num_retries_timer_heartbeat <
        PFCP_ASSOCIATION_HEARTBEAT_MAX_RETRIES) {
      Logger::smf_n4().info(
          "PFCP HEARTBEAT PROCEDURE hash %u TIMED OUT (retrie %d)",
          hash_node_id, association->num_retries_timer_heartbeat);
      association->num_retries_timer_heartbeat++;
      smf_n4_inst->send_heartbeat_request(association);
    } else {
      Logger::smf_n4().warn(
          "PFCP HEARTBEAT PROCEDURE FAILED after %d retries, remove the "
          "association with this UPF",
          PFCP_ASSOCIATION_HEARTBEAT_MAX_RETRIES);
      // Related session contexts and PFCP associations become invalid and may
      // be deleted-> Send request to SMF App to remove all associated
      // sessions and notify AMF accordingly
      std::shared_ptr<itti_n4_node_failure> itti_msg =
          std::make_shared<itti_n4_node_failure>(TASK_SMF_N4, TASK_SMF_APP);
      itti_msg->node_id = association->node_id;
      int ret           = itti_inst->send_msg(itti_msg);
      if (RETURNok != ret) {
        Logger::smf_n4().error(
            "Could not send ITTI message %s to task TASK_SMF_APP",
            itti_msg->get_msg_name());
      }

      // Remove UPF from the associations
      remove_association(hash_node_id);
      associations_graph.print_graph();
    }
  }
}

//------------------------------------------------------------------------------
void pfcp_associations::timeout_release_request(
    timer_id_t timer_id, uint64_t arg2_user) {
  size_t hash_node_id = (size_t) arg2_user;
  auto association    = associations_graph.get_association(hash_node_id);
  if (association) {
    Logger::smf_n4().info("PFCP RELEASE REQUEST hash %u", hash_node_id);
    smf_n4_inst->send_release_request(association);
  }
}

//------------------------------------------------------------------------------
void pfcp_associations::handle_receive_heartbeat_response(
    const uint64_t trxn_id) {
  auto association = associations_graph.get_association_for_trxn_id(trxn_id);

  if (association) {
    itti_inst->timer_remove(association->timer_heartbeat);
    trigger_heartbeat_request_procedure(association);
    return;
  }
}

//------------------------------------------------------------------------------
std::shared_ptr<upf_graph> pfcp_associations::select_up_node(
    const snssai_t& snssai, const std::string& dnn) {
  return associations_graph.select_upf_node(snssai, dnn);
}

//------------------------------------------------------------------------------
std::shared_ptr<upf_graph> pfcp_associations::select_up_node(
    const SmPolicyDecision& decision, const snssai_t& snssai,
    const std::string& dnn) {
  return associations_graph.select_upf_nodes(decision, snssai, dnn);
}

//------------------------------------------------------------------------------
void pfcp_associations::notify_add_session(
    const pfcp::node_id_t& node_id, const pfcp::fseid_t& cp_fseid) {
  std::shared_ptr<pfcp_association> sa = {};
  if (get_association(node_id, sa)) {
    sa->notify_add_session(cp_fseid);
  }
}

//------------------------------------------------------------------------------
void pfcp_associations::notify_del_session(const pfcp::fseid_t& cp_fseid) {
  std::shared_ptr<pfcp_association> sa = {};
  if (get_association(cp_fseid, sa)) {
    sa->notify_del_session(cp_fseid);
  }
}

bool pfcp_associations::add_peer_candidate_node(
    const oai::config::smf::upf& upf_cfg) {
  std::unique_lock peer_lock(m_mutex);
  for (const auto& association : pending_associations) {
    if (association->node_id == upf_cfg.get_node_id()) {
      Logger::smf_app().debug(
          "UPF %s already exists on pending associations.",
          association->get_printable_name());
      return true;
    }
  }

  pending_associations.emplace_back(
      std::make_shared<pfcp_association>(upf_cfg));
  return true;
}

//------------------------------------------------------------------------------
bool pfcp_associations::remove_association(
    const std::string& node_instance_id) {
  // TODO
  return true;
}

//------------------------------------------------------------------------------
bool pfcp_associations::remove_association(const int32_t& hash_node_id) {
  auto association = associations_graph.get_association(hash_node_id);
  return associations_graph.remove_association(association);
}

//------------------------------------------------------------------------------
oai::config::smf::upf pfcp_associations::get_upf_config(
    const pfcp::node_id_t& node_id) const {
  std::shared_lock peer_lock(m_mutex);
  for (const auto& it : pending_associations) {
    if (it->node_id == node_id) {
      Logger::smf_app().debug(
          "Found UPF config for pending PFCP association %s",
          node_id.toString());
      return it->get_upf_config();
    }
  }

  for (const auto& upf : smf_cfg->smf()->get_upfs()) {
    if (upf.get_node_id() == node_id) {
      Logger::smf_app().debug(
          "Found UPF config for UPF-associated association %s",
          node_id.toString());
      return upf;
    }
  }
  Logger::smf_app().debug(
      "Did not find a UPF config for UPF-associated association %s. Use empty "
      "UPF profile with one assumed N3 and N6 interface.",
      node_id.toString());
  oai::model::nrf::UpfInfo info;
  oai::model::nrf::InterfaceUpfInfoItem n3;
  oai::model::nrf::InterfaceUpfInfoItem n6;
  oai::model::nrf::UPInterfaceType n3_type;
  n3_type.setEnumValue(
      oai::model::nrf::UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3);
  oai::model::nrf::UPInterfaceType n6_type;
  n6_type.setEnumValue(
      oai::model::nrf::UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6);
  n3.setInterfaceType(n3_type);
  n6.setInterfaceType(n6_type);

  info.setInterfaceUpfInfoList(
      std::vector<oai::model::nrf::InterfaceUpfInfoItem>{n3, n6});

  // we use the default UPF
  auto upf_cfg = oai::config::smf::DEFAULT_UPF;
  std::string host;

  switch (node_id.node_id_type) {
    case pfcp::NODE_ID_TYPE_FQDN:
      host = node_id.fqdn;
      break;
    case pfcp::NODE_ID_TYPE_IPV4_ADDRESS:
      host = conv::toString(node_id.u1.ipv4_address);
      break;
    case pfcp::NODE_ID_TYPE_IPV6_ADDRESS:
      host = conv::toString(node_id.u1.ipv6_address);
      break;
    default:
      Logger::smf_app().error(
          "Wrong PFCP Node ID type. Use default Host %s", upf_cfg.get_host());
      host = upf_cfg.get_host();
      break;
  }
  upf_cfg = oai::config::smf::upf(
      host, upf_cfg.get_port(), upf_cfg.enable_usage_reporting(),
      upf_cfg.enable_dl_pdr_in_session_establishment(),
      upf_cfg.get_local_n3_ip());

  upf_cfg.set_upf_info(info);

  return upf_cfg;
}

/******************************************************************************/
/***************************** UPF GRAPH
 * **************************************/
/******************************************************************************/

//------------------------------------------------------------------------------
void upf_graph::insert_into_graph(const std::shared_ptr<pfcp_association>& sa) {
  std::vector<std::shared_ptr<pfcp_association>> all_upfs;
  std::vector<std::pair<edge, edge>> edges;

  std::vector<edge> n3_edges;
  std::vector<edge> n6_edges;
  // iterate through all interfaces and see if the FQDN/IPv4 addresses of
  // existing UPFs match
  std::unique_lock graph_lock(graph_mutex);

  // Find N9 interfaces
  for (const auto& it : adjacency_list) {
    edge src_dst;
    edge dst_src;
    bool found = sa->find_upf_edge(it.first, src_dst);
    if (found) {
      // now other direction
      found = it.first->find_upf_edge(sa, dst_src);
      if (found) {
        all_upfs.push_back(it.first);
        edges.emplace_back(src_dst, dst_src);

      } else {
        Logger::smf_app().warn(
            "Found edge from %s to %s, but not in the other direction. You "
            "have an error in your UPF configuration. This UPF is not "
            "added "
            "to the graph",
            sa->get_printable_name().c_str(),
            it.first->get_printable_name().c_str());
      }
    }
  }
  // unlock as here we need unique locks
  graph_lock.unlock();
  // Find N6 or N3 edges
  sa->find_n3_edge(n3_edges);
  sa->find_n6_edge(n6_edges);

  if (all_upfs.empty()) {
    Logger::smf_app().debug(
        "Could not find other edges for UPF, just add UPF as a node");
    add_upf_graph_node(sa);
  } else {
    for (int i = 0; i < all_upfs.size(); i++) {
      add_upf_graph_edge(sa, all_upfs[i], edges[i].first, edges[i].second);
    }
  }
  for (auto n3_edge : n3_edges) {
    add_upf_graph_edge(sa, n3_edge);
  }
  for (auto n6_edge : n6_edges) {
    add_upf_graph_edge(sa, n6_edge);
  }
  print_graph();
}

// TODO can we get rid of this inefficient method?
//------------------------------------------------------------------------------
std::shared_ptr<pfcp_association> upf_graph::get_association(
    const std::size_t association_hash) const {
  std::shared_lock graph_lock(graph_mutex);

  for (const auto& it : adjacency_list) {
    if (it.first->hash_node_id == association_hash) {
      return it.first;
    }
  }
  return {};
}

// TODO can we get rid of this inefficient method?
//------------------------------------------------------------------------------
std::shared_ptr<pfcp_association> upf_graph::get_association(
    const pfcp::fseid_t& cp_fseid) const {
  std::shared_lock graph_lock(graph_mutex);

  for (const auto& it : adjacency_list) {
    if (it.first->has_session(cp_fseid)) {
      return it.first;
    }
  }
  return {};
}

// TODO can we get rid of this inefficient method?
//------------------------------------------------------------------------------
std::shared_ptr<pfcp_association> upf_graph::get_association_for_trxn_id(
    const uint64_t trxn_id) const {
  std::shared_lock graph_lock(graph_mutex);
  for (const auto& it : adjacency_list) {
    if (it.first->trxn_id_heartbeat == trxn_id) {
      return it.first;
    }
  }
  return {};
}

//------------------------------------------------------------------------------
bool upf_graph::remove_association(
    const std::shared_ptr<pfcp_association>& association) {
  std::unique_lock graph_lock(graph_mutex);

  auto it = adjacency_list.find(association);

  if (it != adjacency_list.end()) {
    std::shared_ptr<pfcp_association> to_delete_association = it->first;
    edge to_delete_edge;

    // go through all the other nodes and remove this association from
    // adjacency list
    for (auto other_it : adjacency_list) {
      auto edge_it = other_it.second.begin();

      while (edge_it != other_it.second.end()) {
        if (edge_it->association == it->first) {
          edge_it = other_it.second.erase(edge_it);
        } else {
          edge_it++;
        }
      }
      // TODO does this work?
    }
    adjacency_list.erase(association);
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------------------------
void upf_graph::add_upf_graph_edge(
    const std::shared_ptr<pfcp_association>& source,
    const edge& edge_info_src_dst) {
  add_upf_graph_node(source);

  std::unique_lock lock_graph(graph_mutex);
  auto it_src = adjacency_list.find(source);
  if (it_src == adjacency_list.end()) {
    return;
  }
  bool exists = false;
  for (const auto& edge : it_src->second) {
    if (edge == edge_info_src_dst) {
      exists = true;
      break;
    }
  }

  if (!exists) {
    it_src->second.push_back((edge_info_src_dst));
    Logger::smf_app().debug(
        "Successfully added UPF graph edge for %s: %s",
        source->get_printable_name(), edge_info_src_dst.to_string().c_str());
  }
}

//------------------------------------------------------------------------------
void upf_graph::add_upf_graph_edge(
    const std::shared_ptr<pfcp_association>& source,
    const std::shared_ptr<pfcp_association>& dest, edge& edge_info_src_dst,
    edge& edge_info_dst_src) {
  edge_info_src_dst.association = dest;
  edge_info_dst_src.association = source;
  add_upf_graph_edge(source, edge_info_src_dst);
  add_upf_graph_edge(dest, edge_info_dst_src);
}

//------------------------------------------------------------------------------
void upf_graph::add_upf_graph_node(
    const std::shared_ptr<pfcp_association>& node) {
  std::unique_lock lock_graph(graph_mutex);

  auto it = adjacency_list.find(node);

  if (it == adjacency_list.end()) {
    std::vector<edge> lst = {};
    adjacency_list.insert(std::make_pair(node, lst));

    Logger::smf_app().debug(
        "Successfully added UPF node: %s, (%u)", node->get_printable_name(),
        node->hash_node_id);
  }
}

//------------------------------------------------------------------------------
void upf_graph::print_graph() {
  std::shared_lock lock_graph(graph_mutex);
  std::string output;

  if (adjacency_list.empty()) {
    Logger::smf_app().debug("UPF graph is empty");
    return;
  }

  for (const auto& it : adjacency_list) {
    output.append("* ").append(it.first->get_printable_name()).append(" --> ");
    for (const auto& edge : it.second) {
      output.append(edge.to_string()).append(", ");
    }
    output.append("\n");
  }

  Logger::smf_app().debug("UPF graph ");
  Logger::smf_app().debug("%s", output.c_str());
}

//------------------------------------------------------------------------------
void upf_graph::dfs_next_upf(
    std::vector<edge>& info_dl, std::vector<edge>& info_ul,
    std::shared_ptr<pfcp_association>& upf) {
  // we need unique lock as visited array and stack is written
  std::unique_lock lock_graph(graph_mutex);

  if (!stack_asynch.empty()) {
    std::shared_ptr<pfcp_association> association = stack_asynch.top();
    stack_asynch.pop();

    auto node_it = adjacency_list.find(association);
    if (node_it == adjacency_list.end()) {
      // TODO this scenario might happen when in the meantime one of the UPFs
      // became inavailable. here we should terminate the PDU session and all
      // the other already established sessions
      Logger::smf_app().error(
          "DFS Asynch: node ID does not exist in UPF graph, this should not "
          "happen");
      return;
    }

    // here we need to check if we have more than one unvisited N9_UL edge
    // if yes, we have a UL CL scenario, and we need to finish the other
    // branch first
    Logger::smf_app().debug(
        "DFS Asynch: Handle UPF %s",
        node_it->first->get_printable_name().c_str());
    bool unvisited_n9_node = false;
    // as we have no diamond shape, we only need to care about this in UL
    // direction
    if (uplink_asynch) {
      for (const auto& edge_it : node_it->second) {
        if (edge_it.association) {
          if (!visited_asynch[edge_it.association]) {
            if (edge_it.type == iface_type::N9 && edge_it.uplink) {
              unvisited_n9_node = true;
              Logger::smf_app().debug(
                  "UL CL scenario: We have a node with an unvisited N9_UL "
                  "node.");
              break;
            }
          }
        }
      }
    }
    // we removed the current UPF from stack, but did not set visited
    // so it is re-visited again from another branch
    // we also dont add the neighbors yet
    if (!unvisited_n9_node) {
      visited_asynch[association] = true;

      for (const auto& edge_it : node_it->second) {
        // first add all neighbors to the stack
        if (edge_it.association) {
          if (!visited_asynch[edge_it.association]) {
            stack_asynch.push(edge_it.association);
          }
        }
      }
    }
    upf = node_it->first;

    // set correct edge infos
    for (auto& edge_it : node_it->second) {
      // copy QOS Flow for the whole graph
      //  TODO currently only one flow is supported
      if (edge_it.qos_flows.empty()) {
        std::shared_ptr<smf_qos_flow> flow =
            std::make_shared<smf_qos_flow>(qos_flow_asynch);
        edge_it.qos_flows.emplace_back(flow);
      }
      // TODO thought for refactor: It would be much nicer if it would be the
      // same edge object so we dont have to do that

      // pointer is not null -> N9 interface
      if (edge_it.association) {
        // we add the TEID here for the edge in the other direction
        // direct access is safe as we know the edge exists
        auto edge_node = adjacency_list[edge_it.association];
        for (auto edge_edge : edge_node) {
          if (edge_edge.qos_flows.empty()) {
            edge_edge.qos_flows.emplace_back(
                std::make_shared<smf_qos_flow>(qos_flow_asynch));
          }

          if (edge_edge.association &&
              edge_edge.association == node_it->first) {
            if (edge_edge.type == iface_type::N9 && edge_edge.uplink) {
              // downlink direction
              edge_it.qos_flows[0]->dl_fteid = edge_edge.qos_flows[0]->dl_fteid;
            } else if (edge_edge.type == iface_type::N9) {
              edge_it.qos_flows[0]->ul_fteid = edge_edge.qos_flows[0]->ul_fteid;
            }
          }
        }
      }

      // set the correct edges to return
      if (edge_it.uplink) {
        info_ul.push_back(edge_it);
      } else {
        info_dl.push_back(edge_it);
      }
      current_upf_asynch      = upf;
      current_edges_dl_asynch = info_dl;
      current_edges_ul_asynch = info_ul;
    }
  }
}

//------------------------------------------------------------------------------
void upf_graph::dfs_current_upf(
    std::vector<edge>& info_dl, std::vector<edge>& info_ul,
    std::shared_ptr<pfcp_association>& upf) {
  std::shared_lock graph_lock(graph_mutex);
  upf     = current_upf_asynch;
  info_dl = current_edges_dl_asynch;
  info_ul = current_edges_ul_asynch;
}

//------------------------------------------------------------------------------
void upf_graph::start_asynch_dfs_procedure(
    bool uplink, smf_qos_flow& qos_flow) {
  std::unique_lock graph_lock(graph_mutex);
  if (!stack_asynch.empty()) {
    Logger::smf_app().error(
        "Started DFS procedure, but old stack is not empty. Failure");
  }
  // clear the stack and visited array
  stack_asynch    = {};
  visited_asynch  = {};
  qos_flow_asynch = qos_flow;
  uplink_asynch   = uplink;

  // uplink start at the exit nodes, downlink start at access nodes, do not
  // actually do DFS but put them on the stack
  for (auto& it : adjacency_list) {
    for (auto& edge : it.second) {
      if ((uplink && edge.type == iface_type::N6) ||
          (!uplink && edge.type == iface_type::N3)) {
        stack_asynch.push(it.first);
        break;
      }
    }
  }
}

//---------------------------------------------------------------------------------------------
edge upf_graph::get_access_edge() const {
  std::shared_lock graph_lock(graph_mutex);
  edge info;
  for (const auto& node_it : adjacency_list) {
    for (const auto& edge_it : node_it.second) {
      if (edge_it.type == iface_type::N3) {
        return edge_it;
      }
    }
  }
  Logger::smf_app().warn(
      "Try to get the access edge for this graph, but it does not exist");
  return info;
}

//---------------------------------------------------------------------------------------------
void upf_graph::update_edge_info(
    const std::shared_ptr<pfcp_association>& upf, const edge& info) {
  std::unique_lock graph_lock(graph_mutex);
  auto it = adjacency_list.find(upf);
  if (it == adjacency_list.end()) {
    Logger::smf_app().error("Want to update edge, but UPF does not exist!");
    return;
  }

  for (auto& edge_it : it->second) {
    if (edge_it == info) {
      edge_it = info;
      break;
    }
  }
}

//---------------------------------------------------------------------------------------------
std::shared_ptr<upf_graph> upf_graph::select_upf_node(
    const snssai_t& snssai, const std::string& dnn) {
  Logger::smf_app().info("Select UPF Node");
  auto upf_graph_ptr = std::make_shared<upf_graph>();
  std::shared_lock graph_lock(graph_mutex);
  std::shared_ptr<pfcp_association> used_upf = {};
  if (adjacency_list.empty()) {
    Logger::smf_app().warn("No UPF available. SMF selection failed");
  }
  for (const auto& [current_upf, edges] : adjacency_list) {
    Logger::smf_app().debug("Current UPF info");
    current_upf->display();

    if (current_upf->serves_network(snssai.to_model_snssai(), dnn)) {
      used_upf = current_upf;
      break;
    }
  }
  if (!used_upf) {
    // In case previous round did not produce anything, just return first UPF
    used_upf = adjacency_list.begin()->first;
    Logger::smf_app().warn(
        "Did not find UPF that serves selected SNSSAI and DNN. We select any "
        "UPF");
  }
  // TODO this is not very smart and not beautiful
  // it would be better to integrate this into the DFS
  // TBD when refactoring DFS
  // null check is not necessary as it cannot be
  const auto& it_upf = adjacency_list.find(used_upf);
  bool has_access    = false;
  bool has_exit      = false;
  edge access_edge   = {};
  edge exit_edge     = {};
  for (const auto& edge : it_upf->second) {
    if (edge.type == iface_type::N3) {
      access_edge = edge;
      has_access  = true;
    }
    if (edge.type == iface_type::N6) {
      exit_edge = edge;
      has_exit  = true;
    }
  }
  if (has_access && has_exit) {
    Logger::smf_app().info(
        "Found UPF for this PDU session: %s", used_upf->get_printable_name());
    upf_graph_ptr->add_upf_graph_node(used_upf);
    upf_graph_ptr->add_upf_graph_edge(used_upf, access_edge);
    upf_graph_ptr->add_upf_graph_edge(used_upf, exit_edge);
    return upf_graph_ptr;
  }

  if (upf_graph_ptr->adjacency_list.empty()) {
    upf_graph_ptr.reset();
    Logger::smf_app().warn("UPF could not be selected for the PDU session");
  }

  return upf_graph_ptr;
}

//---------------------------------------------------------------------------------------------
std::shared_ptr<upf_graph> upf_graph::select_upf_node(
    const int node_selection_criteria) {
  std::shared_lock graph_lock(graph_mutex);
  std::shared_ptr<pfcp_association> association = {};
  std::shared_ptr<upf_graph> upf_graph_ptr      = std::make_shared<upf_graph>();

  Logger::smf_app().error("Called non-implemented UPF selection function");

  if (adjacency_list.empty()) {
    upf_graph_ptr.reset();
    return upf_graph_ptr;
  }

  for (const auto& it : adjacency_list) {
    association = it.first;
    // TODO
    switch (node_selection_criteria) {
      case NODE_SELECTION_CRITERIA_BEST_MAX_HEARBEAT_RTT:
      case NODE_SELECTION_CRITERIA_MIN_PFCP_SESSIONS:
      case NODE_SELECTION_CRITERIA_MIN_UP_TIME:
      case NODE_SELECTION_CRITERIA_MAX_AVAILABLE_BW:
      case NODE_SELECTION_CRITERIA_NONE:
      default:
        break;
    }
    // just add first node and then break
    upf_graph_ptr->add_upf_graph_node(association);
    for (auto edge : it.second) {
      if (edge.type != iface_type::N9) {
        upf_graph_ptr->add_upf_graph_edge(it.first, edge);
      }
    }
    return upf_graph_ptr;
  }
  return upf_graph_ptr;
}

//------------------------------------------------------------------------------
// TODO in the current implementation, UL CL needs to be the first node,
// otherwise it is not explored anymore when graph is merged
std::shared_ptr<upf_graph> upf_graph::select_upf_nodes(
    const SmPolicyDecision& policy_decision, const snssai_t& snssai,
    const std::string& dnn) {
  // TODO move this maybe
  std::unique_lock graph_lock(graph_mutex);

  if (!policy_decision.pccRulesIsSet() ||
      !policy_decision.traffContDecsIsSet()) {
    Logger::smf_app().warn(
        "Cannot build UPF graph for PDU session when pcc rules or traffic "
        "control description is missing");
  }

  std::map<std::string, PccRule> pcc_rules = policy_decision.getPccRules();
  std::map<std::string, TrafficControlData> traffic_conts =
      policy_decision.getTraffContDecs();
  std::unordered_set<uint32_t> precedences;

  // std::shared_ptr<upf_graph> correct_sub_graph_ptr;
  std::unordered_set<std::string> dnais_from_all_rules;
  std::shared_ptr<upf_graph> sub_graph_ptr = {};

  // run DFS for each PCC rule, get different graphs and merge them

  for (const auto& rule : pcc_rules) {
    std::unordered_set<std::string> dnais;
    pfcp::redirect_information_t redirect_information = {};
    if (!rule.second.getRefTcData().empty()) {
      // we just take the first traffic control, as defined in the standard
      // see Note 1 in table 5.6.2.6-1 in TS29.512
      std::string tc_data_id = rule.second.getRefTcData()[0];

      auto traffic_it = traffic_conts.find(tc_data_id);
      if (traffic_it != traffic_conts.end()) {
        TrafficControlData data = traffic_it->second;
        traffic_it->second.getTcId();
        if (traffic_it->second.routeToLocsIsSet()) {
          for (const auto& route : traffic_it->second.getRouteToLocs()) {
            dnais.insert(route.getDnai());
            dnais_from_all_rules.insert(route.getDnai());
          }
          if (traffic_it->second.redirectInfoIsSet()) {
            RedirectInformation redirectInfo =
                traffic_it->second.getRedirectInfo();
            if (redirectInfo.isRedirectEnabled()) {
              redirect_information.redirect_address_type = static_cast<uint8_t>(
                  redirectInfo.getRedirectAddressType().getEnumValue());
              redirect_information.redirect_server_address =
                  redirectInfo.getRedirectServerAddress();
              redirect_information.redirect_server_address_length =
                  redirect_information.redirect_server_address.size();
            }
          }
        } else {
          Logger::smf_app().warn("Route to location is not set in PCC rules");
        }
      }
    } else {
      continue;
    }
    std::string flow_description = rule.second.getFirstFlowDescription();
    if (flow_description.empty()) {
      Logger::smf_app().warn(
          "Flow Description is empty. Skip PCC rule %s", rule.first.c_str());
    }

    std::unordered_map<
        std::shared_ptr<pfcp_association>, bool,
        std::hash<std::shared_ptr<pfcp_association>>>
        visited;

    // here we start the DFS algorithm for all start nodes because we can
    // have disconnected graphs

    uint32_t precedence = rule.second.getPrecedence();
    if (auto it = precedences.find(precedence) != precedences.end()) {
      Logger::smf_app().warn(
          "UPF graph selection failed: The precedences in the PCC rule "
          "are not unique. Aborting selection.");
      return nullptr;
    }
    precedences.insert(precedence);

    for (const auto& node : adjacency_list) {
      if (!visited[node.first]) {
        bool has_n3 = false;
        for (const auto& edge : node.second) {
          if (edge.type == iface_type::N3) {
            has_n3 = true;
            break;
          }
        }

        if (has_n3) {
          // need to make a copy in case the algorithm adds nodes to the graph
          // and then the graph is wrong
          std::shared_ptr<upf_graph> sub_graph_copy_ptr;
          if (sub_graph_ptr) {
            sub_graph_copy_ptr = std::make_shared<upf_graph>(*sub_graph_ptr);
          } else {
            sub_graph_ptr = std::make_shared<upf_graph>();
          }
          set_dfs_selection_criteria(
              dnais, flow_description, precedence, snssai, dnn,
              redirect_information);

          create_subgraph_dfs(sub_graph_ptr, node.first, visited);
          // Verify the merged graph with all DNAIs so far
          sub_graph_ptr->set_dfs_selection_criteria(
              dnais_from_all_rules, flow_description, precedence, snssai, dnn,
              redirect_information);

          if (!sub_graph_ptr->verify()) {
            // in case copy is null, new subgraph_ptr is also null, and we
            // create a new upf graph
            sub_graph_ptr = sub_graph_copy_ptr;
          } else {
            // this PCC rule is covered
            break;
          }
        }
      }
    }
  }

  // Now we verify the merged graph
  if (sub_graph_ptr) {
    sub_graph_ptr->set_dfs_selection_criteria(
        dnais_from_all_rules, dfs_flow_description, dfs_precedence, snssai, dnn,
        dfs_redirect_information);
  }
  if (sub_graph_ptr && sub_graph_ptr->verify()) {
    Logger::smf_app().info("Dynamic UPF selection successful.");
    sub_graph_ptr->print_graph();

    // now fix the flow description in case of an UL CL
    for (auto node : sub_graph_ptr->adjacency_list) {
      // use default flow description when there is only one edge
      if (node.second.size() == 1) {
        node.second.front().flow_description = DEFAULT_FLOW_DESCRIPTION;
      }
    }
  } else {
    Logger::smf_app().info("Dynamic UPF selection failed");
    sub_graph_ptr.reset();
  }

  return sub_graph_ptr;
}

//---------------------------------------------------------------------------------------------
bool upf_graph::verify() {
  int access_count = 0;
  bool has_exit    = false;
  std::unordered_set<std::string> all_dnais_in_graph;
  for (const auto& node : adjacency_list) {
    for (const auto& edge : node.second) {
      if (edge.type == iface_type::N3) {
        access_count++;
      } else if (edge.type == iface_type::N6) {
        has_exit = true;
      }
      all_dnais_in_graph.insert(edge.used_dnai);
    }
  }

  if (access_count > 1) {
    Logger::smf_app().info(
        "UPF graph has more than one access node, this is not supported. "
        "Please check your PCC rules and UPF configuration");
    return false;
  }

  // special case, here we allow one DNAI less
  if (adjacency_list.size() == 1 &&
      all_dnais_in_graph.size() == dfs_all_dnais.size() - 1) {
    Logger::smf_app().info(
        "Found UPF graph that serves DNAIs %s",
        get_dnai_list(all_dnais_in_graph).c_str());
    return true;
  }

  if (all_dnais_in_graph.size() != dfs_all_dnais.size()) {
    Logger::smf_app().debug(
        "Found UPF graph that serves DNAIs %s, but not all DNAIs from rule "
        "are covered (%s)",
        get_dnai_list(all_dnais_in_graph).c_str(),
        get_dnai_list(dfs_all_dnais).c_str());
    print_graph();
    return false;
  }

  if (access_count == 1 && has_exit) {
    Logger::smf_app().info(
        "Found UPF graph that serves DNAIs %s",
        get_dnai_list(all_dnais_in_graph).c_str());
    return true;
  } else if (access_count == 0 && !has_exit) {
    Logger::smf_app().info(
        "UPF graph that serves all DNAIs %s cannot be used, because it has "
        "neither entry nor exit nodes",
        get_dnai_list(all_dnais_in_graph).c_str());
    print_graph();
  } else if (access_count != 1) {
    Logger::smf_app().info(
        "UPF graph that serves all DNAIs %s cannot be used, because it does "
        "not have an access node",
        get_dnai_list(all_dnais_in_graph).c_str());
    print_graph();
  } else if (!has_exit) {
    Logger::smf_app().info(
        "UPF graph that serves all DNAIs %s cannot be used, because it does "
        "not have an exit node",
        get_dnai_list(all_dnais_in_graph).c_str());
    print_graph();
  }
  return false;
}

//---------------------------------------------------------------------------------------------
std::string upf_graph::get_dnai_list(const std::unordered_set<string>& dnais) {
  std::string out = {};

  for (const auto& dnai : dnais) {
    out.append(dnai).append(", ");
  }
  if (dnais.size() > 1) {
    out.erase(out.size() - 2);
  }
  return out;
}

//---------------------------------------------------------------------------------------------
void upf_graph::set_dfs_selection_criteria(
    const std::unordered_set<std::string>& all_dnais,
    const std::string& flow_description, uint32_t precedence,
    const snssai_t& snssai, const std::string& dnn,
    const pfcp::redirect_information_t& redirect_information) {
  dfs_all_dnais            = all_dnais;
  dfs_flow_description     = flow_description;
  dfs_precedence           = precedence;
  dfs_snssai               = snssai;
  dfs_dnn                  = dnn;
  dfs_redirect_information = redirect_information;
}

//---------------------------------------------------------------------------------------------
void upf_graph::create_subgraph_dfs(
    std::shared_ptr<upf_graph>& sub_graph,
    const std::shared_ptr<pfcp_association>& start_node,
    std::unordered_map<
        std::shared_ptr<pfcp_association>, bool,
        std::hash<std::shared_ptr<pfcp_association>>>& visited) {
  std::stack<std::shared_ptr<pfcp_association>> stack;
  stack.push(start_node);

  while (!stack.empty()) {
    std::shared_ptr<pfcp_association> node = stack.top();
    stack.pop();
    visited[node] = true;

    auto node_it = adjacency_list.find(node);
    if (node_it == adjacency_list.end()) {
      Logger::smf_app().error(
          "DFS: node ID does not exist in UPF graph, this should not happen");
      continue;
    }

    // DFS: Go through all edges and check if the UPF serves one of the DNAIs
    // from the PCC rule
    for (auto edge_it : node_it->second) {
      std::string found_dnai;
      if (!edge_it.serves_network(
              dfs_dnn, dfs_snssai, dfs_all_dnais, found_dnai)) {
        continue;  // do not consider this edge, does not serve DNN or SNSSAI or
                   // any DNAI
      }
      edge_it.used_dnai = found_dnai;

      edge_it.flow_description = dfs_flow_description;

      edge_it.redirect_information = dfs_redirect_information;
      // TODO move this precedence to the QOS FLOW?
      // refactor the way qos_flow is handled in general
      edge_it.precedence = dfs_precedence;

      // N3 or N6 edge, just add
      if (!edge_it.association) {
        if (edge_it.type == iface_type::N3) {
          edge_it.uplink = false;
        } else if (edge_it.type == iface_type::N6) {
          edge_it.uplink = true;
        }
        sub_graph->add_upf_graph_edge(node_it->first, edge_it);
      } else if (!visited[edge_it.association]) {
        std::string edge_upf = edge_it.association->get_printable_name();
        edge src_dst         = edge_it;
        src_dst.uplink       = true;  // N9 uplink as we start at access
        edge dst_src;

        // for the other direction we need to find this element in the
        // graph and find the original node; direct access is safe as we know
        // this element exists in graph as we have double lists

        // Note: We could also remove this step as this node is evaluated
        // anyway but then we need to somehow track the visited
        //  This is O(#edges_per_upf) so quite small
        auto edge_node = adjacency_list[edge_it.association];
        for (const auto& edge_edge : edge_node) {
          if (edge_edge.association == node_it->first) {
            dst_src        = edge_edge;
            dst_src.uplink = false;
            std::string used_dnai;
            if (!edge_edge.serves_network(
                    dfs_dnn, dfs_snssai, dfs_all_dnais, used_dnai)) {
              Logger::smf_app().error(
                  "Back-Edge in DFS does not serve network. check your "
                  "configuration");
              break;
            }
            dst_src.used_dnai = used_dnai;
          }
        }
        sub_graph->add_upf_graph_edge(
            node_it->first, edge_it.association, src_dst, dst_src);
        stack.push(edge_it.association);
      }
    }
  }
}

//---------------------------------------------------------------------------------------------
bool upf_graph::full() const {
  std::shared_lock graph_lock(graph_mutex);

  return adjacency_list.size() >= PFCP_MAX_ASSOCIATIONS;
}

std::string upf_graph::to_string(const std::string& indent) const {
  std::shared_lock graph_lock(graph_mutex);

  for (const auto& node : adjacency_list) {
    for (const auto& edge : node.second) {
      if (edge.type == iface_type::N3) {
        return to_string_from_start_node(indent, node.first);
      }
    }
  }

  return indent + "Invalid graph";
}

std::string upf_graph::to_string_from_start_node(
    const std::string& indent,
    const std::shared_ptr<pfcp_association>& start) const {
  std::shared_lock graph_lock(graph_mutex);

  std::unordered_map<
      std::shared_ptr<pfcp_association>, bool,
      std::hash<std::shared_ptr<pfcp_association>>>
      visited;
  std::list<std::shared_ptr<pfcp_association>> queue;

  std::string output;

  auto node_it = adjacency_list.find(start);
  if (node_it == adjacency_list.end()) {
    return output.append(indent)
        .append("Node ")
        .append(start->get_printable_name())
        .append(" does not exist in UPF graph.");
  }

  visited[start] = true;
  queue.push_back(start);
  std::map<std::string, std::string> output_per_iface_type;

  while (!queue.empty()) {
    auto node_queue = queue.front();
    node_it         = adjacency_list.find(node_queue);
    if (node_it == adjacency_list.end()) continue;

    queue.pop_front();

    for (const auto& edge : node_it->second) {
      // skip N6 output
      if (edge.type == iface_type::N6) continue;

      std::string iface = pfcp_association::string_from_iface_type(edge.type);
      if (!edge.nw_instance.empty()) {
        iface.append(": ").append(edge.nw_instance);
      }

      for (const auto& flow : edge.qos_flows) {
        // N3
        if (!edge.association) {
          output_per_iface_type[iface].append(flow->toString(indent + "\t"));
        }
        // N9
        else if (!visited[edge.association]) {
          output_per_iface_type[iface].append(flow->toString(indent + "\t"));
          visited[edge.association] = true;
          queue.push_back(edge.association);
        }
      }
    }
  }

  for (const auto& o : output_per_iface_type) {
    output.append(indent).append(o.first).append(":\n");
    output.append(o.second);
  }

  return output;
}
