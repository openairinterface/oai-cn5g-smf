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

/*! \file smf_procedure.cpp
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2019
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr
 */

#include "smf_procedure.hpp"

#include <algorithm>  // std::search
#include <utility>

#include "3gpp_29.244.h"
#include "3gpp_29.500.h"
#include "3gpp_29.502.h"
#include "3gpp_conversions.hpp"
#include "common_defs.h"
#include "conversions.hpp"
#include "itti.hpp"
#include "itti_msg_n4_restore.hpp"
#include "logger.hpp"
#include "smf_app.hpp"
#include "smf_config.hpp"
#include "smf_context.hpp"
#include "smf_pfcp_association.hpp"
#include "ProblemDetails.h"
#include "3gpp_24.501.h"

using namespace pfcp;
using namespace smf;
using namespace std;
using namespace oai::model::nrf;
using namespace oai::model::pcf;

extern itti_mw* itti_inst;
extern smf::smf_app* smf_app_inst;
extern std::unique_ptr<oai::config::smf::smf_config> smf_cfg;

std::string smf_session_procedure::to_string_fteid(const pfcp::fteid_t& fteid) {
  return fmt::format(
      "F-TEID ID 0x{:X} - IP: {}", fteid.teid,
      oai::utils::conv::toString(fteid.ipv4_address));
}

pfcp::ue_ip_address_t smf_session_procedure::pfcp_ue_ip_address(
    const std::shared_ptr<qos_upf_edge>& edge) {
  // only used in PDR,so when it is a downlink edge, we are in UL procedure
  pfcp::ue_ip_address_t ue_ip;
  if (edge->uplink) {
    ue_ip.sd = 1;
  } else {
    ue_ip.sd = 0;
  }
  if (sps->ipv4) {
    ue_ip.v4           = 1;
    ue_ip.ipv4_address = sps->ipv4_address;
  }
  // TODO malformed PFCP message, should be fixed in PFCP layer, but we dont
  // need it for now
  if (sps->ipv6) {
    ue_ip.v6 = 0;
    // ue_ip.ipv6_address = sps->ipv6_address;
  }
  return ue_ip;
}

//------------------------------------------------------------------------------
pfcp::fteid_t smf_session_procedure::pfcp_prepare_fteid(
    pfcp::fteid_t& fteid, const bool& ftup_supported,
    const oai::config::smf::upf& cfg) {
  pfcp::fteid_t local_fteid;
  if (!ftup_supported) {
    Logger::smf_app().info(
        "Generating N3-UL TEID since current UPF does not support TEID "
        "Creation");
    local_fteid.ch   = 0;
    local_fteid.v4   = 1;
    local_fteid.chid = 0;
    if (cfg.get_local_n3_ip().empty()) {
      Logger::smf_app().warn(
          "The UPF %s does not support F-TEID creation, but you did not "
          "configure the N3 host IP. We will try with the UPF hostname",
          cfg.get_host());
      local_fteid.ipv4_address = cfg.get_node_id().u1.ipv4_address;
    } else {
      local_fteid.ipv4_address =
          oai::utils::conv::fromString(cfg.get_local_n3_ip());
    }
    // TODO upon session release, we have to free this F-TEID again
    local_fteid.teid = smf_app_inst->generate_teid();
    fteid            = local_fteid;
    Logger::smf_app().info(
        "    UL F-TEID 0x%" PRIx32 " allocated for N3 IPv4 Addr : %s",
        local_fteid.teid,
        oai::utils::conv::toString(local_fteid.ipv4_address).c_str());
  } else if (fteid.is_zero()) {
    local_fteid.ch   = 1;
    local_fteid.v4   = 1;
    local_fteid.chid = 1;
    // same choose ID, indicates that same TEID should be generated for
    // more than one PDR
    local_fteid.choose_id = 42;
  } else {
    local_fteid = fteid;
  }
  return local_fteid;
}

//------------------------------------------------------------------------------
pfcp::create_far smf_session_procedure::pfcp_create_far(
    const std::shared_ptr<qos_upf_edge>& edge) {
  // When we have a FAR and edge is uplink we know we are in an uplink procedure
  //  e.g. FAR from N3 to N6, N6 is uplink edge -> uplink procedure

  oai::config::smf::upf cfg         = edge->source_upf->get_upf_config();
  pfcp::create_far create_far       = {};
  pfcp::apply_action_t apply_action = {};
  pfcp::forwarding_parameters forwarding_parameters   = {};
  pfcp::outer_header_creation_t outer_header_creation = {};

  // forwarding_parameters IEs
  pfcp::destination_interface_t destination_interface = {};

  apply_action.forw = 1;  // forward the packets

  if (edge->far_id.far_id == 0) {
    edge->far_id = sps->get_session_handler()->generate_far_id();
  }

  // ACCESS is for downlink, CORE for uplink
  if (edge->uplink) {
    destination_interface.interface_value = pfcp::INTERFACE_VALUE_CORE;
  } else {
    destination_interface.interface_value = pfcp::INTERFACE_VALUE_ACCESS;

    if (cfg.enable_dl_pdr_in_session_establishment()) {
      apply_action.forw = 0;
      apply_action.drop = 1;
      create_far.set(edge->far_id);
      create_far.set(apply_action);
      return create_far;
    }
  }

  forwarding_parameters.set(destination_interface);

  //-------------------
  // Network Instance
  //-------------------
  if (!edge->nw_instance.empty()) {
    pfcp::network_instance_t network_instance = {};
    network_instance.network_instance         = edge->nw_instance;
    forwarding_parameters.set(network_instance);
  }
  // we only support URL type redirect information for now
  if (edge->uplink && edge->redirect_information.isRedirectEnabled() &&
      edge->redirect_information.getRedirectAddressType().getEnumValue() ==
          oai::model::pcf::RedirectAddressType_anyOf::
              eRedirectAddressType_anyOf::URL) {
    forwarding_parameters.set(edge->get_pfcp_redirect_information());
  }
  UPInterfaceType n6_type;
  n6_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6);

  if (pfcp_outer_header_creation(edge, outer_header_creation)) {
    forwarding_parameters.set(outer_header_creation);
  }

  create_far.set(edge->far_id);
  create_far.set(apply_action);
  create_far.set(
      forwarding_parameters);  // should check since destination
                               // interface is directly set to FAR (as
                               // described in Table 5.8.2.11.6-1)
  return create_far;
}

