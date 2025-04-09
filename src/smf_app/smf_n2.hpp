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

#ifndef FILE_SMF_N2_HPP_SEEN
#define FILE_SMF_N2_HPP_SEEN

#include <Ngap_QosFlowSetupRequestItem.h>

#include <string>

#include "Cause.hpp"
#include "HandoverCommandTransfer.hpp"
#include "HandoverPreparationUnsuccessfulTransfer.hpp"
#include "HandoverRequestAcknowledgeTransfer.hpp"
#include "HandoverRequiredTransfer.hpp"
#include "HandoverResourceAllocationUnsuccessfulTransfer.hpp"
#include "PathSwitchRequestAcknowledgeTransfer.hpp"
#include "PathSwitchRequestTransfer.hpp"
#include "PduSessionResourceModifyRequestTransfer.hpp"
#include "PduSessionResourceModifyResponseTransfer.hpp"
#include "PduSessionResourceReleaseCommandTransfer.hpp"
#include "PduSessionResourceReleaseResponseTransfer.hpp"
#include "PduSessionResourceSetupRequestTransfer.hpp"
#include "PduSessionResourceSetupResponseTransfer.hpp"
#include "PduSessionResourceSetupUnsuccessfulTransfer.hpp"
#include "QosFlowSetupRequestItem.hpp"
#include "SecondaryRatDataUsageReportTransfer.hpp"
#include "smf.h"
#include "smf_app.hpp"
#include "smf_msg.hpp"

namespace smf {

class smf_n2 {
 private:
  static void set_ngap_bit_rate(
      Ngap_BitRate_t& bit_rate, uint16_t value, uint8_t unit);

  static oai::ngap::QosFlowSetupRequestItem get_qos_flow_setup_request_item(
      const qos_flow_context_updated& qos_flow);

  static oai::ngap::QosFlowLevelQosParameters get_qos_flow_level_qos_parameters(
      const qos_flow_context_updated& qos_flow);

 public:
  smf_n2(){};
  smf_n2(smf_n2 const&) = delete;
  void operator=(smf_n2 const&) = delete;

 public:
  static smf_n2& get_instance() {
    static smf_n2 instance;
    return instance;
  }

  /*
   * Create N2 SM Information: PDU Session Resource Setup Request Transfer
   * This IE is included in N1N2MessageTransfer Request (Accept, PDU Session
   * Establishment procedure - UE initiated)
   * @param [      const std::shared_ptr<pdu_session_sm_context_response>&]
   * sm_context_res: include necessary information for encoding NGAP msg
   * @param [const std::map<uint8_t, qos_flow_context_updated>&] qos_flows: QoS
   * flows info
   * @param [n2_sm_info_type_e] ngap_info_type: NGAP info's type
   * @param [oai::ngap::PduSessionResourceSetupRequestTransfer&]
   * pdu_session_resource_setup_request_transfer:
   * PduSessionResourceSetupRequestTransfer
   * @return boolean: True if the NGAP message has been created successfully,
   * otherwise return false
   *
   */
  bool create_n2_pdu_session_resource_setup_request_transfer(
      const std::shared_ptr<pdu_session_sm_context_response>& sm_context_res,
      const std::map<uint8_t, qos_flow_context_updated>& qos_flows,
      n2_sm_info_type_e ngap_info_type,
      oai::ngap::PduSessionResourceSetupRequestTransfer&
          pdu_session_resource_setup_request_transfer);

  /*
   * Create N2 SM Information: PDU Session Resource Setup Request Transfer
   * This IE is included in N1N2MessageTransfer Request (Accept, PDU Session
   * Establishment procedure - UE initiated)
   * @param [pdu_session_create_sm_context_response&] sm_context_res: include
   * necessary information for encoding NGAP msg
   * @param [n2_sm_info_type_e] ngap_info_type: NGAP info's type
   * @param [std::string&] ngap_msg_str store the created NGAP message in form
   * of string
   * @return boolean: True if the NGAP message has been created successfully,
   * otherwise return false
   *
   */
  bool create_n2_pdu_session_resource_setup_request_transfer(
      pdu_session_create_sm_context_response& sm_context_res,
      n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str);

