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

/*! \file smf_pfcp_association.hpp
 \author  Lionel GAUTHIER
 \date 2019
 \email: lionel.gauthier@eurecom.fr
 */

#ifndef FILE_SMF_PFCP_ASSOCIATION_HPP_SEEN
#define FILE_SMF_PFCP_ASSOCIATION_HPP_SEEN

#include <map>
#include <mutex>
#include <utility>
#include <vector>
#include <stack>
#include <unordered_map>
#include <unordered_set>

#include "3gpp_29.244.h"
#include "itti.hpp"
#include "smf_profile.hpp"
#include "SmPolicyDecision.h"
#include "3gpp_24.007.h"
#include "UpfInfo.h"
#include "smf.h"
#include "smf_config_types.hpp"
#include "smf_qos_upf_edge.hpp"
#include "uint_generator.hpp"

namespace smf {

const int PFCP_ASSOCIATION_HEARTBEAT_INTERVAL_SEC  = 10;
const int PFCP_ASSOCIATION_HEARTBEAT_MAX_RETRIES   = 2;
const int PFCP_ASSOCIATION_GRACEFUL_RELEASE_PERIOD = 5;

const int PFCP_MAX_ASSOCIATIONS = 16;

class pfcp_association {
 public:
  pfcp::node_id_t node_id;
  std::size_t hash_node_id;
  pfcp::recovery_time_stamp_t recovery_time_stamp;
  std::pair<bool, pfcp::up_function_features_s> function_features;
  //
  mutable std::mutex m_sessions;
  std::set<pfcp::fseid_t> sessions;
  //
  timer_id_t timer_heartbeat      = ITTI_INVALID_TIMER_ID;
  int num_retries_timer_heartbeat = 0;
  uint64_t trxn_id_heartbeat      = 0;

  bool is_restore_sessions_pending = false;

  timer_id_t timer_association      = ITTI_INVALID_TIMER_ID;
  timer_id_t timer_graceful_release = ITTI_INVALID_TIMER_ID;

  explicit pfcp_association(oai::config::smf::upf upf_cfg)
      : recovery_time_stamp(), m_upf_cfg(std::move(upf_cfg)) {
    node_id      = m_upf_cfg.get_node_id();
    hash_node_id = std::hash<pfcp::node_id_t>{}(node_id);
  }

  pfcp_association(
      oai::config::smf::upf upf_cfg,
      const pfcp::recovery_time_stamp_t& recovery_time_stamp)
      : recovery_time_stamp(recovery_time_stamp),
        m_upf_cfg(std::move(upf_cfg)) {
    node_id      = m_upf_cfg.get_node_id();
    hash_node_id = std::hash<pfcp::node_id_t>{}(node_id);
  }
  pfcp_association(
      oai::config::smf::upf upf_cfg, const pfcp::recovery_time_stamp_t& rts,
      const pfcp::up_function_features_s& uff)
      : recovery_time_stamp(rts), m_upf_cfg(std::move(upf_cfg)) {
    node_id                  = m_upf_cfg.get_node_id();
    hash_node_id             = std::hash<pfcp::node_id_t>{}(node_id);
    function_features.first  = true;
    function_features.second = uff;
  }

  void notify_add_session(const pfcp::fseid_t& cp_fseid);
  bool has_session(const pfcp::fseid_t& cp_fseid);
  void notify_del_session(const pfcp::fseid_t& cp_fseid);
  void del_sessions();
  void restore_n4_sessions();
  void set(const pfcp::up_function_features_s& ff) {
    function_features.first  = true;
    function_features.second = ff;
  };

  [[nodiscard]] oai::model::nrf::UpfInfo get_upf_info() const;

  size_t operator()(const pfcp_association&) const { return hash_node_id; }

  /**
   * @brief Get the readable name of the UPF associated with this association
   * @return string representing the name of the UPF associated with this
   * association
   */
  std::string get_printable_name() const;

  /*
   * Print related-information for this association
   * @param void
   * @return void:
   */
  void display();

  const oai::config::smf::upf& get_upf_config() const;

  bool serves_network(
      const oai::model::common::Snssai& snssai, const std::string& dnn) const;