//------------------------------------------------------------------------------
pfcp::create_pdr smf_session_procedure::pfcp_create_pdr(
    const std::shared_ptr<qos_upf_edge>& edge) {
  // When we have a PDR and edge is uplink we know we are in a downlink
  // procedure, e.g. PDR from N6 to N3 -> N6 is uplink edge, so downlink
  // procedure

  oai::config::smf::upf cfg = edge->source_upf->get_upf_config();
  pfcp::up_function_features_s up_features =
      edge->source_upf->function_features.second;
  //-------------------
  // IE create_pdr (section 5.8.2.11.3@TS 23.501)
  //-------------------
  pfcp::create_pdr create_pdr   = {};
  pfcp::precedence_t precedence = {};
  pfcp::pdi pdi                 = {};  // packet detection information
  pfcp::outer_header_removal_t outer_header_removal = {};
  // pdi IEs
  pfcp::source_interface_t source_interface          = {};
  pfcp::fteid_t local_fteid                          = {};
  pfcp::sdf_filter_t sdf_filter                      = {};
  pfcp::application_id_t application_id              = {};
  pfcp::_3gpp_interface_type_t source_interface_type = {};

  // Packet detection information (see Table 7.5.2.2-2: PDI IE within PFCP
  // Session Establishment Request, 3GPP TS 29.244 V16.0.0)  source interface

  if (edge->pdr_id.rule_id == 0) {
    edge->pdr_id = sps->get_session_handler()->generate_pdr_id();
  }
  create_pdr.set(edge->pdr_id);

  if (edge->uplink) {
    source_interface.interface_value = pfcp::INTERFACE_VALUE_CORE;
  } else {
    source_interface.interface_value = pfcp::INTERFACE_VALUE_ACCESS;
  }

  Logger::smf_app().debug("Created PDR ID, rule ID %d", edge->pdr_id.rule_id);
  pdi.set(source_interface);

  //-------------------
  // Network Instance for Forward Action
  //-------------------
  if (!edge->nw_instance.empty()) {
    pfcp::network_instance_t network_instance = {};
    network_instance.network_instance         = edge->nw_instance;
    pdi.set(network_instance);
  }

  UPInterfaceType n6_type;
  n6_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6);

  UPInterfaceType n3_type;
  n3_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3);

  UPInterfaceType n9_type;
  n3_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N9);

  if (edge->type != n6_type) {
    local_fteid = pfcp_prepare_fteid(edge->fteid, up_features.ftup, cfg);
    // in UPLINK always choose ID
    if (edge->uplink) {
      local_fteid.chid = 0;
    } else {
      local_fteid = pfcp_prepare_fteid(edge->fteid, up_features.ftup, cfg);
    }
    pdi.set(local_fteid);
  }

  // UE IP address
  pdi.set(pfcp_ue_ip_address(edge));

  if (edge->type == n3_type) {
    source_interface_type.interface_type_value = pfcp::_3GPP_INTERFACE_TYPE_N3;
  } else if (edge->type == n9_type) {
    source_interface_type.interface_type_value = pfcp::_3GPP_INTERFACE_TYPE_N9;
  }
  // do not remove outer header in dl direction
  // also we dont add this information if we use DL PDR in session establishment
  // as we update it later anyway
  if (edge->type != n6_type && !cfg.enable_dl_pdr_in_session_establishment()) {
    outer_header_removal.outer_header_removal_description =
        OUTER_HEADER_REMOVAL_GTPU_UDP_IPV4;
    create_pdr.set(outer_header_removal);
    pdi.set(edge->qfi);  // QFI - QoS Flow ID
  }
  // TODO: Traffic Endpoint ID
  // TODO: Application ID
  // TODO: Ethernet PDU Session Information
  // TODO: Ethernet Packet Filter
  // TODO: Framed Route Information
  // TODO: Framed-Routing
  // TODO: Framed-IPv6-Route

  if (!edge->uplink && !edge->flow_information.getFlowDescription().empty()) {
    sdf_filter.fd               = 1;
    sdf_filter.flow_description = edge->flow_information.getFlowDescription();
    pdi.set(sdf_filter);
  }

  pdi.set(source_interface_type);

  // Here we take the precedence directly from the PCC rules. It should be okay
  // because both values are integer, but we might need to provide another
  // mapping
  precedence.precedence = edge->precedence;

  create_pdr.set(precedence);
  create_pdr.set(pdi);

  // we take the FAR ID of the associated edge, so either from the same QFI or
  // from the same path for UL CL
  create_pdr.set(edge->associated_edge->far_id);

  if (cfg.enable_usage_reporting()) {
    create_pdr.set(edge->urr_id);
  }

  return create_pdr;
}

//------------------------------------------------------------------------------
pfcp::create_urr smf_session_procedure::pfcp_create_urr(
    const std::shared_ptr<qos_upf_edge>& edge) {
  if (edge->urr_id.urr_id == 0) {
    edge->urr_id = sps->get_session_handler()->generate_urr_id();
  }
  pfcp::create_urr create_urr                   = {};
  pfcp::measurement_method_t measurement_method = {};
  pfcp::measurement_period_t measurement_Period = {};
  pfcp::reporting_triggers_t reporting_triggers = {};
  pfcp::volume_threshold_t volume_threshold     = {};
  pfcp::time_threshold_t time_threshold         = {};

  // Hardcoded values for the moment
  measurement_method.volum              = 1;  // Volume based usage report
  measurement_method.durat              = 1;
  measurement_Period.measurement_period = 10;  // Every 10 Sec
  reporting_triggers.perio              = 1;   // Periodic usage report
  reporting_triggers.volth              = 1;
  reporting_triggers.timth              = 1;
  reporting_triggers.volqu              = 0;
  reporting_triggers.timqu              = 0;

  volume_threshold.dlvol           = 1;
  volume_threshold.ulvol           = 0;
  volume_threshold.tovol           = 0;
  volume_threshold.downlink_volume = 1000;

  time_threshold.time_threshold = 5;

  create_urr.set(edge->urr_id);
  create_urr.set(measurement_method);
  create_urr.set(measurement_Period);
  create_urr.set(reporting_triggers);
  create_urr.set(time_threshold);
  create_urr.set(volume_threshold);

  return create_urr;
}

//------------------------------------------------------------------------------
pfcp::remove_pdr smf_session_procedure::pfcp_remove_pdr(
    const shared_ptr<qos_upf_edge>& edge) {
  pfcp::remove_pdr remove_pdr;
  remove_pdr.set(edge->pdr_id);
  return remove_pdr;
}

//------------------------------------------------------------------------------
pfcp::remove_far smf_session_procedure::pfcp_remove_far(
    const shared_ptr<qos_upf_edge>& edge) {
  pfcp::remove_far remove_far;
  remove_far.set(edge->far_id);

  return remove_far;
}

pfcp::update_pdr smf_session_procedure::pfcp_update_pdr(
    const shared_ptr<qos_upf_edge>& edge) {
  // TODO some duplicated code from create_pdr

  oai::config::smf::upf cfg = edge->source_upf->get_upf_config();

  pfcp::update_pdr update_pdr                       = {};
  pfcp::precedence_t precedence                     = {};
  pfcp::pdi pdi                                     = {};
  pfcp::source_interface_t source_interface         = {};
  pfcp::outer_header_removal_t outer_header_removal = {};

  // UE IP address
  pdi.set(pfcp_ue_ip_address(edge));

  precedence.precedence = edge->precedence;
  // TODO this is now only in DL direction
  source_interface.interface_value = pfcp::INTERFACE_VALUE_CORE;
  if (!edge->nw_instance.empty()) {
    // mandatory for travelping
    pfcp::network_instance_t network_instance = {};
    network_instance.network_instance         = edge->nw_instance;
    pdi.set(network_instance);
  }

  pdi.set(source_interface);

  if (cfg.enable_usage_reporting()) {
    pfcp::urr_id_t urr_id = edge->urr_id;
    update_pdr.set(urr_id);
  }

  UPInterfaceType n6_type;
  n6_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6);

  if (edge->type != n6_type) {
    outer_header_removal.outer_header_removal_description =
        OUTER_HEADER_REMOVAL_GTPU_UDP_IPV4;
    update_pdr.set(outer_header_removal);
  }

  update_pdr.set(edge->pdr_id);
  update_pdr.set(precedence);
  update_pdr.set(pdi);
  update_pdr.set(edge->associated_edge->far_id);

  return update_pdr;
}