  /*
   * Create N2 SM Information: PDU Session Resource Setup Request Transfer
   * This IE is included in PDU Session Update SM Context Response​ (Service
   * Request, step 2)
   * @param [pdu_session_update_sm_context_response&] sm_context_res: include
   * necessary information for encoding NGAP msg
   * @param [n2_sm_info_type_e] ngap_info_type: NGAP info's type
   * @param [std::string&] ngap_msg_str store the created NGAP message in form
   * of string
   * @return boolean: True if the NGAP message has been created successfully,
   * otherwise return false
   *
   */
  bool create_n2_pdu_session_resource_setup_request_transfer(
      pdu_session_update_sm_context_response& sm_context_res,
      n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str);

  /*
   * Create N2 SM Information: PDU Session Resource Setup Request Transfer
   * This IE is included in N1N2MessageTranfer (N4 Data Report)
   * @param [pdu_session_report_response] msg: include necessary information for
   * encoding NGAP msg
   * @param [std::string&] ngap_msg_str store the created NGAP message in form
   * of string
   * @return boolean: True if the NGAP message has been created successfully,
   * otherwise return false
   *
   */
  bool create_n2_pdu_session_resource_setup_request_transfer(
      pdu_session_report_response& msg, n2_sm_info_type_e ngap_info_type,
      std::string& ngap_msg_str);

  /*
   * Create N2 SM Information: PDU Session Resource Modify Request Transfer IE
   * This IE is included in  PDU Session Update SM Context Response (PDU Session
   * Modification procedure, UE-initiated, step 1)
   * @param [      const std::shared_ptr<pdu_session_sm_context_response>&]
   * sm_context_res: include necessary information for encoding NGAP msg
   * @param [const std::map<uint8_t, qos_flow_context_updated>&] qos_flows: QoS
   * flows info
   * @param [n2_sm_info_type_e] ngap_info_type: NGAP info's type
   * @param [oai::ngap::PduSessionResourceModifyRequestTransfer&]
   * pdu_session_resource_modify_request_transfer:
   * PduSessionResourceModifyRequestTransfer
   * @return boolean: True if the NGAP message has been created successfully,
   * otherwise return false
   *
   */
  bool create_n2_pdu_session_resource_modify_request_transfer(
      const std::shared_ptr<pdu_session_sm_context_response>& sm_context_res,
      const std::map<uint8_t, qos_flow_context_updated>& qos_flows,
      n2_sm_info_type_e ngap_info_type,
      oai::ngap::PduSessionResourceModifyRequestTransfer&
          pdu_session_resource_modify_request_transfer);

  /*
   * Create N2 SM Information: PDU Session Resource Modify Request Transfer IE
   * This IE is included in  PDU Session Update SM Context Response (PDU Session
   * Modification procedure, UE-initiated, step 1)
   * @param [pdu_session_update_sm_context_response] sm_context_res: include
   * necessary information for encoding NGAP msg
   * @param [n2_sm_info_type_e] ngap_info_type: NGAP info's type
   * @param [std::string&] ngap_msg_str store the created NGAP message in form
   * of string
   * @return boolean: True if the NGAP message has been created successfully,
   * otherwise return false
   *
   */
  bool create_n2_pdu_session_resource_modify_request_transfer(
      pdu_session_update_sm_context_response& sm_context_res,
      n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str);

  /*
   * Create N2 SM Information: PDU Session Resource Modify Request Transfer IE
   * This IE is included in  N1N2MessageTransfer Request (PDU Session
   * Modification procedure, SMF-requested, step 1)
   * @param [pdu_session_update_sm_context_response] sm_context_res: include
   * necessary information for encoding NGAP msg
   * @param [n2_sm_info_type_e] ngap_info_type: NGAP info's type
   * @param [std::string&] ngap_msg_str store the created NGAP message in form
   * of string
   * @return boolean: True if the NGAP message has been created successfully,
   * otherwise return false
   *
   */
  bool create_n2_pdu_session_resource_modify_request_transfer(
      pdu_session_modification_network_requested& msg,
      n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str);

