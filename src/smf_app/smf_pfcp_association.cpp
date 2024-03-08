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
using namespace oai::model::nrf;

extern itti_mw* itti_inst;
extern smf_n4* smf_n4_inst;
extern std::unique_ptr<oai::config::smf::smf_config> smf_cfg;

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
UpfInfo pfcp_association::get_upf_info() const {
  return m_upf_cfg.get_upf_info();
}

//------------------------------------------------------------------------------
const oai::config::smf::upf& pfcp_association::get_upf_config() const {
  return m_upf_cfg;
}

//------------------------------------------------------------------------------
bool pfcp_association::serves_network(
    const oai::model::common::Snssai& snssai, const std::string& dnn) const {
  if (get_upf_info().getSNssaiUpfInfoList().empty()) {
    Logger::smf_app().info(
        "UPF does not have SNSSAIs configured, accept any SNSSAI");
    return true;
  }

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
        node_id.u1.ipv4_address = oai::utils::conv::fromString(ip_addr);
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
    const upf_selection_criteria& selection_criteria) {
  return associations_graph.select_upf_nodes(selection_criteria);
}

//------------------------------------------------------------------------------
std::shared_ptr<upf_graph> pfcp_associations::select_up_node(
    const SmPolicyDecision& decision,
    const upf_selection_criteria& selection_criteria) {
  return associations_graph.select_upf_nodes(decision, selection_criteria);
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
  UpfInfo info;
  InterfaceUpfInfoItem n3;
  InterfaceUpfInfoItem n6;
  UPInterfaceType n3_type;
  n3_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3);
  UPInterfaceType n6_type;
  n6_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6);
  n3.setInterfaceType(n3_type);
  n6.setInterfaceType(n6_type);

  info.setInterfaceUpfInfoList(std::vector<InterfaceUpfInfoItem>{n3, n6});

  // we use the default UPF
  auto upf_cfg = oai::config::smf::DEFAULT_UPF;
  std::string host;

  switch (node_id.node_id_type) {
    case pfcp::NODE_ID_TYPE_FQDN:
      host = node_id.fqdn;
      break;
    case pfcp::NODE_ID_TYPE_IPV4_ADDRESS:
      host = oai::utils::conv::toString(node_id.u1.ipv4_address);
      break;
    case pfcp::NODE_ID_TYPE_IPV6_ADDRESS:
      host = oai::utils::conv::toString(node_id.u1.ipv6_address);
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
void upf_graph::insert_into_graph(
    const std::shared_ptr<pfcp_association>& association_to_add) {
  std::vector<std::shared_ptr<pfcp_association>> all_upfs;

  std::vector<std::shared_ptr<qos_upf_edge>> n9_src_edges;
  std::vector<std::shared_ptr<qos_upf_edge>> n9_dst_edges;

  // iterate through all interfaces and see if the FQDN/IPv4 addresses of
  // existing UPFs match

  auto n3_edges = qos_upf_edge::create_n3_edges(association_to_add);
  auto n6_edges = qos_upf_edge::create_n6_edges(association_to_add);
  for (const auto& n3_edge : n3_edges) {
    add_upf_graph_edge(association_to_add, n3_edge);
  }
  for (const auto& n6_edge : n6_edges) {
    add_upf_graph_edge(association_to_add, n6_edge);
  }

  // Find N9 interfaces
  std::shared_lock graph_lock(graph_mutex);
  for (const auto& [other_upf, edges] : adjacency_list) {
    if (other_upf == association_to_add) {
      continue;
    }

    auto n9_edges_src_dst =
        qos_upf_edge::create_n9_edges(association_to_add, other_upf);
    auto n9_edges_dst_src =
        qos_upf_edge::create_n9_edges(other_upf, association_to_add);
    if (n9_edges_src_dst.size() != n9_edges_dst_src.size()) {
      Logger::smf_app().warn(
          "Found potential UPF N9 interface edge between %s and %s, but it is "
          "not consistent. Please check your UpfInfo configuration. ",
          association_to_add->get_printable_name(),
          other_upf->get_printable_name());
    } else {
      // here we just put the same UPF again, so that we have multiple edges
      // (e.g. when we have redundant N9 edges)
      for (int i = 0; i < n9_edges_src_dst.size(); i++) {
        n9_src_edges.push_back(n9_edges_src_dst[i]);
        n9_dst_edges.push_back(n9_edges_dst_src[i]);
        all_upfs.push_back(other_upf);
      }
    }
  }
  graph_lock.unlock();

  for (int i = 0; i < n9_src_edges.size(); i++) {
    add_upf_graph_edge(
        association_to_add, all_upfs[i], n9_src_edges[i], n9_dst_edges[i]);
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
    qos_upf_edge to_delete_edge;

    // go through all the other nodes and remove this association from
    // adjacency list
    for (auto& [other_upf, edges] : adjacency_list) {
      auto edge_it = edges.begin();

      while (edge_it != edges.end()) {
        if (edge_it->get()->destination_upf == it->first) {
          edge_it = edges.erase(edge_it);
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
    const std::shared_ptr<qos_upf_edge>& edge_info_src_dst) {
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
    it_src->second.push_back(edge_info_src_dst);
    Logger::smf_app().debug(
        "Successfully added UPF graph edge for %s: %s",
        source->get_printable_name(), edge_info_src_dst->to_string(0).c_str());
  }
}

//------------------------------------------------------------------------------
void upf_graph::add_upf_graph_edge(
    const std::shared_ptr<pfcp_association>& source,
    const std::shared_ptr<pfcp_association>& dest,
    const std::shared_ptr<qos_upf_edge>& edge_info_src_dst,
    const std::shared_ptr<qos_upf_edge>& edge_info_dst_src) {
  add_upf_graph_edge(source, edge_info_src_dst);
  add_upf_graph_edge(dest, edge_info_dst_src);
}

//------------------------------------------------------------------------------
void upf_graph::add_upf_graph_node(
    const std::shared_ptr<pfcp_association>& node) {
  std::unique_lock lock_graph(graph_mutex);

  auto it = adjacency_list.find(node);

  if (it == adjacency_list.end()) {
    adjacency_list.try_emplace(node);

    Logger::smf_app().info(
        "Successfully added UPF node: %s", node->get_printable_name());
  }
}

//------------------------------------------------------------------------------
void upf_graph::print_graph() const {
  std::shared_lock lock_graph(graph_mutex);
  std::string output;

  if (adjacency_list.empty()) {
    Logger::smf_app().debug("UPF graph is empty");
    return;
  }

  std::string fmt_value = oai::config::get_value_formatter(0);
  std::string fmt_title = oai::config::get_title_formatter(1);

  for (const auto& it : adjacency_list) {
    output.append(
        fmt::format(fmt_value, "Node", it.first->get_printable_name()));
    output.append(fmt::format(fmt_title, "Edges"));

    for (const auto& edge : it.second) {
      output.append(edge->to_string(2));
    }
  }

  Logger::smf_app().debug("UPF graph");
  // TODO we should have a log_line function in common-src so that we split it
  // (similar like we do for the config)
  Logger::smf_app().debug("\n %s", output);
}

//------------------------------------------------------------------------------
bool upf_graph::dfs_next_upf(
    std::vector<std::shared_ptr<qos_upf_edge>>& info_dl,
    std::vector<std::shared_ptr<qos_upf_edge>>& info_ul,
    std::shared_ptr<pfcp_association>& upf) {
  // we need unique lock as visited array and stack is written
  std::unique_lock lock_graph(graph_mutex);

  UPInterfaceType n9_type;
  n9_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N9);

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
      return false;
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
        if (edge_it->destination_upf) {
          if (!visited_asynch[edge_it->destination_upf]) {
            if (edge_it->type == n9_type && edge_it->uplink) {
              unvisited_n9_node = true;
              Logger::smf_app().debug(
                  "UL CL scenario: We have a node with an unvisited N9_UL "
                  "node.");
              return false;
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
        if (edge_it->destination_upf) {
          if (!visited_asynch[edge_it->destination_upf]) {
            stack_asynch.push(edge_it->destination_upf);
          }
        }
      }
    }
    upf = node_it->first;

    // set correct edge infos
    for (auto& edge_it : node_it->second) {
      // set the correct edges to return
      if (edge_it->uplink) {
        info_ul.push_back(edge_it);
      } else {
        info_dl.push_back(edge_it);
      }
      current_upf_asynch      = upf;
      current_edges_dl_asynch = info_dl;
      current_edges_ul_asynch = info_ul;
    }
  }
  return true;
}

//------------------------------------------------------------------------------
void upf_graph::dfs_current_upf(
    std::vector<std::shared_ptr<qos_upf_edge>>& info_dl,
    std::vector<std::shared_ptr<qos_upf_edge>>& info_ul,
    std::shared_ptr<pfcp_association>& upf) {
  std::shared_lock graph_lock(graph_mutex);
  upf     = current_upf_asynch;
  info_dl = current_edges_dl_asynch;
  info_ul = current_edges_ul_asynch;
}

//------------------------------------------------------------------------------
void upf_graph::start_asynch_dfs_procedure(bool uplink) {
  std::unique_lock graph_lock(graph_mutex);
  if (!stack_asynch.empty()) {
    Logger::smf_app().error(
        "Started DFS procedure, but old stack is not empty. Failure");
  }
  // clear the stack and visited array
  stack_asynch   = {};
  visited_asynch = {};
  uplink_asynch  = uplink;

  UPInterfaceType n3_type;
  n3_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3);

  UPInterfaceType n6_type;
  n6_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6);

  // uplink start at the exit nodes, downlink start at access nodes, do not
  // actually do DFS but put them on the stack
  for (auto& it : adjacency_list) {
    for (auto& edge : it.second) {
      if ((uplink && edge->type == n6_type) ||
          (!uplink && edge->type == n3_type)) {
        stack_asynch.push(it.first);
        break;
      }
    }
  }
}

//---------------------------------------------------------------------------------------------
std::vector<std::shared_ptr<qos_upf_edge>> upf_graph::get_access_edges() const {
  std::shared_lock graph_lock(graph_mutex);
  UPInterfaceType n3_type;
  n3_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3);
  std::vector<std::shared_ptr<qos_upf_edge>> n3_edges;

  for (const auto& [node, edges] : adjacency_list) {
    for (const auto& edge : edges) {
      if (edge->type == n3_type) {
        n3_edges.emplace_back(edge);
      }
    }
  }

  if (n3_edges.empty()) {
    Logger::smf_app().error(
        "Getting Access edges from UPF graph failed. There are no exit edges");
    print_graph();
  }

  return n3_edges;
}