pfcp::update_far smf_session_procedure::pfcp_update_far(
    const shared_ptr<qos_upf_edge>& edge) {
  // TODO there is some duplicated code from create_far
  // Update FAR
  pfcp::update_far update_far                                     = {};
  pfcp::apply_action_t apply_action                               = {};
  pfcp::update_forwarding_parameters update_forwarding_parameters = {};
  pfcp::destination_interface_t destination_interface             = {};
  pfcp::outer_header_creation_t outer_header_creation             = {};

  if (edge->uplink) {
    destination_interface.interface_value = pfcp::INTERFACE_VALUE_CORE;
  } else {
    destination_interface.interface_value = pfcp::INTERFACE_VALUE_ACCESS;
  }
  update_forwarding_parameters.set(destination_interface);
  if (pfcp_outer_header_creation(edge, outer_header_creation)) {
    update_forwarding_parameters.set(outer_header_creation);
  }

  update_far.set(update_forwarding_parameters);
  apply_action.forw = 1;  // forward the packets

  update_far.set(edge->far_id);
  update_far.set(apply_action);

  return update_far;
}

bool smf_session_procedure::pfcp_outer_header_creation(
    const shared_ptr<qos_upf_edge>& edge,
    outer_header_creation_t& outer_header) {
  UPInterfaceType n6_type;
  n6_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6);

  if (edge->type != n6_type) {
    outer_header.outer_header_creation_description =
        OUTER_HEADER_CREATION_GTPU_UDP_IPV4;
    outer_header.teid         = edge->next_hop_fteid.teid;
    outer_header.ipv4_address = edge->next_hop_fteid.ipv4_address;
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------
smf_procedure_code smf_session_procedure::get_current_upf(
    std::vector<std::shared_ptr<qos_upf_edge>>& dl_edges,
    std::vector<std::shared_ptr<qos_upf_edge>>& ul_edges,
    std::shared_ptr<pfcp_association>& current_upf) {
  std::shared_ptr<upf_graph> graph =
      sps->get_session_handler()->get_session_graph();
  if (!graph) {
    Logger::smf_app().warn("UPF graph does not exist. Abort PFCP procedure");
    return smf_procedure_code::ERROR;
  }

  graph->dfs_current_upf(dl_edges, ul_edges, current_upf);

  if (!current_upf || ul_edges.empty() || dl_edges.empty()) {
    Logger::smf_app().warn("UPF selection failed!");
    return smf_procedure_code::ERROR;
  }
  return smf_procedure_code::OK;
}

//------------------------------------------------------------------------------
smf_procedure_code smf_session_procedure::get_next_upf(
    std::vector<std::shared_ptr<qos_upf_edge>>& dl_edges,
    std::vector<std::shared_ptr<qos_upf_edge>>& ul_edges,
    std::shared_ptr<pfcp_association>& next_upf) {
  std::shared_ptr<upf_graph> graph =
      sps->get_session_handler()->get_session_graph();
  if (!graph) {
    Logger::smf_app().warn("UPF graph does not exist. Abort PFCP procedure");
    return smf_procedure_code::ERROR;
  }

  // at some point the graph has to return true, otherwise we are done
  while (!graph->dfs_next_upf(dl_edges, ul_edges, next_upf))
    ;

  if (!next_upf) {
    Logger::smf_app().debug("UPF graph in SMF finished");
    return smf_procedure_code::OK;
  }

  if (dl_edges.empty() || ul_edges.empty()) {
    Logger::smf_app().warn("UPF selection failed!");
    return smf_procedure_code::ERROR;
  }

  return smf_procedure_code::CONTINUE;
}

//------------------------------------------------------------------------------
bool smf_session_procedure::is_qfi_served_in_edges(
    const std::vector<pfcp::qfi_t>& qfis,
    const std::vector<std::shared_ptr<qos_upf_edge>>& edges,
    std::vector<std::shared_ptr<qos_upf_edge>>& served_edges) {
  served_edges.clear();
  if (qfis.empty()) {
    Logger::smf_app().debug(
        "QFIs are not served in edges, because list of QFIs is empty (maybe "
        "because of an earlier reject");
    return false;
  }
  for (const auto& qfi : qfis) {
    bool found_qfi = false;
    for (const auto& edge : edges) {
      if (qfi == edge->qfi) found_qfi = true;
      served_edges.push_back(edge);
    }
    if (!found_qfi) {
      Logger::smf_app().error(
          "Requested QFI %d does not exist in PDU session. Cannot modify PFCP "
          "session");
      return false;
    }
  }
  return true;
}

//------------------------------------------------------------------------------
std::vector<pfcp::qfi_t>
smf_session_procedure::associate_fteid_with_created_pdrs(
    const vector<pfcp::created_pdr>& created_pdrs,
    const vector<std::shared_ptr<qos_upf_edge>>& edges) {
  // using set to eliminate duplicates (e.g. for UL CL scenario)
  std::set<uint8_t> used_qfis;
  std::vector<pfcp::qfi_t> used_qfis_pfcp;
  for (const auto& it : created_pdrs) {
    pfcp::pdr_id_t pdr_id = {};
    if (it.get(pdr_id)) {
      for (const auto& edge : edges) {
        if (edge->pdr_id == pdr_id && it.get(edge->fteid)) {
          Logger::smf_app().debug(
              "Successfully associate PDR %u with %s", edge->pdr_id.rule_id,
              to_string_fteid(edge->fteid));
          used_qfis.insert(edge->qfi.qfi);
          sps->get_session_handler()
              ->get_session_graph()
              ->update_next_hop_fteid(edge, edge->fteid);
        }
      }
    } else {
      Logger::smf_app().error("Could not get pdr_id for created_pdr");
    }
  }

  for (const auto& qfi : used_qfis) {
    pfcp::qfi_t pfcp_qfi;
    pfcp_qfi.qfi = qfi;
    used_qfis_pfcp.push_back(pfcp_qfi);
  }

  return used_qfis_pfcp;
}

//------------------------------------------------------------------------------
void smf_session_procedure::check_if_all_qfis_are_handled(
    const vector<pfcp::qfi_t>& all_qfis_to_check,
    const vector<pfcp::qfi_t>& handled_qfis) {
  if (all_qfis_to_check.size() != handled_qfis.size()) {
    Logger::smf_app().error(
        "Not all QFIs were handled by UPF, rejecting PDU session");
    sps->get_session_handler()->set_cause(
        cause_value_5gsm_e::CAUSE_31_REQUEST_REJECTED_UNSPECIFIED);
  }

  // set the values to be updated in session handler
  sps->get_session_handler()->set_qfis_to_be_updated(handled_qfis);
}

//------------------------------------------------------------------------------
int n4_session_restore_procedure::run() {
  if (pending_sessions.size()) {
    itti_n4_restore* itti_msg = nullptr;
    for (std::set<pfcp::fseid_t>::iterator it = pending_sessions.begin();
         it != pending_sessions.end(); ++it) {
      if (!itti_msg) {
        itti_msg = new itti_n4_restore(TASK_SMF_N4, TASK_SMF_APP);
      }
      itti_msg->sessions.insert(*it);
      if (itti_msg->sessions.size() >= 64) {
        std::shared_ptr<itti_n4_restore> i =
            std::shared_ptr<itti_n4_restore>(itti_msg);
        int ret = itti_inst->send_msg(i);
        if (RETURNok != ret) {
          Logger::smf_n4().error(
              "Could not send ITTI message %s to task TASK_SMF_APP",
              i->get_msg_name());
        }
        itti_msg = nullptr;
      }
    }
    if (itti_msg) {
      std::shared_ptr<itti_n4_restore> i =
          std::shared_ptr<itti_n4_restore>(itti_msg);
      int ret = itti_inst->send_msg(i);
      if (RETURNok != ret) {
        Logger::smf_n4().error(
            "Could not send ITTI message %s to task TASK_SMF_APP",
            i->get_msg_name());
        return RETURNerror;
      }
    }
  }
  return RETURNok;
}

//------------------------------------------------------------------------------
smf_procedure_code
session_create_sm_context_procedure::send_n4_session_establishment_request() {
  std::shared_ptr<pfcp_association> current_upf;
  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges;
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges;
  smf_procedure_code res = get_current_upf(dl_edges, ul_edges, current_upf);
  if (res != smf_procedure_code::OK) {
    return res;
  }

  n4_triggered = std::make_shared<itti_n4_session_establishment_request>(
      TASK_SMF_APP, TASK_SMF_N4);
  n4_triggered->trxn_id = this->trxn_id;
  n4_triggered->r_endpoint =
      endpoint(current_upf->node_id.u1.ipv4_address, pfcp::default_port);

  //-------------------
  // IE node_id_t
  //-------------------
  pfcp::node_id_t node_id = {};
  smf_cfg->get_pfcp_node_id(node_id);
  n4_triggered->pfcp_ies.set(node_id);

  //-------------------
  // IE fseid_t
  //-------------------
  pfcp::fseid_t cp_fseid = {};
  smf_cfg->get_pfcp_fseid(cp_fseid);
  cp_fseid.seid      = sps->seid;
  n4_triggered->seid = sps->seid;
  n4_triggered->pfcp_ies.set(cp_fseid);

  oai::config::smf::upf upf_cfg = current_upf->get_upf_config();

  //-------------------
  // IE CREATE_URR ( Usage Reporting Rules)
  //-------------------
  if (current_upf->get_upf_config().enable_usage_reporting()) {
    pfcp::create_urr create_urr = pfcp_create_urr(dl_edges[0]);
    n4_triggered->pfcp_ies.set(create_urr);
  }
  for (const auto& ul_edge : ul_edges) {
    n4_triggered->pfcp_ies.set(pfcp_create_far(ul_edge));
  }
  for (const auto& dl_edge : dl_edges) {
    n4_triggered->pfcp_ies.set(pfcp_create_pdr(dl_edge));
  }

  if (upf_cfg.enable_dl_pdr_in_session_establishment()) {
    for (const auto& dl_edge : dl_edges) {
      n4_triggered->pfcp_ies.set(pfcp_create_far(dl_edge));
    }
    for (const auto& ul_edge : ul_edges) {
      n4_triggered->pfcp_ies.set(pfcp_create_far(ul_edge));
    }

    Logger::smf_app().info(
        "Adding DL PDR and FAR during PFCP session establishment");
  }

  // TODO: verify whether N4 SessionID should be included in PDR and FAR
  // (Section 5.8.2.11@3GPP TS 23.501)

  Logger::smf_app().info(
      "Sending ITTI message %s to task TASK_SMF_N4",
      n4_triggered->get_msg_name());
  int ret = itti_inst->send_msg(n4_triggered);
  if (RETURNok != ret) {
    Logger::smf_app().error(
        "Could not send ITTI message %s to task TASK_SMF_N4",
        n4_triggered->get_msg_name());
    return smf_procedure_code::ERROR;
  }
  return smf_procedure_code::CONTINUE;
}

//------------------------------------------------------------------------------
smf_procedure_code session_create_sm_context_procedure::run(
    const std::shared_ptr<itti_n11_create_sm_context_request>& sm_context_req,
    const std::shared_ptr<itti_n11_create_sm_context_response>& sm_context_resp,
    std::shared_ptr<smf::smf_context> sc) {
  Logger::smf_app().info("Perform a procedure - Create SM Context Request");
  // TODO check if compatible with ongoing procedures if any
  std::shared_ptr<upf_graph> graph = {};

  upf_selection_criteria criteria;
  criteria.dnn    = sm_context_req->req.get_dnn();
  criteria.snssai = sm_context_req->req.get_snssai().to_model_snssai();

  // get the default QoS profile
  // TODO differentiate between No-PCF default QoS and PCF authorized Qos
  // scenario
  subscribed_default_qos_t default_qos                = {};
  std::shared_ptr<session_management_subscription> ss = {};
  sc->get_default_qos(
      sm_context_req->req.get_snssai(), sm_context_req->req.get_dnn(),
      default_qos);

  criteria.qos_profile._5qi           = default_qos._5qi;
  criteria.qos_profile.arp            = default_qos.arp;
  criteria.qos_profile.priority_level = default_qos.priority_level;

  // Find PDU session
  std::shared_ptr<smf_context_ref> scf = {};
  if (smf_app_inst->is_scid_2_smf_context(sm_context_req->scid)) {
    scf = smf_app_inst->scid_2_smf_context(sm_context_req->scid);
    // scf.get()->upf_node_id = up_node_id;
    std::shared_ptr<smf_pdu_session> sp = {};
    if (!sc->find_pdu_session(scf->pdu_session_id, sp)) {
      Logger::smf_app().warn("PDU session context does not exist!");
      sm_context_resp->res.set_cause(
          PDU_SESSION_APPLICATION_ERROR_CONTEXT_NOT_FOUND);
      return smf_procedure_code::ERROR;
    }

    if (sp->policy_ptr) {
      graph = pfcp_associations::get_instance().select_up_node(
          sp->policy_ptr->decision, criteria);
      if (!graph) {
        Logger::smf_app().warn(
            "UPF selection based on PCC rules failed. Use any UPF.");
      }
    }
    if (!graph) {
      // No policies found or graph selection failed, use default UPF selection
      graph = pfcp_associations::get_instance().select_up_node(criteria);
    }
    // if still there is no graph, send an error
    if (!graph) {
      // TODO better error code?
      sm_context_resp->res.set_cause(
          PDU_SESSION_APPLICATION_ERROR_PEER_NOT_RESPONDING);
      return smf_procedure_code::ERROR;
    } else {
      sp->get_session_handler()->set_session_graph(graph);
    }
  } else {
    Logger::smf_app().warn(
        "SM Context associated with this id " SCID_FMT " does not exit!",
        sm_context_req->scid);
    // TODO:
  }

  //-------------------
  n11_trigger           = sm_context_req;
  n11_triggered_pending = sm_context_resp;
  uint64_t seid         = smf_app_inst->generate_seid();
  sps->set_seid(seid);
  // for finding procedure when receiving response
  smf_app_inst->set_seid_2_smf_context(seid, sc);

  graph->start_asynch_dfs_procedure(true);

  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges;
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges;
  std::shared_ptr<pfcp_association> upf = {};
  // Get next UPF for the first N4 session establishment
  smf_procedure_code res = get_next_upf(dl_edges, ul_edges, upf);
  if (res != smf_procedure_code::CONTINUE) {
    return res;
  }

  return send_n4_session_establishment_request();
}

//------------------------------------------------------------------------------
smf_procedure_code session_create_sm_context_procedure::handle_itti_msg(
    itti_n4_session_establishment_response& resp,
    std::shared_ptr<smf::smf_context> sc) {
  Logger::smf_app().debug(
      "Handle N4 Session Establishment Response (PDU Session Id %d)",
      n11_trigger->req.get_pdu_session_id());

  pfcp::cause_t cause = {};
  resp.pfcp_ies.get(cause);
  if (cause.cause_value == pfcp::CAUSE_VALUE_REQUEST_ACCEPTED) {
    resp.pfcp_ies.get(sps->up_fseid);
    n11_triggered_pending->res.set_cause(
        static_cast<uint8_t>(cause_value_5gsm_e::CAUSE_255_REQUEST_ACCEPTED));
  } else {
    // remove QFIs to be handled to
    sps->get_session_handler()->set_qfis_to_be_updated({});
    Logger::smf_app().warn(
        "N4 Session Establishment Request for PDU Session ID %d was rejected "
        "by UPF",
        n11_trigger->req.get_pdu_session_id());
    // TODO we should have a good cause mapping here
    n11_triggered_pending->res.set_cause(static_cast<uint8_t>(
        cause_value_5gsm_e::CAUSE_31_REQUEST_REJECTED_UNSPECIFIED));
    // TODO we need to abort all ongoing sessions
    return smf_procedure_code::ERROR;
  }

  std::shared_ptr<pfcp_association> current_upf = {};
  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges;
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges;

  if (get_current_upf(dl_edges, ul_edges, current_upf) !=
      smf_procedure_code::OK) {
    return smf_procedure_code::ERROR;
  }
  oai::config::smf::upf upf_cfg = current_upf->get_upf_config();

  pfcp::up_function_features_s up_features =
      current_upf->function_features.second;

  // TODO
  /*
  if (upf_cfg.enable_dl_pdr_in_session_establishment() &&
      resp.pfcp_ies.created_pdrs.empty()) {
    pfcp::pdr_id_t pdr_id_tmp;
    // we use qos flow for 1st PDR for the moment
    // TODO: remove this hardcoding of qos flow
    pdr_id_tmp.rule_id = 1;
    auto flow          = dl_edges[0].get_qos_flow(pdr_id_tmp);
    if (flow) {
      default_qos_flow = flow;
    }
  } */
  std::vector<pfcp::qfi_t> used_qfis =
      associate_fteid_with_created_pdrs(resp.pfcp_ies.created_pdrs, dl_edges);

  UPInterfaceType n9_type;
  n9_type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N9);
  // covers the case that UL CL is returned from algorithm, but not all TEIDs
  // have been set (not all paths explored yet)
  //  we go through until no UPF is left or until we find one to send N4 to
  bool search_upf = true;
  bool send_n4    = true;
  smf_procedure_code send_n4_res;
  while (search_upf) {
    std::vector<std::shared_ptr<qos_upf_edge>> next_dl_edges;
    std::vector<std::shared_ptr<qos_upf_edge>> next_ul_edges;
    std::shared_ptr<pfcp_association> next_upf = {};
    send_n4_res = get_next_upf(next_dl_edges, next_ul_edges, next_upf);
    if (send_n4_res != smf_procedure_code::CONTINUE) {
      search_upf = false;
      send_n4    = false;
    } else {
      Logger::smf_app().debug(
          "Try to send N4 to UPF %s", next_upf->get_printable_name());
      // update FTEID for forward tunnel info for this edge
      send_n4 = true;
      for (const auto& ul_edge : next_ul_edges) {
        if (ul_edge->type == n9_type && ul_edge->next_hop_fteid.is_zero()) {
          Logger::smf_app().debug(
              "UPF %s has unvisited UL edges", next_upf->get_printable_name());
          send_n4 = false;
        }
      }
      // if we found UPF to send N4, we don't need to search UPF anymore
      search_upf = !send_n4;
    }
  }
  if (send_n4) {
    return send_n4_session_establishment_request();
  }

  auto all_qfis = sps->get_session_handler()->get_all_qfis();
  if (up_features.ftup) {
    check_if_all_qfis_are_handled(all_qfis, used_qfis);
  } else {
    // If UPF does not support TEID Creation then set all qfis to be updated in
    // session handler
    sps->get_session_handler()->set_qfis_to_be_updated(all_qfis);
  }

  // TODO we have more than one QoS flow here, to adapt with new QoS framework
  for (const auto& flow :
       sps->get_session_handler()->get_qos_flows_context_updated()) {
    n11_triggered_pending->res.set_qos_flow_context(flow);
  }

  return smf_procedure_code::OK;
}

