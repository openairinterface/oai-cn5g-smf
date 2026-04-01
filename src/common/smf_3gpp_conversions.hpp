/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_SMF_3GPP_CONVERSIONS_HPP_SEEN
#define FILE_SMF_3GPP_CONVERSIONS_HPP_SEEN

#include "3gpp_24.008.h"
#include "3gpp_24.501.hpp"
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
    oai::app::smf::pdu_session_create_sm_context_request& pcr);

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
    oai::app::smf::pdu_session_update_sm_context_request& pur);

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
    oai::app::smf::pdu_session_release_sm_context_request& prr);

/*
 * Convert NsmfEventExposure from OpenAPI into Event Exposure Msg
 * @param [const oai::model::smf::NsmfEventExposure&] nee:
 * NsmfEventExposure in OpenAPI
 * @param [smf::event_exposure_msg&] eem: Event Exposure Msg
 * @return void
 */
void smf_event_exposure_notification_from_openapi(
    const oai::model::smf::NsmfEventExposure& nee,
    oai::app::smf::event_exposure_msg& eem);

/*
 * Convert NAS to SM Context Request msg
 * @param [const std::shared_ptr<Nas5gsmMessage>&] nas_msg: 5GSM NAS message
 * @param [smf::pdu_session_create_sm_context_request&] pcr: PDU
 * SessionCreateSMContextRequest msg
 * @return void
 */
void sm_context_request_from_nas(
    const std::shared_ptr<Nas5gsmMessage>& nas_msg,
    oai::app::smf::pdu_session_create_sm_context_request& pcr);

void create_sm_context_response_from_ctx_request(
    const std::shared_ptr<itti_sbi_create_sm_context_request>& ct_request,
    std::shared_ptr<itti_sbi_create_sm_context_response>& ct_response);

void update_sm_context_response_from_ctx_request(
    const std::shared_ptr<itti_sbi_update_sm_context_request>& ct_request,
    std::shared_ptr<itti_sbi_update_sm_context_response>& ct_response);

void plmn_from_model(const oai::model::common::PlmnId&, plmn_t& plmn);

}  // namespace xgpp_conv

#endif /* FILE_3GPP_CONVERSIONS_HPP_SEEN */