//---------------------------------------------------------------------------------------------
std::shared_ptr<upf_graph> upf_graph::select_upf_nodes(
    const upf_selection_criteria& base_criteria) {
  auto graph    = std::make_shared<upf_graph>();
  auto criteria = base_criteria;
  // this is the UPF selection without PCC rules so we always have only the
  // default QoS with only one QFI
  bool success = select_upf_nodes(criteria, graph, criteria);
  if (success) {
    return graph;
  } else {
    return nullptr;
  }
}

//------------------------------------------------------------------------------
// TODO in the current implementation, UL CL needs to be the first node,
// otherwise it is not explored anymore when graph is merged
std::shared_ptr<upf_graph> upf_graph::select_upf_nodes(
    const SmPolicyDecision& policy_decision,
    const upf_selection_criteria& base_criteria) {
  std::shared_lock graph_lock(graph_mutex);

  // TODO also allow for QoS rules only without traffic rules
  if (!policy_decision.pccRulesIsSet() ||
      !policy_decision.traffContDecsIsSet()) {
    Logger::smf_app().warn(
        "Cannot build UPF graph for PDU session when pcc rules or traffic "
        "control description is missing");
  }

  auto pcc_rules     = policy_decision.getPccRules();
  auto traffic_conts = policy_decision.getTraffContDecs();
  std::unordered_set<uint32_t> precedences;

  std::shared_ptr<upf_graph> sub_graph_ptr = {};
  upf_selection_criteria verify_criteria;

  // run DFS for each PCC rule, get different graphs and merge them

  upf_selection_criteria selection_criteria = base_criteria;
  for (const auto& rule : pcc_rules) {
    selection_criteria.dnais.clear();
    // TODO without Qos we only have one QFI, we should include the QOS data
    // from PCF here and generate a QFI for each of the QoS flows

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
            selection_criteria.dnais.insert(route.getDnai());
            verify_criteria.dnais.insert(route.getDnai());
          }
          if (traffic_it->second.redirectInfoIsSet()) {
            selection_criteria.redirect_information =
                traffic_it->second.getRedirectInfo();
          }
        } else {
          Logger::smf_app().info("Route to location is not set in PCC rules");
        }
      }
      // TODO at this point add and convert QoS information from PCC rules
      // TODO if we want the graph function to generate a new QFI, we have to
      // set qfi to 0 here

    } else {
      continue;
    }
    if (rule.second.flowInfosIsSet() && !rule.second.getFlowInfos().empty()) {
      auto flow_info = rule.second.getFlowInfos()[0];
      if (!flow_info.flowDirectionIsSet()) {
        Logger::smf_app().info(
            "Flow direction is not set in PCC rules, use default "
            "BIDIRECTIONAL");
        FlowDirectionRm flow_direction;
        flow_direction.setEnumValue(
            FlowDirection_anyOf::eFlowDirection_anyOf::BIDIRECTIONAL);
        flow_info.setFlowDirection(flow_direction);
      }
      // TODO 29.512 defines this as false per default, but we take the filter
      // here to build the NAS filter list we should take it from default QoS
      // from session rules, if we don't set it to true here, COTS UEs will
      // complain

      flow_info.setPacketFilterUsage(true);

      selection_criteria.flow_information = rule.second.getFlowInfos()[0];
    } else {
      Logger::smf_app().warn(
          "Flow Description is empty. Skip PCC rule %s", rule.first.c_str());
      continue;
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
    selection_criteria.precedence = precedence;

    select_upf_nodes(selection_criteria, sub_graph_ptr, verify_criteria);
  }

  // Now we verify the merged graph
  if (sub_graph_ptr && sub_graph_ptr->verify(verify_criteria)) {
    Logger::smf_app().info("Dynamic UPF selection successful.");
    sub_graph_ptr->print_graph();

  } else {
    Logger::smf_app().info("Dynamic UPF selection failed");
    sub_graph_ptr.reset();
  }

  return sub_graph_ptr;
}