//------------------------------------------------------------------------------
smf_procedure_code
session_update_sm_context_procedure::send_n4_session_modification_request(
    const std::vector<pfcp::qfi_t>& list_of_qfis) {
  Logger::smf_app().debug("Send N4 Session Modification Request");

  std::shared_ptr<pfcp_association> current_upf = {};
  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges{};
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges{};
  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges_to_use{};
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges_to_use{};

  if (get_current_upf(dl_edges, ul_edges, current_upf) !=
      smf_procedure_code::OK) {
    return smf_procedure_code::ERROR;
  }

  if (list_of_qfis.empty()) {
    dl_edges_to_use = dl_edges;
    ul_edges_to_use = ul_edges;
  } else {
    // get edges for QFIs to be updated
    is_qfi_served_in_edges(list_of_qfis, dl_edges, dl_edges_to_use);
    is_qfi_served_in_edges(list_of_qfis, ul_edges, ul_edges_to_use);
  }

  oai::config::smf::upf upf_cfg = current_upf->get_upf_config();

  n4_triggered = std::make_shared<itti_n4_session_modification_request>(
      TASK_SMF_APP, TASK_SMF_N4);
  n4_triggered->seid    = sps->up_fseid.seid;
  n4_triggered->trxn_id = this->trxn_id;
  n4_triggered->r_endpoint =
      endpoint(current_upf->node_id.u1.ipv4_address, pfcp::default_port);

  for (const auto& dl_edge : dl_edges_to_use) {
    n4_triggered->pfcp_ies.set(pfcp_create_far(dl_edge));
  }

  for (const auto& ul_edge : ul_edges_to_use) {
    n4_triggered->pfcp_ies.set(pfcp_create_pdr(ul_edge));
  }

  Logger::smf_app().info(
      "Sending ITTI message %s to task TASK_SMF_N4",
      n4_triggered->get_msg_name());
  int ret = itti_inst->send_msg(n4_triggered);
  if (RETURNok != ret) {
    Logger::smf_app().error(
        "Could not send ITTI message %s to task TASK_SMF_N4",
        n4_triggered->get_msg_name());
    return smf_procedure_code::ERROR;
  }
  return smf_procedure_code::CONTINUE;
}

