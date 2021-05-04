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
  in6_addr ipcp_out_dns_prim_ipv6_addr       = smf_cfg.default_dnsv6;
  pco_protocol_or_container_id_t poc_id_resp = {0};
  uint8_t dnsv6_array[16];

  Logger::smf_app().debug(
      "PCO: Protocol identifier IPCP option DNS Server v6 Request");
  poc_id_resp.protocol_id = PCO_CONTAINER_IDENTIFIER_DNS_SERVER_IPV6_ADDRESS;
  poc_id_resp.length_of_protocol_id_contents = 16;

  char str_addr6[INET6_ADDRSTRLEN];
  if (inet_ntop(
          AF_INET6, &ipcp_out_dns_prim_ipv6_addr, str_addr6,
          sizeof(str_addr6))) {
    std::string ipv6_addr_str((char*) str_addr6, INET6_ADDRSTRLEN);
    // Logger::smf_app().info(" Ipv6 address....: %s", ipv6_addr_str.c_str());
    unsigned char buf_in6_addr[sizeof(struct in6_addr)];
    if (inet_pton(AF_INET6, util::trim(ipv6_addr_str).c_str(), buf_in6_addr) ==
        1) {
      for (int i = 0; i <= 15; i++) dnsv6_array[i] = (uint8_t)(buf_in6_addr[i]);
    }
  }

  std::string tmp_s((const char*) &dnsv6_array[0], sizeof(dnsv6_array));
  poc_id_resp.protocol_id_contents = tmp_s;

  return pco_push_protocol_or_container_id(pco_resp, &poc_id_resp);
}
//------------------------------------------------------------------------------
int smf_app::process_pco_link_mtu_request(
    protocol_configuration_options_t& pco_resp,
    const pco_protocol_or_container_id_t* const poc_id) {
  pco_protocol_or_container_id_t poc_id_resp = {0};
  uint8_t mtu_array[2];

  Logger::smf_app().debug(
      "PCO: Protocol identifier IPCP option Link MTU Request");
  poc_id_resp.protocol_id = PCO_CONTAINER_IDENTIFIER_IPV4_LINK_MTU;
  poc_id_resp.length_of_protocol_id_contents = 2;
  mtu_array[0]                               = (uint8_t)(smf_cfg.ue_mtu >> 8);
  mtu_array[1]                               = (uint8_t)(smf_cfg.ue_mtu & 0xFF);
  std::string tmp_s((const char*) &mtu_array[0], 2);
  poc_id_resp.protocol_id_contents = tmp_s;

  return pco_push_protocol_or_container_id(pco_resp, &poc_id_resp);
}

//------------------------------------------------------------------------------
int smf_app::process_pco_request(
    const protocol_configuration_options_t& pco_req,
    protocol_configuration_options_t& pco_resp,
    protocol_configuration_options_ids_t& pco_ids) {
  switch (pco_req.configuration_protocol) {
    case PCO_CONFIGURATION_PROTOCOL_PPP_FOR_USE_WITH_IP_PDP_TYPE_OR_IP_PDN_TYPE:
      pco_resp.ext                          = 1;
      pco_resp.spare                        = 0;
      pco_resp.num_protocol_or_container_id = 0;
      pco_resp.configuration_protocol       = pco_req.configuration_protocol;
      break;

    default:
      Logger::smf_app().warn(
          "PCO: configuration protocol 0x%X not supported now",
          pco_req.configuration_protocol);
      break;
  }

  for (int id = 0; id < pco_req.num_protocol_or_container_id; id++) {
    switch (pco_req.protocol_or_container_ids[id].protocol_id) {
      case PCO_PROTOCOL_IDENTIFIER_IPCP:
        process_pco_request_ipcp(
            pco_resp, &pco_req.protocol_or_container_ids[id]);
        pco_ids.pi_ipcp = true;
        break;

      case PCO_CONTAINER_IDENTIFIER_DNS_SERVER_IPV4_ADDRESS_REQUEST:
        process_pco_dns_server_request(
            pco_resp, &pco_req.protocol_or_container_ids[id]);
        pco_ids.ci_dns_server_ipv4_address_request = true;
        break;
      case PCO_CONTAINER_IDENTIFIER_DNS_SERVER_IPV6_ADDRESS:
        process_pco_dns_server_v6_request(
            pco_resp, &pco_req.protocol_or_container_ids[id]);
        pco_ids.ci_dns_server_ipv6_address_request = true;
        break;
      case PCO_CONTAINER_IDENTIFIER_IP_ADDRESS_ALLOCATION_VIA_NAS_SIGNALLING:
        Logger::smf_app().debug("PCO: Allocation via NAS signaling requested");
        pco_ids.ci_ip_address_allocation_via_nas_signalling = true;
        break;

      case PCO_CONTAINER_IDENTIFIER_IPV4_LINK_MTU_REQUEST:
        process_pco_link_mtu_request(
            pco_resp, &pco_req.protocol_or_container_ids[id]);
        pco_ids.ci_ipv4_link_mtu_request = true;
        break;

      default:
        Logger::smf_app().warn(
            "PCO: Protocol/container identifier 0x%04X not supported now",
            pco_req.protocol_or_container_ids[id].protocol_id);
    }
  }

  if (smf_cfg.force_push_pco) {
    pco_ids.ci_ip_address_allocation_via_nas_signalling = true;
    if (!pco_ids.ci_dns_server_ipv4_address_request) {
      process_pco_dns_server_request(pco_resp, nullptr);
    }
    if (!pco_ids.ci_ipv4_link_mtu_request) {
      process_pco_link_mtu_request(pco_resp, nullptr);
    }
  }
  return RETURNok;
}
