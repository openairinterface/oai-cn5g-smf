/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _SMF_SBI_HELPER_HPP
#define _SMF_SBI_HELPER_HPP

#include <nlohmann/json.hpp>

#include "smf_config.hpp"
#include "sbi_helper.hpp"

using namespace oai::config;
using namespace oai::common::sbi;

extern std::unique_ptr<oai::config::smf::smf_config> smf_cfg;

namespace oai::smf::api {

class smf_sbi_helper : public sbi_helper {
 public:
  static std::string SmfPduSessionBase() {
    return sbi_helper::SmfPduSessionBase + smf_cfg->sbi_api_version;
  }
  static std::string SmfEventExposureBase() {
    return sbi_helper::SmfEventExposureBase + smf_cfg->sbi_api_version;
  }

  static std::string SmfStatusNotifyBase() {
    return sbi_helper::SmfStatusNotifyBase + smf_cfg->sbi_api_version;
  }

  static std::string SmfConfBase() {
    return sbi_helper::SmfConfBase + smf_cfg->sbi_api_version;
  }

  static std::string SmfCallbackBase() {
    return sbi_helper::SmfCallbackBase + smf_cfg->sbi_api_version;
  }

  static std::string UdmUeCmBase() {
    return sbi_helper::UdmUeCmBase + smf_cfg->sbi_api_version;
  }

  static void set_problem_details(
      nlohmann::json& json_data, const std::string& detail);

  static std::string get_amf_comm_ue_context_n1_n2_message_base_uri(
      const std::string& supi);
  static std::string get_udm_sdm_sm_data_uri(const std::string& supi);
  static std::string get_udm_sdm_subscriptions_uri(const std::string& supi);
  static std::string get_udm_uecm_smf_registration_pdu_session_uri(
      const std::string& supi, const uint8_t& pdu_session_id);
  static std::string get_udm_sdm_unsubscriptions_uri(
      const std::string& supi, const std::string& subscription_id);
};

}  // namespace oai::smf::api

#endif