//------------------------------------------------------------------------------
smf_procedure_code session_update_sm_context_procedure::run(
    const std::shared_ptr<itti_n11_update_sm_context_request>& sm_context_req,
    std::shared_ptr<itti_n11_update_sm_context_response> sm_context_resp,
    const std::shared_ptr<smf::smf_context>& sc) {
  // Handle SM update sm context request
  // The SMF initiates an N4 Session Modification procedure with the UPF. The
  // SMF provides AN Tunnel Info to the UPF as well as the corresponding
  // forwarding rules

  bool send_n4 = false;
  Logger::smf_app().info("Perform a procedure - Update SM Context Request");
  // TODO check if compatible with ongoing procedures if any
  // Get UPF node
  std::shared_ptr<smf_context_ref> scf = {};
  scid_t scid                          = {};
  try {
    scid = std::stoi(sm_context_req->scid);
  } catch (const std::exception& err) {
    Logger::smf_app().warn(
        "SM Context associated with this id %s does not exit!",
        sm_context_req->scid.c_str());
  }
  if (smf_app_inst->is_scid_2_smf_context(scid)) {
    scf = smf_app_inst->scid_2_smf_context(scid);
    // up_node_id = scf.get()->upf_node_id;
  } else {
    Logger::smf_app().warn(
        "SM Context associated with this id " SCID_FMT " does not exit!", scid);
    // TODO:
    return smf_procedure_code::ERROR;
  }

  std::shared_ptr<smf_pdu_session> sp = {};
  if (!sc->find_pdu_session(scf->pdu_session_id, sp)) {
    Logger::smf_app().warn("PDU session context does not exist!");
    return smf_procedure_code::ERROR;
  }

  std::shared_ptr<upf_graph> graph =
      sps->get_session_handler()->get_session_graph();

  if (!graph) {
    Logger::smf_app().warn("PDU session does not have a UPF association");
    return smf_procedure_code::ERROR;
  }

  //  TODO: UPF insertion in case of Handover

  graph->start_asynch_dfs_procedure(false);

  std::shared_ptr<pfcp_association> current_upf = {};
  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges;
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges;
  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges_to_update;
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges_to_update;

  if (get_next_upf(dl_edges, ul_edges, current_upf) !=
      smf_procedure_code::CONTINUE) {
    Logger::smf_app().error("DL Procedure Error: No UPF to select");
    return smf_procedure_code::ERROR;
  }

  oai::config::smf::upf upf_cfg = current_upf->get_upf_config();

  //-------------------
  n11_trigger           = sm_context_req;
  n11_triggered_pending = std::move(sm_context_resp);

  n4_triggered = std::make_shared<itti_n4_session_modification_request>(
      TASK_SMF_APP, TASK_SMF_N4);
  n4_triggered->seid    = sps->up_fseid.seid;
  n4_triggered->trxn_id = this->trxn_id;
  n4_triggered->r_endpoint =
      endpoint(current_upf->node_id.u1.ipv4_address, pfcp::default_port);

  // QoS Flow to be modified
  pdu_session_update_sm_context_request sm_context_req_msg =
      sm_context_req->req;
  std::vector<pfcp::qfi_t> list_of_qfis_to_be_modified = {};
  sm_context_req_msg.get_qfis(list_of_qfis_to_be_modified);

  sps->get_session_handler()->set_qfis_to_be_updated(
      list_of_qfis_to_be_modified);

  if (!is_qfi_served_in_edges(
          list_of_qfis_to_be_modified, dl_edges, dl_edges_to_update) ||
      !is_qfi_served_in_edges(
          list_of_qfis_to_be_modified, ul_edges, ul_edges_to_update)) {
    // TODO check on NAS, maybe can reject some QFIs and accept others?
    Logger::smf_app().error(
        "PDU Session establishment modification failed. Wrong QFI. Sending "
        "reject");
    n11_triggered_pending->res.set_cause(static_cast<uint8_t>(
        cause_value_5gsm_e::CAUSE_31_REQUEST_REJECTED_UNSPECIFIED));
    return smf_procedure_code::ERROR;
  }

  Logger::smf_app().debug(
      "Session procedure type: %s",
      session_management_procedures_type_e2str
          .at(static_cast<int>(session_procedure_type))
          .c_str());

  pfcp::fteid_t gnb_fteid = {};
  sm_context_req_msg.get_dl_fteid(gnb_fteid);

  switch (session_procedure_type) {
    case session_management_procedures_type_e::
        PDU_SESSION_ESTABLISHMENT_UE_REQUESTED:
    case session_management_procedures_type_e::
        PDU_SESSION_MODIFICATION_SMF_REQUESTED:
    case session_management_procedures_type_e::
        PDU_SESSION_MODIFICATION_AN_REQUESTED:
    case session_management_procedures_type_e::
        PDU_SESSION_MODIFICATION_UE_INITIATED_STEP2: {
      for (const auto& dl_edge : dl_edges_to_update) {
        if (gnb_fteid == dl_edge->next_hop_fteid) {
          Logger::smf_app().debug(
              "QFI %d dl_fteid unchanged", dl_edge->qfi.qfi);
          // return smf_procedure_code::OK;
          continue;
        } else {
          dl_edge->next_hop_fteid = gnb_fteid;
        }
      }
      return send_n4_session_modification_request(list_of_qfis_to_be_modified);
    }

    case session_management_procedures_type_e::HO_PATH_SWITCH_REQ:
    case session_management_procedures_type_e::N2_HO_PREPARATION_PHASE_STEP2: {
      for (const auto& dl_edge : dl_edges_to_update) {
        if (gnb_fteid == dl_edge->next_hop_fteid) {
          Logger::smf_app().debug(
              "QFI %d dl_fteid unchanged", dl_edge->qfi.qfi);
          return smf_procedure_code::OK;
        } else if (dl_edge->far_id.far_id != 0) {
          // Update DL F-TEID because of new info from gNB after handover
          // then tell it to UPF with Update FAR
          dl_edge->next_hop_fteid = gnb_fteid;
          n4_triggered->pfcp_ies.set(pfcp_update_far(dl_edge));
          send_n4 = true;
        } else {
          // handover, but FAR ID is not existing yet, we create new one
          dl_edge->next_hop_fteid = gnb_fteid;
          n4_triggered->pfcp_ies.set(pfcp_create_far(dl_edge));
          send_n4 = true;
        }
      }

      // for each UL edge we need to update or create the PDR
      for (auto& ul_edge : ul_edges_to_update) {
        if (ul_edge->pdr_id.rule_id != 0) {
          n4_triggered->pfcp_ies.set(pfcp_update_pdr(ul_edge));
          send_n4 = true;
        } else {
          n4_triggered->pfcp_ies.set(pfcp_create_pdr(ul_edge));
          send_n4 = true;
        }
      }
    } break;

    case session_management_procedures_type_e::
        SERVICE_REQUEST_UE_TRIGGERED_STEP2: {
      // here we only have to update first UPF, as we get new F-TEID from gNB,
      // basically just make new PDRs / FARs in DL
      for (const auto& dl_edge : dl_edges) {
        dl_edge->next_hop_fteid = gnb_fteid;
      }
      // At this stage, is the list of QFIs from NGAP always sent and should we
      // honor it? here we just update everything regardless of QFI
      std::vector<pfcp::qfi_t> empty_qfi_list;
      send_n4_session_modification_request(empty_qfi_list);

      // as the procedure is done at this point, we tell smf_context to not
      // continue
      return smf_procedure_code::OK;
    }

    case session_management_procedures_type_e::
        SERVICE_REQUEST_UE_TRIGGERED_STEP1: {
      Logger::smf_app().debug("SERVICE_REQUEST_UE_TRIGGERED_STEP1");

      // make PDR/FAR in UL
      // TODO do we still need this "trick" to increase precedence to not
      // confuse UPF?
      for (const auto& ul_edge : ul_edges_to_update) {
        ul_edge->precedence += 1;
        n4_triggered->pfcp_ies.set(pfcp_create_far(ul_edge));
      }
      for (const auto& dl_edge : dl_edges_to_update) {
        dl_edge->precedence += 1;
        n4_triggered->pfcp_ies.set(pfcp_create_pdr(dl_edge));
      }
      // Re-enable also old URR
      if (current_upf->get_upf_config().enable_usage_reporting()) {
        n4_triggered->pfcp_ies.set(pfcp_create_urr(dl_edges_to_update[0]));
      }
      send_n4 = true;
    } break;

    case session_management_procedures_type_e::
        PDU_SESSION_RELEASE_AN_INITIATED: {
      Logger::smf_app().debug("PDU_SESSION_RELEASE_AN_INITIATED");
      remove_pdrs_and_fars(ul_edges_to_update);
      remove_pdrs_and_fars(dl_edges_to_update);
      send_n4 = true;
    } break;

    default: {
      Logger::smf_app().error(
          "Update SM Context procedure: Unknown session management type %d",
          (int) session_procedure_type);
    }
  }

  if (send_n4) {
    Logger::smf_app().info(
        "Sending ITTI message %s to task TASK_SMF_N4",
        n4_triggered->get_msg_name());
    int ret = itti_inst->send_msg(n4_triggered);
    if (RETURNok != ret) {
      Logger::smf_app().error(
          "Could not send ITTI message %s to task TASK_SMF_N4",
          n4_triggered->get_msg_name());
      return smf_procedure_code::ERROR;
    }
  } else {
    Logger::smf_app().error(
        "Update SM Context procedure: There is no QoS flow to be modified");
    return smf_procedure_code::ERROR;
  }
  return smf_procedure_code::OK;
}

