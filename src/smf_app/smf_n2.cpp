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

/*! \file smf_n2.cpp
 \brief
 \author  Tien-Thinh NGUYEN, Keliang DU
 \company Eurecom
 \date 2019
 \email:  tien-thinh.nguyen@eurecom.fr
 */

#include "smf_n2.hpp"

#include <arpa/inet.h>
#include <stdexcept>
#include "string.hpp"

#include "smf.h"

extern "C" {
#include "Ngap_AssociatedQosFlowItem.h"
#include "Ngap_Criticality.h"
#include "Ngap_Dynamic5QIDescriptor.h"
#include "Ngap_GTPTunnel.h"
#include "Ngap_NGAP-PDU.h"
#include "Ngap_NonDynamic5QIDescriptor.h"
#include "Ngap_PDUSessionResourceModifyRequestTransfer.h"
#include "Ngap_PDUSessionResourceReleaseCommandTransfer.h"
#include "Ngap_PDUSessionResourceReleaseResponseTransfer.h"
#include "Ngap_PDUSessionResourceSetupRequestTransfer.h"
#include "Ngap_PDUSessionResourceSetupResponseTransfer.h"
#include "Ngap_ProcedureCode.h"
#include "Ngap_ProtocolIE-Field.h"
#include "Ngap_QosFlowAddOrModifyRequestItem.h"
#include "Ngap_QosFlowAddOrModifyResponseItem.h"
#include "Ngap_QosFlowAddOrModifyResponseList.h"
#include "Ngap_QosFlowSetupRequestItem.h"
#include "Ngap_UL-NGU-UP-TNLModifyItem.h"
#include "dynamic_memory_check.h"
}

using namespace smf;
extern smf_app* smf_app_inst;

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_setup_request_transfer(
    pdu_session_create_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  return false;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_setup_request_transfer(
    pdu_session_update_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  return false;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_modify_request_transfer(
    pdu_session_update_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  return false;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_release_command_transfer(
    pdu_session_update_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  return true;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_setup_request_transfer(
    pdu_session_report_response& msg, n2_sm_info_type_e ngap_info_type,
    std::string& ngap_msg_str) {
  return true;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_modify_request_transfer(
    pdu_session_modification_network_requested& msg,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  return true;
}

//------------------------------------------------------------------------------
bool smf_n2::create_n2_pdu_session_resource_modify_response_transfer(
    pdu_session_update_sm_context_response& sm_context_res,
    n2_sm_info_type_e ngap_info_type, std::string& ngap_msg_str) {
  return true;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<Ngap_PDUSessionResourceSetupResponseTransfer_t>& ngap_IE,
    const std::string& n2_sm_info) {
  return RETURNok;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<Ngap_PDUSessionResourceModifyResponseTransfer_t>& ngap_IE,
    const std::string& n2_sm_info) {
  return RETURNok;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<Ngap_PDUSessionResourceReleaseResponseTransfer_t>& ngap_IE,
    const std::string& n2_sm_info) {
  return RETURNok;
}

//---------------------------------------------------------------------------------------------
int smf_n2::decode_n2_sm_information(
    std::shared_ptr<Ngap_PDUSessionResourceSetupUnsuccessfulTransfer_t>&
        ngap_IE,
    const std::string& n2_sm_info) {
  return RETURNok;
}
