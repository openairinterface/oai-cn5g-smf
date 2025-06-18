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
  static std::string SmfCallbackBase() {
    return sbi_helper::SmfCallbackBase + smf_cfg->sbi_api_version;
  }

  static void set_problem_details(
      nlohmann::json& json_data, const std::string& detail);

  static std::string get_udm_sdm_subscriptions_uri(const std::string& supi);
};

}  // namespace oai::smf::api

#endif