//------------------------------------------------------------------------------
smf_procedure_code session_update_sm_context_procedure::handle_itti_msg(
    itti_n4_session_modification_response& resp,
    std::shared_ptr<smf::smf_context> sc) {
  Logger::smf_app().info(
      "Handle N4 Session Modification Response (PDU Session Id %d)",
      n11_trigger->req.get_pdu_session_id());

  pfcp::cause_t cause = {};
  resp.pfcp_ies.get(cause);

  n11_triggered_pending->res.set_cause(static_cast<uint8_t>(
      cause_value_5gsm_e::CAUSE_31_REQUEST_REJECTED_UNSPECIFIED));

  if (cause.cause_value != CAUSE_VALUE_REQUEST_ACCEPTED) {
    // TODO: Nsmf_PDUSession_SMContextStatusNotify
    /*  If the PDU Session establishment is not successful, the SMF informs
     the AMF by invoking Nsmf_PDUSession_SMContextStatusNotify (Release). The
     SMF also releases any N4 session(s) created, any PDU Session address if
     allocated (e.g. IP address) and releases the association with PCF, if
     any. see step 18, section 4.3.2.2.1@3GPP TS 23.502)
     */
    // TODO: should we return here with smf_procedure_code::ERROR;
  } else {
    n11_triggered_pending->res.set_cause(
        static_cast<uint8_t>(cause_value_5gsm_e::CAUSE_255_REQUEST_ACCEPTED));
  }

  // list of accepted QFI(s) and AN Tunnel Info corresponding to the PDU
  // Session
  std::vector<pfcp::qfi_t> list_of_qfis_to_be_modified = {};
  n11_trigger->req.get_qfis(list_of_qfis_to_be_modified);

  std::shared_ptr<pfcp_association> current_upf = {};
  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges{};
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges{};
  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges_to_update{};
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges_to_update{};

  if (get_current_upf(dl_edges, ul_edges, current_upf) ==
      smf_procedure_code::ERROR) {
    Logger::smf_app().error("SMF DL procedure: Could not get current UPF");
    // TODO is this enough as an error message? We have cause 31 but not
    // values
    return smf_procedure_code::ERROR;
  }
  // TODO put in helper function or make a get_current_upf with this
  if (!is_qfi_served_in_edges(
          list_of_qfis_to_be_modified, dl_edges, dl_edges_to_update) ||
      !is_qfi_served_in_edges(
          list_of_qfis_to_be_modified, ul_edges, ul_edges_to_update)) {
    // TODO check on NAS, maybe can reject some QFIs and accept others?
    Logger::smf_app().error(
        "PDU Session establishment modification failed. Wrong QFI. Sending "
        "reject");
    n11_triggered_pending->res.set_cause(static_cast<uint8_t>(
        cause_value_5gsm_e::CAUSE_31_REQUEST_REJECTED_UNSPECIFIED));
    return smf_procedure_code::ERROR;
  }

  bool continue_n4 = true;

  Logger::smf_app().debug(
      "Session procedure type: %s",
      session_management_procedures_type_e2str
          .at(static_cast<int>(session_procedure_type))
          .c_str());

  nlohmann::json json_data = {};
  std::map<uint8_t, qos_flow_context_updated> qos_flow_context_to_be_updateds =
      {};
  n11_triggered_pending->res.get_all_qos_flow_context_updateds(
      qos_flow_context_to_be_updateds);
  n11_triggered_pending->res.remove_all_qos_flow_context_updateds();
  for (const auto& it : qos_flow_context_to_be_updateds)
    Logger::smf_app().debug("QoS Flow context to be modified QFI %d", it.first);

  switch (session_procedure_type) {
    case session_management_procedures_type_e::
        PDU_SESSION_ESTABLISHMENT_UE_REQUESTED:
    case session_management_procedures_type_e::
        PDU_SESSION_MODIFICATION_SMF_REQUESTED:
    case session_management_procedures_type_e::
        PDU_SESSION_MODIFICATION_AN_REQUESTED:
    case session_management_procedures_type_e::
        SERVICE_REQUEST_UE_TRIGGERED_STEP2:
    case session_management_procedures_type_e::
        PDU_SESSION_MODIFICATION_UE_INITIATED_STEP2: {
      std::vector<pfcp::qfi_t> used_qfis = associate_fteid_with_created_pdrs(
          resp.pfcp_ies.created_pdrs, ul_edges_to_update);
      // if it is not empty, we have created PDR with F-TEID in PDU session
      // modification
      if (!used_qfis.empty()) {
        check_if_all_qfis_are_handled(list_of_qfis_to_be_modified, used_qfis);
      }
      continue_n4 = true;
      /* the difference between normal PDU session establishment and HO is:
       * in PDU sess establishment, we have to make DL tunnels for all UPFs,
       * e.g. in ULCL or other modes When we have a handover (at least in SCC 1)
       * we only change the first UPF
       */
    } break;
    case session_management_procedures_type_e::HO_PATH_SWITCH_REQ:
    case session_management_procedures_type_e::N2_HO_PREPARATION_PHASE_STEP2: {
      std::vector<pfcp::qfi_t> used_qfis = associate_fteid_with_created_pdrs(
          resp.pfcp_ies.created_pdrs, ul_edges_to_update);
      if (!used_qfis.empty()) {
        check_if_all_qfis_are_handled(list_of_qfis_to_be_modified, used_qfis);
      }
      continue_n4 = false;
    } break;

    case session_management_procedures_type_e::
        SERVICE_REQUEST_UE_TRIGGERED_STEP1: {
      Logger::smf_app().debug(
          "PDU Session Update SM Context, SERVICE_REQUEST_UE_TRIGGERED_STEP1");

      std::vector<pfcp::qfi_t> used_qfis = associate_fteid_with_created_pdrs(
          resp.pfcp_ies.created_pdrs, dl_edges_to_update);

      check_if_all_qfis_are_handled(list_of_qfis_to_be_modified, used_qfis);
      // we just update N3 interface
      continue_n4 = false;

    } break;

    case session_management_procedures_type_e::
        PDU_SESSION_RELEASE_AN_INITIATED: {
      Logger::smf_app().debug("PDU_SESSION_RELEASE_AN_INITIATED");

      for (const auto& it : qos_flow_context_to_be_updateds) {
        Logger::smf_app().debug(
            "QoS Flow context to be modified QFI %d", it.first);
        // sps->remove_qos_flow(it.second.qfi);
      }
      // Mark as deactivated
      sps->set_upCnx_state(upCnx_state_e::UPCNX_STATE_DEACTIVATED);

      json_data["upCnxState"] = "DEACTIVATED";
      n11_triggered_pending->res.set_json_data(json_data);
      // we just update N3 interface
      continue_n4 = false;
    } break;

    default: {
      Logger::smf_app().error(
          "Update SM Context procedure: Unknown session management type %d",
          (int) session_procedure_type);
    }
  }

  std::shared_ptr<pfcp_association> next_upf = {};
  std::vector<std::shared_ptr<qos_upf_edge>> next_dl_edges{};
  std::vector<std::shared_ptr<qos_upf_edge>> next_ul_edges{};

  if (continue_n4 && get_next_upf(next_dl_edges, next_ul_edges, next_upf) ==
                         smf_procedure_code::CONTINUE) {
    return send_n4_session_modification_request(list_of_qfis_to_be_modified);
  }

  for (const auto& flow :
       sps->get_session_handler()->get_qos_flows_context_updated()) {
    n11_triggered_pending->res.add_qos_flow_context_updated(flow);
  }

  // n11_triggered_pending->res.set_cause(cause.cause_value);
  n11_triggered_pending->res.set_http_code(
      oai::common::sbi::http_status_code::OK);

  return smf_procedure_code::OK;
}

