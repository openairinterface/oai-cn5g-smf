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

#include "smf_n2.hpp"

#include <Ngap_GBR-QosInformation.h>
#include <arpa/inet.h>

#include <stdexcept>

#include "3gpp_commons.h"
#include "Helpers.h"
#include "PduSessionResourceSetupRequestTransfer.hpp"
#include "PduSessionType.hpp"
#include "PreemptionCapability_anyOf.h"
#include "PreemptionVulnerability_anyOf.h"
#include "common_defs.h"
#include "conversions.h"
#include "output_wrapper.hpp"
#include "smf.h"
#include "string.hpp"
#include "utils.hpp"

using namespace smf;
using namespace oai::model::common;
using namespace oai::ngap;
extern smf_app* smf_app_inst;

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_setup_request_transfer(
    const std::shared_ptr<pdu_session_sm_context_response>& sm_context_res,
    const std::map<uint8_t, qos_flow_context_updated>& qos_flows,
    n2_sm_info_type_e ngap_info_type,
    PduSessionResourceSetupRequestTransfer&
        pdu_session_resource_setup_request_transfer) {
  Logger::smf_n2().info(
      "Create N2 SM Information, PDU Session Resource Setup Request Transfer");

  bool result = false;

  for (const auto& qos_flow_pair : qos_flows) {
    auto qos_flow = qos_flow_pair.second;
    Logger::smf_n2().debug(
        "UL F-TEID, TEID "
        "0x%" PRIx32 ", IP Address %s",
        qos_flow.ul_fteid.teid,
        oai::utils::conv::toString(qos_flow.ul_fteid.ipv4_address).c_str());
    Logger::smf_n2().info(
        "QoS parameters: QFI %d, 5QI: %d, Priority level %d, ARP priority "
        "level %d",
        qos_flow.qfi.qfi, qos_flow.qos_profile.getR5qi(),
        qos_flow.qos_profile.getPriorityLevel(),
        qos_flow.qos_profile.getArp().getPriorityLevel());

    // check the QoS Flow
    if ((qos_flow.qfi.qfi < QOS_FLOW_IDENTIFIER_FIRST) or
        (qos_flow.qfi.qfi > QOS_FLOW_IDENTIFIER_LAST)) {
      // error
      Logger::smf_n2().error("Incorrect QFI %d", qos_flow.qfi.qfi);
      return false;
    }
  }

  // PDUSessionAggregateMaximumBitRate
  PduSessionAggregateMaximumBitRate pdu_session_aggregate_maximum_bit_rate = {};
  supi_t supi                     = sm_context_res->get_supi();
  supi64_t supi64                 = smf_supi_to_u64(supi);
  std::shared_ptr<smf_context> sc = {};
  if (smf_app_inst->is_supi_2_smf_context(supi64)) {
    Logger::smf_n2().debug("Get SMF context with SUPI " SUPI_64_FMT "", supi64);
    sc = smf_app_inst->supi_2_smf_context(supi64);
    oai::nas::SessionAmbr session_ambr = {};
    sc.get()->get_session_ambr(
        session_ambr, sm_context_res->get_snssai(), sm_context_res->get_dnn());
    uint64_t bit_rate_dl = session_handler::parse_nas_value_unit_to_bps(
        session_ambr.GetSessionAmbrForDownlink(),
        session_ambr.GetUnitForDownlink());
    uint64_t bit_rate_ul = session_handler::parse_nas_value_unit_to_bps(
        session_ambr.GetSessionAmbrForUplink(),
        session_ambr.GetUnitForUplink());
    pdu_session_aggregate_maximum_bit_rate.set(bit_rate_dl, bit_rate_ul);

  } else {
    Logger::smf_n2().warn(
        "SMF context with SUPI " SUPI_64_FMT " does not exist!", supi64);
    return false;
  }
  pdu_session_resource_setup_request_transfer
      .setPduSessionAggregateMaximumBitRate(
          pdu_session_aggregate_maximum_bit_rate);

  // UL NG-U UP TNL Information (UP Transport Layer Information)
  // TODO: only first flow is considered for now
  pfcp::fteid_t ul_fteid = {};
  ul_fteid.v4            = qos_flows.begin()->second.ul_fteid.v4;
  ul_fteid.teid          = qos_flows.begin()->second.ul_fteid.teid;
  ul_fteid.ipv4_address  = qos_flows.begin()->second.ul_fteid.ipv4_address;

  UpTransportLayerInformation ul_ng_u_up_tnl_information = {};
  TransportLayerAddress transport_layer_address          = {};
  GtpTeid gtp_teid                                       = {};
  transport_layer_address.setIpv4Address(ul_fteid.ipv4_address);
  gtp_teid.set(ul_fteid.teid);
  ul_ng_u_up_tnl_information.set(transport_layer_address, gtp_teid);
  pdu_session_resource_setup_request_transfer.setUlNgUUpTnlInformation(
      ul_ng_u_up_tnl_information);

  // TODO: Additional UL NG-U UP TNL Information
  // TODO: DataForwardingNotPossible

  // PDUSessionType
  uint8_t pdu_session_type =
      sm_context_res->get_pdu_session_type() -
      1;  // TODO: dirty code, difference between Ngap_PDUSessionType_ipv4 vs
  // pdu_session_type_e::PDU_SESSION_TYPE_E_IPV4 (TS 38.413 vs
  // TS 24.501)

  pdu_session_resource_setup_request_transfer.setPduSessionType(
      static_cast<e_Ngap_PDUSessionType>(pdu_session_type));

  Logger::smf_n2().debug(
      "PDU Session Type: %d ", sm_context_res->get_pdu_session_type());

  // TODO: Security Indication
  // TODO: Network Instance

  // QoS Flow Setup Request List
  QosFlowSetupRequestList qos_flow_setup_request_list;
  std::vector<QosFlowSetupRequestItem> qos_flow_setup_request_list_vector;
  QosFlowSetupRequestItem qos_flow_setup_request_item;

  int i = 0;
  for (const auto& qos_flow_pair : qos_flows) {
    auto qos_flow = qos_flow_pair.second;

    qos_flow_setup_request_item = get_qos_flow_setup_request_item(qos_flow);
    qos_flow_setup_request_list_vector.push_back(qos_flow_setup_request_item);

    Logger::smf_n2().info(
        "QoS parameters: QFI %d, ARP priority level %d, "
        "qos_flow.qos_profile.arp.preempt_cap %s, "
        "qos_flow.qos_profile.arp.preempt_vuln %s",
        qos_flow.qfi.qfi, qos_flow.qos_profile.getArp().getPriorityLevel(),
        qos_flow.qos_profile.getArp().getPreemptCap().getEnumString(),
        qos_flow.qos_profile.getArp().getPreemptVuln().getEnumString());

    i++;
  }

  qos_flow_setup_request_list.set(qos_flow_setup_request_list_vector);
  pdu_session_resource_setup_request_transfer.setQosFlowSetupRequestList(
      qos_flow_setup_request_list);

  return true;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_setup_request_transfer(
    pdu_session_create_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  Logger::smf_n2().info(
      "Create N2 SM Information, PDU Session Resource Setup Request Transfer");

  bool result = false;
  PduSessionResourceSetupRequestTransfer
      pdu_session_resource_setup_request_transfer = {};

  // get QoS flows
  std::map<uint8_t, qos_flow_context_updated> qos_flows = {};
  sm_context_res.get_all_qos_flow_context_created(qos_flows);

  std::shared_ptr<pdu_session_create_sm_context_response> sm_context_response =
      std::make_shared<pdu_session_create_sm_context_response>(sm_context_res);
  if (!create_n2_pdu_session_resource_setup_request_transfer(
          sm_context_response, qos_flows, ngap_info_type,
          pdu_session_resource_setup_request_transfer)) {
    Logger::smf_n2().warn(
        "Couldn't fill NGAP PDU Session Resource Setup Request Transfer "
        "contents");
    return false;
  }

  // Encode
  // TODO: get actual message length
  auto buffer = new (std::nothrow) uint8_t[BUF_LEN]();
  if (buffer == nullptr) {
    Logger::smf_n2().error("Error when allocating buffer!");
    return false;
  }

  int encoded_size = 0;
  pdu_session_resource_setup_request_transfer.encode2NewBuffer(
      buffer, encoded_size);

  if (encoded_size < 0) {
    Logger::smf_n2().warn(
        "NGAP PDU Session Resource Setup Request Transfer encode failed "
        "(encode size %d)",
        encoded_size);
    result = false;
  } else {
    oai::utils::output_wrapper::print_buffer(
        {}, "N2 SM Buffer Data:", buffer, encoded_size);

    std::string ngap_message((char*) buffer, encoded_size);
    ngap_msg_str = ngap_message;
    result       = true;
  }

  return result;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_setup_request_transfer(
    pdu_session_update_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  Logger::smf_n2().info(
      "Create N2 SM Information, PDU Session Resource Setup Request Transfer");

  bool result = false;

  PduSessionResourceSetupRequestTransfer
      pdu_session_resource_setup_request_transfer = {};

  // get QoS flows
  std::map<uint8_t, qos_flow_context_updated> qos_flows = {};
  sm_context_res.get_all_qos_flow_context_updateds(qos_flows);

  for (std::map<uint8_t, qos_flow_context_updated>::iterator it =
           qos_flows.begin();
       it != qos_flows.end(); ++it)
    Logger::smf_n2().debug("QoS Flow context to be updated QFI %d", it->first);

  if (qos_flows.empty()) {
    return false;
  }

  std::shared_ptr<pdu_session_update_sm_context_response> sm_context_response =
      std::make_shared<pdu_session_update_sm_context_response>(sm_context_res);
  if (!create_n2_pdu_session_resource_setup_request_transfer(
          sm_context_response, qos_flows, ngap_info_type,
          pdu_session_resource_setup_request_transfer)) {
    Logger::smf_n2().warn(
        "Couldn't fill NGAP PDU Session Resource Setup Request Transfer "
        "contents");
    return false;
  }

  // Encode
  // TODO: get actual message length
  auto buffer = new (std::nothrow) uint8_t[BUF_LEN]();
  if (buffer == nullptr) {
    Logger::smf_n2().error("Error when allocating buffer!");
    return false;
  }

  int encoded_size = 0;
  pdu_session_resource_setup_request_transfer.encode2NewBuffer(
      buffer, encoded_size);

  if (encoded_size < 0) {
    Logger::smf_n2().warn(
        "NGAP PDU Session Resource Setup Request Transfer encode failed "
        "(encode size %d)",
        encoded_size);
    result = false;
  } else {
    oai::utils::output_wrapper::print_buffer(
        {}, "N2 SM Buffer Data:", buffer, encoded_size);

    std::string ngap_message((char*) buffer, encoded_size);
    ngap_msg_str = ngap_message;
    result       = true;
  }

  return result;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_modify_request_transfer(
    const std::shared_ptr<pdu_session_sm_context_response>& sm_context_res,
    const std::map<uint8_t, qos_flow_context_updated>& qos_flows,
    n2_sm_info_type_e ngap_info_type,
    PduSessionResourceModifyRequestTransfer&
        pdu_session_resource_modify_request_transfer) {
  Logger::smf_n2().info(
      "Create N2 SM Information, PDU Session Resource Modify Request Transfer");

  bool result = false;

  for (const auto& qos_flow_pair : qos_flows) {
    auto qos_flow = qos_flow_pair.second;
    Logger::smf_n2().debug(
        "UL F-TEID, TEID "
        "0x%" PRIx32 ", IP Address %s",
        qos_flow.ul_fteid.teid,
        oai::utils::conv::toString(qos_flow.ul_fteid.ipv4_address).c_str());
    Logger::smf_n2().info(
        "QoS parameters: QFI %d, 5QI: %d, Priority level %d, ARP priority "
        "level %d",
        qos_flow.qfi.qfi, qos_flow.qos_profile.getR5qi(),
        qos_flow.qos_profile.getPriorityLevel(),
        qos_flow.qos_profile.getArp().getPriorityLevel());

    // check the QoS Flow
    if ((qos_flow.qfi.qfi < QOS_FLOW_IDENTIFIER_FIRST) or
        (qos_flow.qfi.qfi > QOS_FLOW_IDENTIFIER_LAST)) {
      // error
      Logger::smf_n2().error("Incorrect QFI %d", qos_flow.qfi.qfi);
      return false;
    }
  }

  // PDUSessionAggregateMaximumBitRate
  PduSessionAggregateMaximumBitRate pdu_session_aggregate_maximum_bit_rate = {};
  supi_t supi                     = sm_context_res->get_supi();
  supi64_t supi64                 = smf_supi_to_u64(supi);
  std::shared_ptr<smf_context> sc = {};
  if (smf_app_inst->is_supi_2_smf_context(supi64)) {
    Logger::smf_n2().debug("Get SMF context with SUPI " SUPI_64_FMT "", supi64);
    sc = smf_app_inst->supi_2_smf_context(supi64);
    oai::nas::SessionAmbr session_ambr = {};
    sc.get()->get_session_ambr(
        session_ambr, sm_context_res->get_snssai(), sm_context_res->get_dnn());
    uint64_t bit_rate_dl = session_handler::parse_nas_value_unit_to_bps(
        session_ambr.GetSessionAmbrForDownlink(),
        session_ambr.GetUnitForDownlink());
    uint64_t bit_rate_ul = session_handler::parse_nas_value_unit_to_bps(
        session_ambr.GetSessionAmbrForUplink(),
        session_ambr.GetUnitForUplink());
    pdu_session_aggregate_maximum_bit_rate.set(bit_rate_dl, bit_rate_ul);
  } else {
    Logger::smf_n2().warn(
        "SMF context with SUPI " SUPI_64_FMT " does not exist!", supi64);
    return false;
  }
  pdu_session_resource_modify_request_transfer
      .setPduSessionAggregateMaximumBitRate(
          pdu_session_aggregate_maximum_bit_rate);

  // UL NG-U UP TNL Modify List
  // Ngap_UL_NGU_UP_TNLModifyList_t (included if the PDU Session modification
  // was requested by the UE for a  PDU Session that has no established User
  // Plane resources)
  // TODO: only first flow is considered for now
  pfcp::fteid_t ul_fteid = {};
  ul_fteid.v4            = qos_flows.begin()->second.ul_fteid.v4;
  ul_fteid.teid          = qos_flows.begin()->second.ul_fteid.teid;
  ul_fteid.ipv4_address  = qos_flows.begin()->second.ul_fteid.ipv4_address;

  pfcp::fteid_t dl_fteid = {};
  dl_fteid.v4            = qos_flows.begin()->second.dl_fteid.v4;
  dl_fteid.teid          = qos_flows.begin()->second.dl_fteid.teid;
  dl_fteid.ipv4_address  = qos_flows.begin()->second.dl_fteid.ipv4_address;

  UlNgUUpTnlModifyList ul_ng_u_up_tnl_modify_list = {};
  std::vector<UlNgUUpTnlModifyItem> ul_ngu_up_tnl_modify_item_list;
  UlNgUUpTnlModifyItem ul_ngu_up_tnl_modify_item = {};

  UpTransportLayerInformation ul_ng_u_up_tnl_information_ul = {};
  TransportLayerAddress transport_layer_address_ul          = {};
  GtpTeid gtp_teid_ul                                       = {};
  transport_layer_address_ul.setIpv4Address(ul_fteid.ipv4_address);
  gtp_teid_ul.set(ul_fteid.teid);
  ul_ng_u_up_tnl_information_ul.set(transport_layer_address_ul, gtp_teid_ul);

  UpTransportLayerInformation ul_ng_u_up_tnl_information_dl = {};
  TransportLayerAddress transport_layer_address_dl          = {};
  GtpTeid gtp_teid_dl                                       = {};
  transport_layer_address_dl.setIpv4Address(dl_fteid.ipv4_address);
  gtp_teid_dl.set(dl_fteid.teid);
  ul_ng_u_up_tnl_information_dl.set(transport_layer_address_dl, gtp_teid_dl);

  ul_ngu_up_tnl_modify_item.set(
      ul_ng_u_up_tnl_information_ul, ul_ng_u_up_tnl_information_dl);
  ul_ngu_up_tnl_modify_item_list.push_back(ul_ngu_up_tnl_modify_item);
  ul_ng_u_up_tnl_modify_list.set(ul_ngu_up_tnl_modify_item_list);

  pdu_session_resource_modify_request_transfer.setUlNgUUpTnlModifyList(
      ul_ng_u_up_tnl_modify_list);

  // TODO: Network Instance (Optional)

  // QoS Flow Add or Modify Request List
  QosFlowAddOrModifyRequestList qos_flow_list = {};
  QosFlowAddOrModifyRequestItem qos_flow_item = {};
  for (const auto& qos_flow_pair : qos_flows) {
    auto qos_flow                         = qos_flow_pair.second;
    QosFlowIdentifier qos_flow_identifier = {};
    qos_flow_identifier.set(qos_flow.qfi.qfi);
    QosFlowLevelQosParameters qos_parameters =
        get_qos_flow_level_qos_parameters(qos_flow);
    qos_flow_item.setQosFlowIdentifier(qos_flow_identifier);
    qos_flow_item.setQosFlowLevelQosParameters(qos_parameters);
    qos_flow_list.addItem(qos_flow_item);
  }
  pdu_session_resource_modify_request_transfer.setQosFlowAddOrModifyRequestList(
      qos_flow_list);

  // TODO: QoS Flow to Release List (Optional)
  // TODO: Additional UL NG-U UP TNL Information (Optional)
  // TODO: Common Network Instance (Optional)
  // TODO: Additional Redundant UL NG-U UP TNL Information (Optional)
  // TODO: Redundant Common Network Instance (Optional)
  // TODO: Redundant UL NG-U UP TNL Information (Optional)
  // TODO: Security Indication (Optional)

  return true;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_modify_request_transfer(
    pdu_session_update_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  Logger::smf_n2().debug(
      "Create N2 SM Information: NGAP PDU Session Resource Modify Request "
      "Transfer");

  bool result = false;

  PduSessionResourceModifyRequestTransfer
      pdu_session_resource_modify_request_transfer = {};

  // get default QoS info
  std::map<uint8_t, qos_flow_context_updated> qos_flows = {};
  sm_context_res.get_all_qos_flow_context_updateds(qos_flows);
  if (qos_flows.empty()) {
    // Should not be empty, but could be because delete existing qos rule is not
    // supported yet
    Logger::smf_n2().error("QoS flow context to be updated list is empty");
    return false;
  }

  std::shared_ptr<pdu_session_update_sm_context_response> sm_context_response =
      std::make_shared<pdu_session_update_sm_context_response>(sm_context_res);
  if (!create_n2_pdu_session_resource_modify_request_transfer(
          sm_context_response, qos_flows, ngap_info_type,
          pdu_session_resource_modify_request_transfer)) {
    Logger::smf_n2().warn(
        "Couldn't fill NGAP PDU Session Resource Modify Request Transfer "
        "contents");
    return false;
  }

  // Encode
  uint8_t buffer[BUF_LEN];  // TODO: get actual message length
  int encoded_size =
      pdu_session_resource_modify_request_transfer.encode(buffer, BUF_LEN);

  if (encoded_size < 0) {
    Logger::smf_n2().warn(
        "NGAP PDU Session Resource Setup Modify Transfer encode failed "
        "(encode size %d)",
        encoded_size);
    result = false;
  } else {
    oai::utils::output_wrapper::print_buffer(
        {}, "N2 SM Buffer Data:", buffer, encoded_size);

    std::string ngap_message((char*) buffer, encoded_size);
    ngap_msg_str = ngap_message;
    result       = true;
  }

  return result;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_release_command_transfer(
    const oai::ngap::Cause& cause, n2_sm_info_type_e ngap_info_type,
    std::string& ngap_msg_str) {
  Logger::smf_n2().debug(
      "Create N2 SM Information: NGAP PDU Session Resource Release Command "
      "Transfer");
  bool result = false;

  PduSessionResourceReleaseCommandTransfer
      pdu_session_resource_release_command_transfer = {};
  pdu_session_resource_release_command_transfer.setCause(cause);

  // Encode
  // TODO: get actual message length
  auto buffer = new (std::nothrow) uint8_t[BUF_LEN]();
  if (buffer == nullptr) {
    Logger::smf_n2().error("Error when allocating buffer!");
    return false;
  }

  int encoded_size = 0;
  pdu_session_resource_release_command_transfer.encode2NewBuffer(
      buffer, encoded_size);

  if (encoded_size < 0) {
    Logger::smf_n2().warn(
        "NGAP PDU Session Resource Setup Release Command Transfer encode "
        "failed "
        "(encode size %d)",
        encoded_size);
    result = false;
  } else {
    oai::utils::output_wrapper::print_buffer(
        {}, "N2 SM Buffer Data:", buffer, encoded_size);

    std::string ngap_message((char*) buffer, encoded_size);
    ngap_msg_str = ngap_message;
    result       = true;
  }

  return result;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_setup_request_transfer(
    pdu_session_report_response& msg, n2_sm_info_type_e ngap_info_type,
    std::string& ngap_msg_str) {
  Logger::smf_n2().debug(
      "Create N2 SM Information: NGAP PDU Session Resource Setup Request "
      "Transfer IE");
  // TODO:
  Logger::smf_n2().warn("This function has not been implemented!");

  return true;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_modify_request_transfer(
    pdu_session_modification_network_requested& msg,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  Logger::smf_n2().debug(
      "Create N2 SM Information: NGAP PDU Session Resource Modify Request "
      "Transfer IE");
  // TODO:
  Logger::smf_n2().warn("This function has not been implemented!");

  return true;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_modify_response_transfer(
    pdu_session_update_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  // PDU Session Resource Modify Response Transfer IE for testing purpose
  Logger::smf_n2().debug(
      "Create N2 SM Information: NGAP PDU Session Resource Modify Response "
      "Transfer IE");
  // TODO:
  Logger::smf_n2().warn("This function has not been implemented!");

  return true;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_path_switch_request_ack(
    pdu_session_update_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  // Path Switch Request Acknowledge Transfer IE
  Logger::smf_n2().debug(
      "Create N2 SM Information: Path Switch Request Acknowledge Transfer IE");
  bool result = false;

  PathSwitchRequestAcknowledgeTransfer path_switch_req_ack = {};

  // TODO:
  pfcp::fteid_t ul_fteid            = {};
  qos_flow_context_updated qos_flow = {};

  // get QoS values
  std::map<uint8_t, qos_flow_context_updated> qos_flows = {};
  sm_context_res.get_all_qos_flow_context_updateds(qos_flows);
  for (std::map<uint8_t, qos_flow_context_updated>::iterator it =
           qos_flows.begin();
       it != qos_flows.end(); ++it)
    Logger::smf_n2().debug("QoS Flow context to be updated QFI %d", it->first);

  if (qos_flows.empty()) {
    return false;
  }

  // TODO: only first flow is considered for now
  ul_fteid = qos_flows.begin()->second.ul_fteid;

  UpTransportLayerInformation ul_ng_u_up_tnl_information = {};
  TransportLayerAddress transport_layer_address          = {};
  GtpTeid gtp_teid                                       = {};
  transport_layer_address.setIpv4Address(ul_fteid.ipv4_address);
  gtp_teid.set(ul_fteid.teid);
  ul_ng_u_up_tnl_information.set(transport_layer_address, gtp_teid);
  path_switch_req_ack.setUlNgUUpTnlInformation(ul_ng_u_up_tnl_information);

  // TODO: Security Indication
  // TODO: Additional NG-U UP TNL Information
  // TODO: Redundant UL NG-U UP TNL Information
  // TODO: Additional Redundant NG-U UP TNL Information
  // TODO: QoS Flow Parameters List

  // Encode
  uint8_t buffer[BUF_LEN];  // TODO: get actual message length
  int encoded_size = path_switch_req_ack.encode(buffer, BUF_LEN);

  if (encoded_size < 0) {
    Logger::smf_n2().warn(
        "NGAP Path Switch Request Acknowledge Transfer encode failed "
        "(encode size %d)",
        encoded_size);
    result = false;
  } else {
    oai::utils::output_wrapper::print_buffer(
        {}, "N2 SM Buffer Data:", buffer, encoded_size);

    std::string ngap_message((char*) buffer, encoded_size);
    ngap_msg_str = ngap_message;
    result       = true;
  }

  return result;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_handover_command_transfer(
    pdu_session_update_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  // Handover Command Transfer IE
  Logger::smf_n2().debug(
      "Create N2 SM Information: Handover Command Transfer IE");
  bool result = false;

  HandoverCommandTransfer handover_command_transfer = {};

  // TODO:
  pfcp::fteid_t ul_fteid            = {};
  qos_flow_context_updated qos_flow = {};

  // get QoS values
  std::map<uint8_t, qos_flow_context_updated> qos_flows = {};
  sm_context_res.get_all_qos_flow_context_updateds(qos_flows);
  for (std::map<uint8_t, qos_flow_context_updated>::iterator it =
           qos_flows.begin();
       it != qos_flows.end(); ++it)
    Logger::smf_n2().debug("QoS Flow context to be updated QFI %d", it->first);

  if (qos_flows.empty()) {
    return false;
  }

  // TODO: only first flow is considered for now
  ul_fteid = qos_flows.begin()->second.ul_fteid;

  UpTransportLayerInformation dl_forwarding_up_tnl_information = {};
  TransportLayerAddress transport_layer_address                = {};
  GtpTeid gtp_teid                                             = {};
  transport_layer_address.setIpv4Address(ul_fteid.ipv4_address);
  gtp_teid.set(ul_fteid.teid);
  dl_forwarding_up_tnl_information.set(transport_layer_address, gtp_teid);
  handover_command_transfer.setDlForwardingUpTnlInformation(
      dl_forwarding_up_tnl_information);

  // TODO: QoS Flow to be Forwarded List
  // TODO: Data Forwarding Response DRB List (Optional)
  // TODO: Additional DL Forwarding UP TNL Information (Optional)
  // TODO: UL Forwarding UP TNL Information (Optional)
  // TODO: Additional UL Forwarding UP TNL Information (Optional)
  // TODO: Data Forwarding Response E-RAB List (Optional)
  // TODO: QoS Flow Failed to Setup List (Optional)

  // Encode
  uint8_t buffer[BUF_LEN];  // TODO: get actual message length
  int encoded_size = handover_command_transfer.encode(buffer, BUF_LEN);

  if (encoded_size < 0) {
    Logger::smf_n2().warn(
        "NGAP Handover Command Transfer encode failed "
        "(encode size %d)",
        encoded_size);
    result = false;
  } else {
    oai::utils::output_wrapper::print_buffer(
        {}, "N2 SM Buffer Data:", buffer, encoded_size);

    std::string ngap_message((char*) buffer, encoded_size);
    ngap_msg_str = ngap_message;
    result       = true;
  }

  return result;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_handover_preparation_unsuccessful_transfer(
    pdu_session_update_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  // Handover Preparation Unsuccessful Transfer IE
  Logger::smf_n2().debug(
      "Create N2 SM Information: Handover Preparation Unsuccessful Transfer "
      "IE");
  bool result = false;

  HandoverPreparationUnsuccessfulTransfer
      handover_preparation_unsuccessful_transfer = {};
  Cause cause                                    = {};
  // TODO: Just an example
  cause.set(
      Ngap_CauseRadioNetwork_ho_target_not_allowed, Ngap_Cause_PR_radioNetwork);
  handover_preparation_unsuccessful_transfer.setCause(cause);

  // Encode
  uint8_t buffer[BUF_LEN];  // TODO: get actual message length
  int encoded_size =
      handover_preparation_unsuccessful_transfer.encode(buffer, BUF_LEN);

  if (encoded_size < 0) {
    Logger::smf_n2().warn(
        "NGAP Handover Preparation Unsuccessful Transfer encode failed "
        "(encode size %d)",
        encoded_size);
    result = false;
  } else {
    oai::utils::output_wrapper::print_buffer(
        {}, "N2 SM Buffer Data:", buffer, encoded_size);

    std::string ngap_message((char*) buffer, encoded_size);
    ngap_msg_str = ngap_message;
    result       = true;
  }

  return result;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<PduSessionResourceSetupResponseTransfer>& ngap_ie,
    const std::string& n2_sm_info) {
  Logger::smf_n2().info(
      "Decode NGAP message (PDUSessionResourceSetupResponseTransfer) from N2 "
      "SM Information");
  int result = KEncodeDecodeOK;

  unsigned int data_len = n2_sm_info.length();
  unsigned char* data   = (unsigned char*) malloc(data_len + 1);
  memset(data, 0, data_len + 1);
  memcpy((void*) data, (void*) n2_sm_info.c_str(), data_len);

  oai::utils::output_wrapper::print_buffer({}, "Content:", data, data_len);

  if (!ngap_ie->decode(data, data_len)) {
    Logger::smf_n2().warn(
        "Decode PDUSessionResourceSetupResponseTransfer failed");
    result = KEncodeDecodeError;
  }

  // free memory
  oai::utils::utils::free_wrapper((void**) &data);
  return result;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<PduSessionResourceModifyResponseTransfer>& ngap_ie,
    const std::string& n2_sm_info) {
  Logger::smf_n2().info(
      "Decode NGAP message (PDUSessionResourceModifyResponseTransfer) "
      "from N2 SM Information");
  int result = KEncodeDecodeOK;

  unsigned int data_len = n2_sm_info.length();
  unsigned char* data   = (unsigned char*) malloc(data_len + 1);
  memset(data, 0, data_len + 1);
  memcpy((void*) data, (void*) n2_sm_info.c_str(), data_len);

  oai::utils::output_wrapper::print_buffer({}, "Content:", data, data_len);

  // PDUSessionResourceModifyResponseTransfer
  if (!ngap_ie->decode(data, data_len)) {
    Logger::smf_n2().warn(
        "Decode PDUSessionResourceSetupResponseTransfer failed");
    result = KEncodeDecodeError;
  }
  // free memory
  oai::utils::utils::free_wrapper((void**) &data);

  return KEncodeDecodeOK;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<PduSessionResourceReleaseResponseTransfer>& ngap_ie,
    const std::string& n2_sm_info) {
  Logger::smf_n2().info(
      "Decode NGAP message (PDUSessionResourceReleaseResponseTransfer) "
      "from N2 SM Information");

  int result            = KEncodeDecodeOK;
  unsigned int data_len = n2_sm_info.length();
  unsigned char* data   = (unsigned char*) malloc(data_len + 1);
  memset(data, 0, data_len + 1);
  memcpy((void*) data, (void*) n2_sm_info.c_str(), data_len);

  if (!ngap_ie->decode(data, data_len)) {
    Logger::smf_n2().warn(
        "Decode PDUSessionResourceSetupResponseTransfer failed");
    result = KEncodeDecodeError;
  }
  // free memory
  oai::utils::utils::free_wrapper((void**) &data);

  return result;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<oai::ngap::PduSessionResourceSetupUnsuccessfulTransfer>&
        ngap_ie,
    const std::string& n2_sm_info) {
  Logger::smf_n2().info(
      "Decode NGAP message (PDUSessionResourceSetupUnsuccessfulTransfer) "
      "from N2 SM Information");

  int result            = KEncodeDecodeOK;
  unsigned int data_len = n2_sm_info.length();
  unsigned char* data   = (unsigned char*) malloc(data_len + 1);
  memset(data, 0, data_len + 1);
  memcpy((void*) data, (void*) n2_sm_info.c_str(), data_len);

  if (!ngap_ie->decode(data, data_len)) {
    Logger::smf_n2().warn(
        "Decode PDUSessionResourceSetupUnsuccessfulTransfer failed");
    result = KEncodeDecodeError;
  }
  // free memory
  oai::utils::utils::free_wrapper((void**) &data);

  return result;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<PathSwitchRequestTransfer>& ngap_ie,
    const std::string& n2_sm_info) {
  Logger::smf_n2().info(
      "Decode NGAP message (PathSwitchRequestTransfer) "
      "from N2 SM Information");

  int result            = KEncodeDecodeOK;
  unsigned int data_len = n2_sm_info.length();
  unsigned char* data   = (unsigned char*) malloc(data_len + 1);
  memset(data, 0, data_len + 1);
  memcpy((void*) data, (void*) n2_sm_info.c_str(), data_len);

  if (!ngap_ie->decode(data, data_len)) {
    Logger::smf_n2().warn("Decode PathSwitchRequestTransfer failed");
    result = KEncodeDecodeError;
  }
  // free memory
  oai::utils::utils::free_wrapper((void**) &data);

  return result;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<HandoverRequiredTransfer>& ngap_ie,
    const std::string& n2_sm_info) {
  Logger::smf_n2().info(
      "Decode NGAP message (HandoverRequiredTransfer) "
      "from N2 SM Information");

  int result            = KEncodeDecodeOK;
  unsigned int data_len = n2_sm_info.length();
  unsigned char* data   = (unsigned char*) malloc(data_len + 1);
  memset(data, 0, data_len + 1);
  memcpy((void*) data, (void*) n2_sm_info.c_str(), data_len);

  if (!ngap_ie->decode(data, data_len)) {
    Logger::smf_n2().warn("Decode HandoverRequiredTransfer failed");
    result = KEncodeDecodeError;
  }
  // free memory
  oai::utils::utils::free_wrapper((void**) &data);

  return result;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<HandoverRequestAcknowledgeTransfer>& ngap_ie,
    const std::string& n2_sm_info) {
  Logger::smf_n2().info(
      "Decode NGAP message (HandoverRequestAcknowledgeTransfer) "
      "from N2 SM Information");

  int result            = KEncodeDecodeOK;
  unsigned int data_len = n2_sm_info.length();
  unsigned char* data   = (unsigned char*) malloc(data_len + 1);
  memset(data, 0, data_len + 1);
  memcpy((void*) data, (void*) n2_sm_info.c_str(), data_len);

  if (!ngap_ie->decode(data, data_len)) {
    Logger::smf_n2().warn("Decode HandoverRequestAcknowledgeTransfer failed");
    result = KEncodeDecodeError;
  }
  // free memory
  oai::utils::utils::free_wrapper((void**) &data);

  return result;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<HandoverResourceAllocationUnsuccessfulTransfer>& ngap_ie,
    const std::string& n2_sm_info) {
  Logger::smf_n2().info(
      "Decode NGAP message "
      "(HandoverResourceAllocationUnsuccessfulTransfer) "
      "from N2 SM Information");

  int result            = KEncodeDecodeOK;
  unsigned int data_len = n2_sm_info.length();
  unsigned char* data   = (unsigned char*) malloc(data_len + 1);
  memset(data, 0, data_len + 1);
  memcpy((void*) data, (void*) n2_sm_info.c_str(), data_len);

  if (!ngap_ie->decode(data, data_len)) {
    Logger::smf_n2().warn(
        "Decode HandoverResourceAllocationUnsuccessfulTransfer failed");
    result = KEncodeDecodeError;
  }
  // free memory
  oai::utils::utils::free_wrapper((void**) &data);

  return result;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<SecondaryRatDataUsageReportTransfer>& ngap_ie,
    const std::string& n2_sm_info) {
  Logger::smf_n2().info(
      "Decode NGAP message "
      "(SecondaryRATDataUsageReportTransfer) "
      "from N2 SM Information");

  int result            = KEncodeDecodeOK;
  unsigned int data_len = n2_sm_info.length();
  unsigned char* data   = (unsigned char*) malloc(data_len + 1);
  memset(data, 0, data_len + 1);
  memcpy((void*) data, (void*) n2_sm_info.c_str(), data_len);

  if (!ngap_ie->decode(data, data_len)) {
    Logger::smf_n2().warn("Decode SecondaryRATDataUsageReportTransfer failed");
    result = KEncodeDecodeError;
  }
  // free memory
  oai::utils::utils::free_wrapper((void**) &data);

  return result;
}

//---------------------------------------------------------------------------------------------
oai::ngap::QosFlowSetupRequestItem smf_n2::get_qos_flow_setup_request_item(
    const qos_flow_context_updated& qos_flow) {
  QosFlowSetupRequestItem qos_flow_setup_request_item = {};

  QosFlowIdentifier qos_flow_identifier = {};
  qos_flow_identifier.set((uint8_t) qos_flow.qfi.qfi);

  QosFlowLevelQosParameters qos_flow_level_qos_parameters =
      get_qos_flow_level_qos_parameters(qos_flow);

  qos_flow_setup_request_item.set(
      qos_flow_identifier, qos_flow_level_qos_parameters);

  return qos_flow_setup_request_item;
}

//---------------------------------------------------------------------------------------------
oai::ngap::QosFlowLevelQosParameters smf_n2::get_qos_flow_level_qos_parameters(
    const qos_flow_context_updated& qos_flow) {
  QosFlowLevelQosParameters qos_flow_level_qos_parameters = {};

  // QosCharacteristics
  QosCharacteristics qos_characteristics             = {};
  NonDynamic5qiDescriptor non_dynamic_5qi_descriptor = {};
  FiveQI five_qi                                     = {};
  if (qos_flow.qos_profile.r5qiIsSet())
    five_qi.set(qos_flow.qos_profile.getR5qi());

  std::optional<PriorityLevelQos> priority_level_qos_opt = std::nullopt;
  if (qos_flow.qos_profile.priorityLevelIsSet()) {
    PriorityLevelQos priority_level_qos = {};
    priority_level_qos.set(qos_flow.qos_profile.getPriorityLevel());
    priority_level_qos_opt =
        std::make_optional<PriorityLevelQos>(priority_level_qos);
  }
  std::optional<AveragingWindow> averaging_window_opt = std::nullopt;
  if (qos_flow.qos_profile.averWindowIsSet()) {
    AveragingWindow averaging_window = {};
    averaging_window.set(qos_flow.qos_profile.getAverWindow());
    averaging_window_opt =
        std::make_optional<AveragingWindow>(averaging_window);
  }
  std::optional<MaximumDataBurstVolume> maximum_data_burst_volume_opt =
      std::nullopt;
  if (qos_flow.qos_profile.maxDataBurstVolIsSet()) {
    MaximumDataBurstVolume maximum_data_burst_volume = {};
    maximum_data_burst_volume.set(qos_flow.qos_profile.getMaxDataBurstVol());
    maximum_data_burst_volume_opt =
        std::make_optional<MaximumDataBurstVolume>(maximum_data_burst_volume);
  }

  non_dynamic_5qi_descriptor.set(
      five_qi, priority_level_qos_opt, averaging_window_opt,
      maximum_data_burst_volume_opt);
  qos_characteristics.set(non_dynamic_5qi_descriptor);

  // AllocationAndRetentionPriority
  AllocationAndRetentionPriority allocation_and_retention_priority = {};
  PriorityLevelARP priority_level_arp                              = {};
  Pre_emptionCapability pre_emption_capability                     = {};
  Pre_emptionVulnerability pre_emption_vulnerability               = {};

  priority_level_arp.set(qos_flow.qos_profile.getArp().getPriorityLevel());

  auto preemptCapEnumValue =
      qos_flow.qos_profile.getArp().getPreemptCap().getEnumValue();
  if (preemptCapEnumValue ==
      PreemptionCapability_anyOf::ePreemptionCapability_anyOf::NOT_PREEMPT) {
    pre_emption_capability.set(
        Ngap_Pre_emptionCapability_shall_not_trigger_pre_emption);
  } else {
    pre_emption_capability.set(
        Ngap_Pre_emptionCapability_may_trigger_pre_emption);
  }

  auto preemptVulnEnumValue =
      qos_flow.qos_profile.getArp().getPreemptVuln().getEnumValue();
  if (preemptVulnEnumValue ==
      PreemptionVulnerability_anyOf::ePreemptionVulnerability_anyOf::
          NOT_PREEMPTABLE) {
    pre_emption_vulnerability.set(
        Ngap_Pre_emptionVulnerability_not_pre_emptable);
  } else {
    pre_emption_vulnerability.set(Ngap_Pre_emptionVulnerability_pre_emptable);
  }

  allocation_and_retention_priority.set(
      priority_level_arp, pre_emption_capability, pre_emption_vulnerability);

  // GbrQosFlowInformation
  std::optional<GbrQosFlowInformation> gbr_qos_flow_information_opt =
      std::nullopt;

  // TODO: from old code, need to be fixed: check the if condition is okay.
  if (qos_flow.qos_profile.gbrUlIsSet() && qos_flow.qos_profile.gbrDlIsSet() &&
      qos_flow.qos_profile.maxbrUlIsSet() &&
      qos_flow.qos_profile.maxbrDlIsSet()) {
    long maximum_flow_bit_rate_dl;
    long maximum_flow_bit_rate_ul;
    long guaranteed_flow_bit_rate_dl;
    long guaranteed_flow_bit_rate_ul;
    GbrQosFlowInformation gbr_qos_flow_information = {};

    std::vector<oai::nas::QosFlowDescriptionParameter>
        qos_flow_description_parameter_list =
            qos_flow.get_qos_flow_descriptions().GetParametersList();

    for (int j = 0; j < qos_flow_description_parameter_list.size(); j++) {
      std::optional<BitRate> bit_rate =
          qos_flow_description_parameter_list[j].GetBitRate();
      if (bit_rate.has_value()) {
        uint8_t parameter_identifier =
            qos_flow_description_parameter_list[j].GetIdentifier();
        if (parameter_identifier ==
            oai::nas::kQosFlowDescriptionParameterIdentifierMfbrUplink) {
          maximum_flow_bit_rate_ul =
              session_handler::parse_nas_value_unit_to_bps(
                  bit_rate.value().value, bit_rate.value().unit);
        } else if (
            parameter_identifier ==
            oai::nas::kQosFlowDescriptionParameterIdentifierMfbrDownlink) {
          maximum_flow_bit_rate_dl =
              session_handler::parse_nas_value_unit_to_bps(
                  bit_rate.value().value, bit_rate.value().unit);
        } else if (
            parameter_identifier ==
            oai::nas::kQosFlowDescriptionParameterIdentifierGfbrUplink) {
          guaranteed_flow_bit_rate_ul =
              session_handler::parse_nas_value_unit_to_bps(
                  bit_rate.value().value, bit_rate.value().unit);
        } else if (
            parameter_identifier ==
            oai::nas::kQosFlowDescriptionParameterIdentifierGfbrDownlink) {
          guaranteed_flow_bit_rate_dl =
              session_handler::parse_nas_value_unit_to_bps(
                  bit_rate.value().value, bit_rate.value().unit);
        }
      }
    }

    gbr_qos_flow_information.set(
        maximum_flow_bit_rate_dl, maximum_flow_bit_rate_ul,
        guaranteed_flow_bit_rate_dl, guaranteed_flow_bit_rate_ul);
    gbr_qos_flow_information_opt =
        std::make_optional<GbrQosFlowInformation>(gbr_qos_flow_information);
  }

  qos_flow_level_qos_parameters.set(
      qos_characteristics, allocation_and_retention_priority,
      gbr_qos_flow_information_opt);
  return qos_flow_level_qos_parameters;
}

//---------------------------------------------------------------------------------------------
void smf_n2::set_ngap_bit_rate(
    Ngap_BitRate_t& bit_rate, uint16_t value, uint8_t unit) {
  bit_rate.size = 8;
  bit_rate.buf  = (uint8_t*) calloc(8, sizeof(uint8_t));
  uint64_t bit_rate_value =
      session_handler::parse_nas_value_unit_to_bps(value, unit);
  INT64_TO_BUFFER(bit_rate_value, bit_rate.buf);
}
