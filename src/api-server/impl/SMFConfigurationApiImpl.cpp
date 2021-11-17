
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

#include "SMFConfigurationApiImpl.h"
#include "logger.hpp"
#include "smf_msg.hpp"
#include "itti_msg_n11.hpp"
#include "3gpp_29.502.h"
#include <nghttp2/asio_http2_server.h>
#include "smf_config.hpp"
#include "3gpp_conversions.hpp"
#include "mime_parser.hpp"
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
#include <boost/chrono.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/chrono/duration.hpp>
#include <boost/chrono/system_clocks.hpp>

extern smf::smf_config smf_cfg;

namespace oai {
namespace smf_server {
namespace api {

using namespace oai::smf_server::model;

SMFConfigurationApiImpl::SMFConfigurationApiImpl(
    std::shared_ptr<Pistache::Rest::Router> rtr, smf::smf_app* smf_app_inst,
    std::string address)
    : SMFConfigurationApi(rtr), m_smf_app(smf_app_inst), m_address(address) {}

void SMFConfigurationApiImpl::add_dnn_configuration(
    const DnnConfigurationMessage& dnnConfigurationMessage,
    Pistache::Http::ResponseWriter& response) {
  Logger::smf_api_server().info(
      "Received a request to add a new DNN configuration.");
  response.send(Pistache::Http::Code::Ok, "OK");
}

void SMFConfigurationApiImpl::retrieve_dnn_configurations(
    const DnnConfigurationMessage& dnnConfigurationMessage,
    Pistache::Http::ResponseWriter& response) {
  Logger::smf_api_server().info(
      "Received a request to retrieve DNN configurations.");

  // Handle the message in smf_app
  std::shared_ptr<itti_sbi_dnn_configuration> itti_msg =
      std::make_shared<itti_sbi_dnn_configuration>(TASK_SMF_SBI, TASK_SMF_APP);
  itti_msg->dnn_configuration_msg = dnnConfigurationMessage;
  itti_msg->http_version          = 1;

  ProblemDetails problem_details = {};
  uint8_t http_code              = 0;

  if (m_smf_app->handle_dnn_configurations(
          itti_msg, problem_details, http_code)) {
    response.send(Pistache::Http::Code(204));
  } else {
    nlohmann::json json_data = {};
    to_json(json_data, problem_details);
    // content type
    response.headers().add<Pistache::Http::Header::ContentType>(
        Pistache::Http::Mime::MediaType("application/problem+json"));
    response.send(Pistache::Http::Code(http_code), json_data.dump().c_str());
  }

  response.send(Pistache::Http::Code::Ok, "OK");
}

}  // namespace api
}  // namespace smf_server
}  // namespace oai