//------------------------------------------------------------------------------
void session_update_sm_context_procedure::remove_pdrs_and_fars(
    const vector<std::shared_ptr<qos_upf_edge>>& edges) {
  for (const auto& edge : edges) {
    if (edge->pdr_id.rule_id != 0) {
      n4_triggered->pfcp_ies.set(pfcp_remove_pdr(edge));
    }
    if (edge->far_id.far_id != 0) {
      n4_triggered->pfcp_ies.set(pfcp_remove_far(edge));
    }
    edge->clear_session();
  }
}

//------------------------------------------------------------------------------
smf_procedure_code
session_release_sm_context_procedure::send_n4_session_deletion_request() {
  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges;
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges;
  std::shared_ptr<pfcp_association> current_upf = {};

  if (get_current_upf(dl_edges, ul_edges, current_upf) ==
      smf_procedure_code::ERROR) {
    return smf_procedure_code::ERROR;
  }

  n4_triggered = std::make_shared<itti_n4_session_deletion_request>(
      TASK_SMF_APP, TASK_SMF_N4);
  n4_triggered->seid    = sps->up_fseid.seid;
  n4_triggered->trxn_id = this->trxn_id;
  n4_triggered->r_endpoint =
      endpoint(current_upf->node_id.u1.ipv4_address, pfcp::default_port);

  Logger::smf_app().info(
      "Sending ITTI message %s to task TASK_SMF_N4",
      n4_triggered->get_msg_name());
  int ret = itti_inst->send_msg(n4_triggered);
  if (RETURNok != ret) {
    Logger::smf_app().error(
        "Could not send ITTI message %s to task TASK_SMF_N4",
        n4_triggered->get_msg_name());
    return smf_procedure_code::ERROR;
  }
  return smf_procedure_code::CONTINUE;
}

