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

#include "smf_sbi_helper.hpp"

#include <fmt/format.h>

#include <boost/algorithm/string.hpp>
#include <regex>
#include <vector>

#include "ProblemDetails.h"
#include "logger.hpp"

namespace oai::smf::api {
//------------------------------------------------------------------------------
void smf_sbi_helper::set_problem_details(
    nlohmann::json& json_data, const std::string& detail) {
  Logger::smf_app().error("%s", detail);
  oai::model::common::ProblemDetails problem_details;
  problem_details.setDetail(detail);
  to_json(json_data, problem_details);
}

//------------------------------------------------------------------------------
std::string smf_sbi_helper::get_udm_sdm_sm_data_uri(const std::string& supi) {
  std::string fmr_format_str = {};
  get_fmt_format_form(UdmSdmPathSupiSmData, fmr_format_str);

  return smf_cfg->get_nf(oai::config::UDM_CONFIG_NAME)
             ->get_sbi()
             .get_url(smf_cfg->enable_tls()) +
         oai::common::sbi::sbi_helper::UdmUeCmBase +
         smf_cfg->get_nf(oai::config::UDM_CONFIG_NAME)
             ->get_sbi()
             .get_api_version() +
         fmt::format(fmr_format_str, supi);
}

//------------------------------------------------------------------------------
std::string smf_sbi_helper::get_udm_sdm_subscriptions_uri(
    const std::string& supi) {
  std::string fmr_format_str = {};
  get_fmt_format_form(UdmSdmPathSupiSdmSubscriptions, fmr_format_str);

  return smf_cfg->get_nf(oai::config::UDM_CONFIG_NAME)
             ->get_sbi()
             .get_url(smf_cfg->enable_tls()) +
         oai::common::sbi::sbi_helper::UdmUeCmBase +
         smf_cfg->get_nf(oai::config::UDM_CONFIG_NAME)
             ->get_sbi()
             .get_api_version() +
         fmt::format(fmr_format_str, supi);
}

//------------------------------------------------------------------------------
std::string smf_sbi_helper::get_udm_sdm_unsubscriptions_uri(
    const std::string& supi, const std::string& subscription_id) {
  std::string fmr_format_str = {};
  get_fmt_format_form(
      UdmSdmPathSupiSdmSubscriptionsSubscriptionId, fmr_format_str);

  return smf_cfg->get_nf(oai::config::UDM_CONFIG_NAME)
             ->get_sbi()
             .get_url(smf_cfg->enable_tls()) +
         oai::common::sbi::sbi_helper::UdmUeCmBase +
         smf_cfg->get_nf(oai::config::UDM_CONFIG_NAME)
             ->get_sbi()
             .get_api_version() +
         fmt::format(fmr_format_str, supi, subscription_id);
}

//------------------------------------------------------------------------------
std::string smf_sbi_helper::get_udm_uecm_smf_registration_pdu_session_uri(
    const std::string& supi, const uint8_t& pdu_session_id) {
  std::string fmr_format_str = {};
  get_fmt_format_form(UdmUeCmPathSmfRegistrationPduSession, fmr_format_str);

  return smf_cfg->get_nf(oai::config::UDM_CONFIG_NAME)
             ->get_sbi()
             .get_url(smf_cfg->enable_tls()) +
         oai::common::sbi::sbi_helper::UdmUeCmBase +
         smf_cfg->get_nf(oai::config::UDM_CONFIG_NAME)
             ->get_sbi()
             .get_api_version() +
         fmt::format(
             fmr_format_str, supi, static_cast<uint8_t>(pdu_session_id));
}

}  // namespace oai::smf::api