 private:
  oai::config::smf::upf m_upf_cfg;
};

class qos_upf_edge;
class upf_selection_criteria;

class upf_graph {
 private:
  // Adjacency List representation of UPF graph, Index is the hashed value
  // of a PFCP association, then we have a list of edges
  // use std::hash of shared_ptr so that hash function of pfcp_association is
  // called
  std::unordered_map<
      std::shared_ptr<pfcp_association>,
      std::vector<std::shared_ptr<qos_upf_edge>>,
      std::hash<std::shared_ptr<pfcp_association>>>
      adjacency_list;

  mutable std::shared_mutex graph_mutex;

  // State information for asynchronous DFS during PFCP procedures
  std::stack<std::shared_ptr<pfcp_association>> stack_asynch;
  std::unordered_map<
      std::shared_ptr<pfcp_association>, bool,
      std::hash<std::shared_ptr<pfcp_association>>>
      visited_asynch;
  std::vector<std::shared_ptr<qos_upf_edge>> current_edges_ul_asynch;
  std::vector<std::shared_ptr<qos_upf_edge>> current_edges_dl_asynch;
  std::shared_ptr<pfcp_association> current_upf_asynch;
  bool uplink_asynch = false;

  // statistics which are useful for verifying the graph
  unsigned int access_edge_count     = 0;
  unsigned int exit_edge_count       = 0;
  unsigned int total_edge_count      = 0;
  unsigned int associated_edge_count = 0;
  std::unordered_set<std::string> served_dnais;

  /**
   * @brief Adds an edge in one direction, adds node if it does not exist
   * @param source
   * @param edge_info_src_dst
   */
  void add_upf_graph_edge(
      const std::shared_ptr<pfcp_association>& source,
      const std::shared_ptr<qos_upf_edge>& edge_info_src_dst);

  /**
   * @brief Adds an edge to the graph in both direction, adds node if it does
   * not exist
   *
   * @param source
   * @param dest
   * @param edge_info_src_dest
   * @param edge_info_dst_src
   */
  void add_upf_graph_edge(
      const std::shared_ptr<pfcp_association>& source,
      const std::shared_ptr<pfcp_association>& dest,
      const std::shared_ptr<qos_upf_edge>& edge_info_src_dest,
      const std::shared_ptr<qos_upf_edge>& edge_info_dst_src);

  /**
   * @brief Adds a UPF graph node
   *
   * @param node
   */
  void add_upf_graph_node(const std::shared_ptr<pfcp_association>& node);

  /**
   * @brief Creates a subgraph based on all the UPFs in the current graph which
   * serve the DNAIs from all_dnais. Only UPFs are considered that serve the
   * SNSSAI and DNN
   * @pre Not thread-safe, lock before AND call set_dfs_selection_criteria
   *
   * @param sub_graph Existing graph. may already contain nodes
   * @param start_node Node to start the DFS search
   * @param visited visited map for DFS
   */
  void create_subgraph_dfs(
      std::shared_ptr<upf_graph>& sub_graph,
      const std::shared_ptr<pfcp_association>& start_node,
      std::unordered_map<
          std::shared_ptr<pfcp_association>, bool,
          std::hash<std::shared_ptr<pfcp_association>>>& visited,
      const upf_selection_criteria& selection_criteria);

  /**
   * @brief: Verifies this graph based on the selection criteria
   * @return true if graph is correct, false if not
   */
  bool verify(const upf_selection_criteria& selection_criteria);

  /**
   * TODO
   * @param dnais
   * @return
   */
  static std::string get_dnai_list(
      const std::unordered_set<std::string>& dnais);

  /**
   * Traverse the graph in BFS from start and generate info
   * @param start Start node
   * @param indent Added in the beginning of each line
   * @return
   */
  std::string to_string_from_start_node(
      const std::string& indent,
      const std::shared_ptr<pfcp_association>& start) const;

  uint8_t generate_qfi();

  void release_qfi(uint8_t qfi);

  oai::utils::uint_generator<uint8_t> qfi_generator;

 public:
  upf_graph() : adjacency_list(), visited_asynch(){};

  upf_graph(const upf_graph& g) {
    // TODO do I need to lock the other graph here?
    adjacency_list = g.adjacency_list;
    visited_asynch = g.visited_asynch;
    // do not copy mutex
  }

  /**
   * @brief Inserts a PFCP association into the UPF graph, based on the UPF
   * interface list and adds edges with other existing associations
   *
   * @param association_to_add pointer to the pfcp_association, cannot be null
   */
  void insert_into_graph(
      const std::shared_ptr<pfcp_association>& association_to_add);