  /*
   * Create N2 SM Information: PDU Session Resource Modify Response Transfer IE
   *
   * @param [pdu_session_update_sm_context_response] sm_context_res: include
   * necessary information for encoding NGAP msg
   * @param [std::string&] ngap_msg_str store the created NGAP message in form
   * of string
   * @param [n2_sm_info_type_e] ngap_info_type: NGAP info's type
   * @return boolean: True if the NGAP message has been created successfully,
   * otherwise return false
   *
   */
  bool create_n2_pdu_session_resource_modify_response_transfer(
      pdu_session_update_sm_context_response& sm_context_res,
      n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str);

  bool create_n2_pdu_session_resource_release_command_transfer(
      const oai::ngap::Cause& cause, n2_sm_info_type_e ngap_info_type,
      std::string& ngap_msg_str);

  bool create_n2_path_switch_request_ack(
      pdu_session_update_sm_context_response& sm_context_res,
      n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str);

  //------------------------------------------------------------------------------
  bool create_n2_handover_command_transfer(
      pdu_session_update_sm_context_response& sm_context_res,
      n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str);

  //------------------------------------------------------------------------------
  bool create_n2_handover_preparation_unsuccessful_transfer(
      pdu_session_update_sm_context_response& sm_context_res,
      n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str);

  /*
   * Decode N2 SM Information PDUSessionResourceModifyResponseTransfer
   * @param [std::shared_ptr<PduSessionResourceSetupResponseTransfer>&]
   * ngap_ie Store decoded NGAP message
   * @param [const std::string&] n2_sm_info N2 SM Information
   * @return status of the decode process
   */
  int decode_n2_sm_information(
      std::shared_ptr<oai::ngap::PduSessionResourceSetupResponseTransfer>&
          ngap_ie,
      const std::string& n2_sm_info);

  /*
   * Decode N2 SM Information PDUSessionResourceModifyResponseTransfer
   * @param [std::shared_ptr<PDUSessionResourceModifyResponseTransfer>&]
   * ngap_ie Store decoded NGAP message
   * @param [std::string&] n2_sm_info N2 SM Information
   * @return status of the decode process
   */
  int decode_n2_sm_information(
      std::shared_ptr<oai::ngap::PduSessionResourceModifyResponseTransfer>&
          ngap_ie,
      const std::string& n2_sm_info);

  /*
   * Decode N2 SM Information PDUSessionResourceReleaseResponseTransfer
   * @param [std::shared_ptr<PDUSessionResourceReleaseResponseTransfer>&]
   * ngap_ie Store decoded NGAP message
   * @param [std::string&] n2_sm_info N2 SM Information
   * @return status of the decode process
   */
  int decode_n2_sm_information(
      std::shared_ptr<oai::ngap::PduSessionResourceReleaseResponseTransfer>&
          ngap_ie,
      const std::string& n2_sm_info);

  int decode_n2_sm_information(
      std::shared_ptr<
          oai::ngap::HandoverResourceAllocationUnsuccessfulTransfer>& ngap_ie,
      const std::string& n2_sm_info);
  /*
   * Decode N2 SM Information PDUSessionResourceSetupUnsuccessfulTransfer
   * @param
   * [std::shared_ptr<PDUSessionResourceSetupUnsuccessfulTransfer>&]
   * ngap_ie Store decoded NGAP message
   * @param [std::string&] n2_sm_info N2 SM Information
   * @return status of the decode process
   */
  int decode_n2_sm_information(
      std::shared_ptr<oai::ngap::PduSessionResourceSetupUnsuccessfulTransfer>&
          ngap_ie,
      const std::string& n2_sm_info);

  int decode_n2_sm_information(
      std::shared_ptr<oai::ngap::PathSwitchRequestTransfer>& ngap_ie,
      const std::string& n2_sm_info);

  int decode_n2_sm_information(
      std::shared_ptr<oai::ngap::HandoverRequiredTransfer>& ngap_ie,
      const std::string& n2_sm_info);

  int decode_n2_sm_information(
      std::shared_ptr<oai::ngap::HandoverRequestAcknowledgeTransfer>& ngap_ie,
      const std::string& n2_sm_info);

  int decode_n2_sm_information(
      std::shared_ptr<oai::ngap::SecondaryRatDataUsageReportTransfer>& ngap_ie,
      const std::string& n2_sm_info);
};

}  // namespace smf

#endif /* FILE_SMF_N2_HPP_SEEN */
