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

#ifndef FILE_3GPP_CONVERSIONS_HPP_SEEN
#define FILE_3GPP_CONVERSIONS_HPP_SEEN

#include "3gpp_24.008.h"
#include "3gpp_24.501.h"
#include "3gpp_29.244.h"
#include "NotificationData.h"
#include "NsmfEventExposure.h"
#include "SmContextMessage.h"
#include "SmContextReleaseMessage.h"
#include "SmContextUpdateMessage.h"
#include "endpoint.hpp"
#include "itti_msg_sbi.hpp"
#include "smf_msg.hpp"
#include "Nas5gsmMessage.hpp"

using namespace oai::nas;

namespace xgpp_conv {

/*
 * Convert PAA to PFCP UE IP Addr
 * @param [const paa_t&] paa: paa
 * @param [pfcp::ue_ip_address_t&] ue_ip_address: UE IP Addr
 * @return void
 */
void paa_to_pfcp_ue_ip_address(
    const paa_t& paa, pfcp::ue_ip_address_t& ue_ip_address);

/*
 * Convert PDN IP to PFCP UE IP Addr
 * @param [const pdu_session_type_t&] pdu_session_type: PDU Session Type
 * @param [const struct in_addr&] ipv4_address: IPv4 Addr
 * @param [const struct in6_addr] ipv6_address: IPv6 Addr
 * @param [pfcp::ue_ip_address_t&] ue_ip_address: UE IP Addr
 * @return void
 */
void pdn_ip_to_pfcp_ue_ip_address(
    const pdu_session_type_t& pdu_session_type,
    const struct in_addr& ipv4_address, const struct in6_addr ipv6_address,
    pfcp::ue_ip_address_t& ue_ip_address);

/*
 * Convert SM Context Create Msg from OpenAPI into PDU
 * SessionCreateSMContextRequest msg
 * @param [const oai::model::smf::SmContextMessage&] scm: SM Context
 * Create Msg in OpenAPI
 * @param [smf::pdu_session_create_sm_context_request&] pcr: PDU
 * SessionCreateSMContextRequest msg
 * @return void
 */
void sm_context_create_from_openapi(
    const oai::model::smf::SmContextMessage& scm,
    smf::pdu_session_create_sm_context_request& pcr);

/*
 * Convert SM Context Update Msg from OpenAPI into PDU
 * SessionUpdateSMContextRequest msg
 * @param [const oai::model::smf::SmContextUpdateMessage&] scu: SM
 * Context Update Msg in OpenAPI
 * @param [smf::pdu_session_update_sm_context_request&] pur: PDU
 * SessionUpdateSMContextRequest msg
 * @return void
 */
void sm_context_update_from_openapi(
    const oai::model::smf::SmContextUpdateMessage& scu,
    smf::pdu_session_update_sm_context_request& pur);

/*
 * Convert SM Context Release Msg from OpenAPI into PDU
 * SessionReleaseSMContextRequest msg
 * @param [const oai::model::smf::SmContextReleaseMessage&] srm: SM
 * Context Release Msg in OpenAPI
 * @param [smf::pdu_session_release_sm_context_request&] prr: PDU
 * SessionReleaseSMContextRequest msg
 * @return void
 */
void sm_context_release_from_openapi(
    const oai::model::smf::SmContextReleaseMessage& srm,
    smf::pdu_session_release_sm_context_request& prr);

/*
 * Convert NsmfEventExposure from OpenAPI into Event Exposure Msg
 * @param [const oai::model::smf::NsmfEventExposure&] nee:
 * NsmfEventExposure in OpenAPI
 * @param [smf::event_exposure_msg&] eem: Event Exposure Msg
 * @return void
 */
void smf_event_exposure_notification_from_openapi(
    const oai::model::smf::NsmfEventExposure& nee,
    smf::event_exposure_msg& eem);

/*
 * Convert NAS to SM Context Request msg
 * @param [const std::shared_ptr<Nas5gsmMessage>&] nas_msg: 5GSM NAS message
 * @param [smf::pdu_session_create_sm_context_request&] pcr: PDU
 * SessionCreateSMContextRequest msg
 * @return void
 */
void sm_context_request_from_nas(
    const std::shared_ptr<Nas5gsmMessage>& nas_msg,
    smf::pdu_session_create_sm_context_request& pcr);

void create_sm_context_response_from_ctx_request(
    const std::shared_ptr<itti_sbi_create_sm_context_request>& ct_request,
    std::shared_ptr<itti_sbi_create_sm_context_response>& ct_response);

void update_sm_context_response_from_ctx_request(
    const std::shared_ptr<itti_sbi_update_sm_context_request>& ct_request,
    std::shared_ptr<itti_sbi_update_sm_context_response>& ct_response);

void plmn_from_model(const oai::model::common::PlmnId&, plmn_t& plmn);

}  // namespace xgpp_conv

#endif /* FILE_3GPP_CONVERSIONS_HPP_SEEN */