  /**
   * @brief Get Association from an UPF node id hash
   * @param association_hash
   * @return shared_ptr to an association, may be null
   */
  std::shared_ptr<pfcp_association> get_association(
      std::size_t association_hash) const;

  /**
   * @brief Get Association from CP_FSEID
   * @param cp_fseid
   * @return shared_ptr to an association, may be null
   */
  std::shared_ptr<pfcp_association> get_association(
      const pfcp::fseid_t& cp_fseid) const;

  // cannot overload size_t and uint64_t
  /**
   * @brief Get Association from TRXN_ID
   * @param trxn_id
   * @return shared_ptr to an association, may be null
   */
  std::shared_ptr<pfcp_association> get_association_for_trxn_id(
      uint64_t trxn_id) const;
  /**
   * @brief Remove association from graph
   * @param association_hash
   * @return
   */
  bool remove_association(const std::shared_ptr<pfcp_association>& association);

  /**
   * @brief select one UPF node based on SNSSAI and DNAI. Returns a nullpointer
   * if no suitable UPF is found.
   *
   * @param criteria  DNN, SNSSAI and QoS Profile needs to be set
   * @return std::shared_ptr<upf_graph>
   */
  std::shared_ptr<upf_graph> select_upf_nodes(
      const upf_selection_criteria& criteria);

  /**
   *  @brief Select UPF nodes based on the policy decision and especially on the
   * PCC rules and the traffic descriptions from within. Tries to build a graph
   * where all the DNAIs from the traffic descriptions are contained. In case no
   * graph can be found, an empty pointer is returned
   *
   * @param policy_decision
   * @param criteria DNN, SNSSAI must be set, the rest is inferred from PCC
   * rules
   * @return std::shared_ptr<upf_graph>
   */
  std::shared_ptr<upf_graph> select_upf_nodes(
      const oai::model::pcf::SmPolicyDecision& policy_decision,
      const upf_selection_criteria& criteria);

  /**
   * @brief Select UPF nodes based on the selection criteria. This method is
   * called by the other overloaded select_upf_nodes functions and is
   * responsible for traversing the graph with a DFS. In case no graph can be
   * found, an empty pointer is returned.
   * @param criteria
   * @param verify_criteria   Use different criteria to verify the graph (only
   * relevant for DNAIs)
   * @param sub_graph  Existing graph. may already contain nodes
   * @return bool  true if a UPF graph can be selected
   */
  bool select_upf_nodes(
      upf_selection_criteria& criteria, std::shared_ptr<upf_graph>& sub_graph,
      const upf_selection_criteria& verify_criteria);

  /**
   * @brief Traverses the UPF asynch DFS procedure and returns the next UPF.
   * `start_asynch_dfs_procedure` needs to be started first!
   *
   * @param info_dl output_parameter: dl edge
   * @param info_ul output_parameter: ul edge
   * @param upf output_parameter: UPF
   * @return true in case the UPF selection was successfull, false e.g. if there
   * are unvisited UL UPFs
   */
  bool dfs_next_upf(
      std::vector<std::shared_ptr<qos_upf_edge>>& info_dl,
      std::vector<std::shared_ptr<qos_upf_edge>>& info_ul,
      std::shared_ptr<pfcp_association>& upf);

  /**
   * @brief Gives the information from the UPF which has previously been
   * returned by the call to 'dfs_next_upf'
   *
   * @param info_dl output_parameter: dl edge
   * @param info_ul output_parameter: ul edge
   * @param upf output_parameter: UPF
   */
  void dfs_current_upf(
      std::vector<std::shared_ptr<qos_upf_edge>>& info_dl,
      std::vector<std::shared_ptr<qos_upf_edge>>& info_ul,
      std::shared_ptr<pfcp_association>& upf);

  /**
   * @brief Starts asynchronous DFS procedure,
   * @param uplink if uplink or downlink direction
   */
  void start_asynch_dfs_procedure(bool uplink);

  /**
   * Get all the access edges for this graph, for each QFI there may be an
   * access edge
   * @return
   */
  std::vector<std::shared_ptr<qos_upf_edge>> get_access_edges() const;

  /**
   * @brief: Debug-prints the current graph
   */
  void print_graph() const;
  /**
   * Returns true if graph exceeds MAX_PFCP_ASSOCIATIONS
   * @return
   */
  bool full() const;

