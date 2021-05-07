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

/*! \file smf_n1.cpp
 \brief
 \author  Tien-Thinh NGUYEN, Keliang DU
 \company Eurecom
 \date 2019
 \email:  tien-thinh.nguyen@eurecom.fr
 */

#include "smf_n1.hpp"

#include <arpa/inet.h>
#include <stdexcept>
#include "string.hpp"

#include "smf.h"
#include "smf_app.hpp"
#include "3gpp_conversions.hpp"
#include "epc.h"

extern "C" {
#include "dynamic_memory_check.h"
}

using namespace smf;
extern smf_app* smf_app_inst;

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_establishment_accept(
    pdu_session_create_sm_context_response& sm_context_res,
    std::string& nas_msg_str, cause_value_5gsm_e sm_cause) {
  return true;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_establishment_reject(
    pdu_session_msg& msg, std::string& nas_msg_str,
    cause_value_5gsm_e sm_cause) {
  return true;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_modification_command(
    pdu_session_update_sm_context_response& sm_context_res,
    std::string& nas_msg_str, cause_value_5gsm_e sm_cause) {
  return true;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_modification_command(
    pdu_session_modification_network_requested& msg, std::string& nas_msg_str,
    cause_value_5gsm_e sm_cause) {
  return true;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_release_reject(
    pdu_session_update_sm_context_request& sm_context_res,
    std::string& nas_msg_str, cause_value_5gsm_e sm_cause) {
  return true;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_release_command(
    pdu_session_update_sm_context_response& sm_context_res,
    std::string& nas_msg_str, cause_value_5gsm_e sm_cause) {
  return true;
}

//-----------------------------------------------------------------------------------------------------
bool create_n1_pdu_session_release_command(
    pdu_session_modification_network_requested& msg, std::string& nas_msg_str,
    cause_value_5gsm_e sm_cause) {
  return true;
}