bool upf_graph::select_upf_nodes(
    upf_selection_criteria& criteria, std::shared_ptr<upf_graph>& sub_graph_ptr,
    const upf_selection_criteria& verify_criteria) {
  std::shared_lock graph_lock(graph_mutex);

  std::unordered_map<
      std::shared_ptr<pfcp_association>, bool,
      std::hash<std::shared_ptr<pfcp_association>>>
      visited;

  UPInterfaceType n3_type;
  n3_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3);
  bool has_n3 = false;
  for (const auto& [upf, edges] : adjacency_list) {
    if (visited[upf]) {
      continue;
    }
    for (const auto& edge : edges) {
      if (edge->type == n3_type) {
        has_n3 = true;
        break;
      }
    }
    // we start the algorithm with any UPF that has an N3 interface
    if (has_n3) {
      // need to make a copy in case the algorithm adds nodes to the graph
      // and then the graph is wrong
      std::shared_ptr<upf_graph> sub_graph_copy_ptr;
      if (sub_graph_ptr) {
        sub_graph_copy_ptr = std::make_shared<upf_graph>(*sub_graph_ptr);
      } else {
        sub_graph_ptr = std::make_shared<upf_graph>();
      }
      if (criteria.qfi == 0) {
        criteria.qfi = sub_graph_ptr->generate_qfi();
      }

      create_subgraph_dfs(sub_graph_ptr, upf, visited, criteria);

      if (!sub_graph_ptr->verify(verify_criteria)) {
        // in case copy is empty, new subgraph_ptr is also empty, and we
        // create a new upf graph
        sub_graph_ptr = sub_graph_copy_ptr;
        criteria.qfi  = 0;
      } else {
        return true;
      }
    }
  }
  Logger::smf_app().warn("UPF selection failed");

  return false;
}