  /**
   * Traverses the graph in BFS starting at the N3 interface and returns graph
   * information: For each QFI: UL/DL FTEID for each edge
   * @param indent Added in the beginning of each line
   * @return string representation of this graph
   *
   */
  [[nodiscard]] std::string to_string(const std::string& indent) const;

  /**
   * Updates the next hop F-TEID for the next hope from the src_edge
   * @param src_edge
   * @param fteid
   */
  void update_next_hop_fteid(
      const std::shared_ptr<qos_upf_edge>& src_edge,
      const pfcp::fteid_t& fteid);
};

class pfcp_associations {
 private:
  std::vector<std::shared_ptr<pfcp_association>> pending_associations;
  mutable std::shared_mutex m_mutex;

  upf_graph associations_graph;

  pfcp_associations() : pending_associations(), associations_graph(){};

  void trigger_heartbeat_request_procedure(
      std::shared_ptr<pfcp_association>& s);

  std::shared_ptr<pfcp_association> check_association_on_add(
      const pfcp::node_id_t& node_id,
      const pfcp::recovery_time_stamp_t& recovery_time_stamp,
      bool& restore_n4_sessions, bool use_function_features,
      const pfcp::up_function_features_s& function_features);

  bool resolve_upf_hostname(pfcp::node_id_t& node_id);

  /**
   * Finds UPF configuration from pending associations (for SMF-initiated PFCP
   * associations) or from config (for UPF-initiated associations). In case
   * nothing can be found, the default configuration is taken
   * @param node_id
   * @return
   */
  oai::config::smf::upf get_upf_config(const pfcp::node_id_t& node_id) const;

  bool add_association(
      pfcp::node_id_t& node_id,
      const pfcp::recovery_time_stamp_t& recovery_time_stamp,
      bool& restore_n4_sessions,
      const pfcp::up_function_features_s& function_feature,
      const pfcp::enterprise_specific_s& enterprise_specific,
      bool use_function_features = false, bool use_enterprise_specific = false);

 public:
  static pfcp_associations& get_instance() {
    static pfcp_associations instance;
    return instance;
  }

  pfcp_associations(pfcp_associations const&) = delete;
  void operator=(pfcp_associations const&) = delete;

  bool add_association(
      pfcp::node_id_t& node_id,
      const pfcp::recovery_time_stamp_t& recovery_time_stamp,
      bool& restore_n4_sessions);
  bool add_association(
      pfcp::node_id_t& node_id,
      const pfcp::recovery_time_stamp_t& recovery_time_stamp,
      const pfcp::up_function_features_s& function_features,
      bool& restore_n4_sessions);
  bool add_association(
      pfcp::node_id_t& node_id,
      const pfcp::recovery_time_stamp_t& recovery_time_stamp,
      const pfcp::up_function_features_s& function_features,
      const pfcp::enterprise_specific_s& enterprise_specific,
      bool& restore_n4_sessions);
  bool update_association(
      pfcp::node_id_t& node_id,
      pfcp::up_function_features_s& function_features);
  bool get_association(
      const pfcp::node_id_t& node_id, std::shared_ptr<pfcp_association>& sa);
  bool get_association(
      const pfcp::fseid_t& cp_fseid,
      std::shared_ptr<pfcp_association>& sa) const;

  void notify_add_session(
      const pfcp::node_id_t& node_id, const pfcp::fseid_t& cp_fseid);
  void notify_del_session(const pfcp::fseid_t& cp_fseid);

  void restore_n4_sessions(const pfcp::node_id_t& node_id);

  void initiate_heartbeat_request(timer_id_t timer_id, uint64_t arg2_user);
  void timeout_heartbeat_request(timer_id_t timer_id, uint64_t arg2_user);
  void timeout_release_request(timer_id_t timer_id, uint64_t arg2_user);
  void handle_receive_heartbeat_response(const uint64_t trxn_id);

  std::shared_ptr<upf_graph> select_up_node(
      const upf_selection_criteria& selection_criteria);

  std::shared_ptr<upf_graph> select_up_node(
      const oai::model::pcf::SmPolicyDecision& decision,
      const upf_selection_criteria& selection_criteria);

  bool add_peer_candidate_node(const oai::config::smf::upf& upf_cfg);
  bool remove_association(const std::string& node_instance_id);
  bool remove_association(const int32_t& hash_node_id);
};

}  // namespace smf

#endif /* FILE_SMF_PFCP_ASSOCIATION_HPP_SEEN */
