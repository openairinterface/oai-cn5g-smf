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

/*! \file smf_pco.cpp
 \brief
 \author Lionel Gauthier
 \company Eurecom
 \email: lionel.gauthier@eurecom.fr
 */

#include "smf_pco.hpp"

#include "3gpp_24.008.h"
#include "common_defs.h"
#include "logger.hpp"
#include "rfc_1332.h"
#include "rfc_1877.h"
#include "smf_app.hpp"
#include "smf_config.hpp"
#include "string.hpp"

using namespace smf;

extern smf_config smf_cfg;

//------------------------------------------------------------------------------
int smf_app::pco_push_protocol_or_container_id(
    protocol_configuration_options_t& pco,
    pco_protocol_or_container_id_t* const poc_id) {
  return RETURNok;
}

//------------------------------------------------------------------------------
int smf_app::process_pco_request_ipcp(
    protocol_configuration_options_t& pco_resp,
    const pco_protocol_or_container_id_t* const poc_id) {
  return RETURNok;
}

//------------------------------------------------------------------------------
int smf_app::process_pco_dns_server_request(
    protocol_configuration_options_t& pco_resp,
    const pco_protocol_or_container_id_t* const poc_id) {
  return RETURNok;
}

//------------------------------------------------------------------------------
int smf_app::process_pco_dns_server_v6_request(
    protocol_configuration_options_t& pco_resp,
    const pco_protocol_or_container_id_t* const poc_id) {
  
  return RETURNok;
}
//------------------------------------------------------------------------------
int smf_app::process_pco_link_mtu_request(
    protocol_configuration_options_t& pco_resp,
    const pco_protocol_or_container_id_t* const poc_id) {
  return RETURNok;
}

//------------------------------------------------------------------------------
int smf_app::process_pco_request(
    const protocol_configuration_options_t& pco_req,
    protocol_configuration_options_t& pco_resp,
    protocol_configuration_options_ids_t& pco_ids) {
  return RETURNok;
}