//---------------------------------------------------------------------------------------------
bool upf_graph::verify(const upf_selection_criteria& criteria) {
  if (total_edge_count != associated_edge_count) {
    Logger::smf_app().info(
        "UPF graph selection could not associate links between all UPFs");
    return false;
  }

  if (access_edge_count == 0) {
    Logger::smf_app().info("UPF graph does not have an access (N3) node");
    return false;
  }

  if (exit_edge_count == 0) {
    Logger::smf_app().info("UPF graph does not have an exit (N6) node");
    return false;
  }

  // special case, here we allow one DNAI less
  if (adjacency_list.size() == 1 &&
      served_dnais.size() == criteria.dnais.size() - 1) {
    Logger::smf_app().debug(
        "Found UPF graph that serves DNAIs %s", get_dnai_list(served_dnais));
    return true;
  }

  if (served_dnais.size() != criteria.dnais.size()) {
    Logger::smf_app().debug(
        "Found UPF graph that serves DNAIs %s, but not all DNAIs from rule "
        "are covered (%s)",
        get_dnai_list(served_dnais), get_dnai_list(criteria.dnais));
    print_graph();
    return false;
  }
  if (!served_dnais.empty()) {
    Logger::smf_app().debug(
        "Found UPF graph that serves DNAIs %s", get_dnai_list(served_dnais));
  }
  Logger::smf_app().info("UPF selection was successful.");
  print_graph();
  return true;
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

void upf_graph::update_next_hop_fteid(
    const std::shared_ptr<qos_upf_edge>& src_edge, const pfcp::fteid_t& fteid) {
  std::shared_lock graph_lock(graph_mutex);
  if (!src_edge->destination_upf) {
    return;
  }
  auto node_it = adjacency_list.find(src_edge->destination_upf);
  if (node_it == adjacency_list.end()) {
    Logger::smf_app().error(
        "Update F-TEID: node ID does not exist in UPF graph, this should not "
        "happen");
    return;
  }
  for (const auto& edge_it : node_it->second) {
    if (edge_it->destination_upf &&
        edge_it->destination_upf == src_edge->source_upf) {
      edge_it->next_hop_fteid = fteid;
    }
  }
}

//---------------------------------------------------------------------------------------------
void upf_graph::create_subgraph_dfs(
    std::shared_ptr<upf_graph>& sub_graph,
    const std::shared_ptr<pfcp_association>& start_node,
    std::unordered_map<
        std::shared_ptr<pfcp_association>, bool,
        std::hash<std::shared_ptr<pfcp_association>>>& visited,
    const upf_selection_criteria& selection_criteria) {
  std::stack<std::shared_ptr<pfcp_association>> stack;
  stack.push(start_node);

  // if DNAIs are empty, we only search for the first N6 exit
  bool simple_mode = selection_criteria.dnais.empty();

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
    Logger::smf_app().debug(
        "UPF selection DFS: Handle UPF %s", node->get_printable_name());

    // DFS: Go through all edges and check if the UPF serves one of the DNAIs
    // from the PCC rule
    std::vector<std::shared_ptr<qos_upf_edge>> edges_to_connect;
    unsigned int uplink_next_hop_edges = 0;
    bool push_next_upf                 = false;
    // NOTE: As we use shared_ptrs here, we have to make a copy before changing
    // the object otherwise we change the edges also in the full graph and/or
    // for other PDU sessions
    for (const auto& edge_it : node_it->second) {
      auto edge_to_use = std::make_shared<qos_upf_edge>(*edge_it);
      if (!edge_to_use->serves_network(selection_criteria)) {
        continue;  // do not consider this edge, does not serve DNN or SNSSAI or
                   // any DNAI
      }
      if (!edge_to_use->used_dnai.empty()) {
        sub_graph->served_dnais.insert(edge_to_use->used_dnai);
      }
      UPInterfaceType n3_type;
      n3_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3);
      UPInterfaceType n6_type;
      n6_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6);
      UPInterfaceType n9_type;
      n9_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N9);

      if (edge_to_use->type == n3_type) {
        if (simple_mode && sub_graph->access_edge_count == 1) {
          continue;
        }
        sub_graph->access_edge_count++;
        edge_to_use->uplink = false;
      } else if (edge_to_use->type == n6_type) {
        edge_to_use->uplink = true;
        sub_graph->exit_edge_count++;
        uplink_next_hop_edges++;
      } else if (edge_to_use->type == n9_type) {
        // if next hop is not visited, it is uplink
        if (edge_to_use->destination_upf &&
            !visited[edge_to_use->destination_upf]) {
          uplink_next_hop_edges++;
          edge_to_use->uplink = true;
          push_next_upf       = true;
        }
      }

      if (simple_mode && uplink_next_hop_edges > 1) {
        Logger::smf_app().debug(
            "UPF graph already has one UL edge and DNAIs are not supported, "
            "skipping edge \n %s",
            edge_to_use->to_string(0));
        continue;
      }

      edges_to_connect.push_back(edge_to_use);
      sub_graph->add_upf_graph_edge(node_it->first, edge_to_use);
      sub_graph->total_edge_count++;

      // continue DFS with next UPF node if not visited
      if (push_next_upf) {
        stack.push(edge_to_use->destination_upf);
      }
    }

    // if we have a clear association between N3 and N6, N3 and N9, N9 and N9 or
    // N9 and N6, we connect the edges
    if (edges_to_connect.size() == 2) {
      edges_to_connect[0]->associated_edge = edges_to_connect[1];
      edges_to_connect[1]->associated_edge = edges_to_connect[0];
      sub_graph->associated_edge_count += 2;
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

  for (const auto& [upf, edges] : adjacency_list) {
    for (const auto& edge : edges) {
      UPInterfaceType n3_type;
      n3_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3);

      if (edge->type == n3_type) {
        return to_string_from_start_node(indent, upf);
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

  UPInterfaceType n6_type;
  n6_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6);

  while (!queue.empty()) {
    auto node_queue = queue.front();
    node_it         = adjacency_list.find(node_queue);
    if (node_it == adjacency_list.end()) continue;

    queue.pop_front();

    for (const auto& edge : node_it->second) {
      // skip N6 output
      if (edge->type == n6_type) continue;

      std::string iface = edge->type.getEnumString();
      if (!edge->nw_instance.empty()) {
        iface.append(": ").append(edge->nw_instance);
      }
      output_per_iface_type[iface].append(edge->to_string(0));
    }
  }

  for (const auto& o : output_per_iface_type) {
    output.append(indent).append(o.first).append(":\n");
    output.append(o.second);
  }

  return output;
}

uint8_t upf_graph::generate_qfi() {
  return qfi_generator.get_uid();
}

void upf_graph::release_qfi(uint8_t qfi) {
  qfi_generator.free_uid(qfi);
}