//------------------------------------------------------------------------------
smf_procedure_code session_release_sm_context_procedure::run(
    const std::shared_ptr<itti_n11_release_sm_context_request>& sm_context_req,
    std::shared_ptr<itti_n11_release_sm_context_response> sm_context_res,
    const std::shared_ptr<smf::smf_context>& sc) {
  Logger::smf_app().info("Release SM Context Request");
  // TODO check if compatible with ongoing procedures if any
  pfcp::node_id_t up_node_id = {};
  // Get UPF node
  std::shared_ptr<smf_context_ref> scf = {};
  scid_t scid                          = {};
  try {
    scid = std::stoi(sm_context_req->scid);
  } catch (const std::exception& err) {
    Logger::smf_app().warn(
        "SM Context associated with this id %s does not exit!",
        sm_context_req->scid.c_str());
  }
  if (smf_app_inst->is_scid_2_smf_context(scid)) {
    scf = smf_app_inst->scid_2_smf_context(scid);
    // up_node_id = scf.get()->upf_node_id;
  } else {
    Logger::smf_app().warn(
        "SM Context associated with this id " SCID_FMT " does not exit!", scid);
    // TODO:
    return smf_procedure_code::ERROR;
  }

  std::shared_ptr<smf_pdu_session> sp = {};
  if (!sc->find_pdu_session(scf->pdu_session_id, sp)) {
    Logger::smf_app().warn("PDU session context does not exist!");
    return smf_procedure_code::ERROR;
  }

  std::shared_ptr<upf_graph> graph =
      sp->get_session_handler()->get_session_graph();

  if (!graph) {
    Logger::smf_app().warn("PDU session does not have a UPF association");
    return smf_procedure_code::ERROR;
  }
  // we start from the access nodes, because we have only ULCLs we don't have
  // the situation that one UPF is returned more than once
  graph->start_asynch_dfs_procedure(false);

  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges;
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges;
  std::shared_ptr<pfcp_association> current_upf = {};
  if (get_next_upf(dl_edges, ul_edges, current_upf) ==
      smf_procedure_code::ERROR) {
    return smf_procedure_code::ERROR;
  }

  n11_trigger           = sm_context_req;
  n11_triggered_pending = std::move(sm_context_res);
  return send_n4_session_deletion_request();
}

//------------------------------------------------------------------------------
smf_procedure_code session_release_sm_context_procedure::handle_itti_msg(
    itti_n4_session_deletion_response& resp,
    std::shared_ptr<smf::smf_context> sc) {
  Logger::smf_app().info(
      "Handle itti_n4_session_deletion_response (Release SM Context "
      "Request): "
      "pdu-session-id %d",
      n11_trigger->req.get_pdu_session_id());

  pfcp::cause_t cause = {};
  resp.pfcp_ies.get(cause);

  std::vector<std::shared_ptr<qos_upf_edge>> dl_edges;
  std::vector<std::shared_ptr<qos_upf_edge>> ul_edges;
  std::shared_ptr<pfcp_association> current_upf = {};
  if (get_next_upf(dl_edges, ul_edges, current_upf) ==
      smf_procedure_code::CONTINUE) {
    // If we have to continue, we ignore the PFCP error code, because we
    // should at least remove other UPF sessions
    return send_n4_session_deletion_request();
  }

  if (cause.cause_value == CAUSE_VALUE_REQUEST_ACCEPTED) {
    n11_triggered_pending->res.set_cause(
        static_cast<uint8_t>(cause_value_5gsm_e::CAUSE_255_REQUEST_ACCEPTED));
    Logger::smf_app().info("PDU Session Release SM Context accepted by UPFs");
    return smf_procedure_code::OK;
  } else {
    n11_triggered_pending->res.set_cause(static_cast<uint8_t>(
        cause_value_5gsm_e::CAUSE_31_REQUEST_REJECTED_UNSPECIFIED));
    // We cannot return an error here, because we need to delete all the UPFs
    return smf_procedure_code::ERROR;
  }

  // TODO:
  /* If it is the last PDU Session the SMF is handling for the UE for the
   associated (DNN, S- NSSAI), the SMF unsubscribes from Session Management
   Subscription data changes notification with the UDM by means of the
   Nudm_SDM_Unsubscribe service operation. The SMF invokes the
   Nudm_UECM_Deregistration service operation so that the UDM removes the
   association it had stored between the SMF identity and the associated DNN
   and PDU Session Id
   */
}
