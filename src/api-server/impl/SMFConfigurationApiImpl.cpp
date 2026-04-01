/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "logger.hpp"
#include "SMFConfigurationApiImpl.h"
#include "http_definitions.hpp"

namespace oai::smf_server::api {

using namespace oai::model::smf;

SMFConfigurationApiImpl::SMFConfigurationApiImpl(
    std::shared_ptr<Pistache::Rest::Router> rtr,
    oai::app::smf::smf_app* smf_app_inst)
    : SMFConfigurationApi(rtr), m_smf_app(smf_app_inst) {}

void SMFConfigurationApiImpl::read_configuration(
    Pistache::Http::ResponseWriter& response) {
  Logger::smf_api_server().debug("Get SMF Configuration, handling...");

  // Generate a promise and associate this promise to the ITTI message
  uint32_t promise_id = generate_promise_id();
  Logger::smf_n1().debug("Promise ID generated %d", promise_id);

  boost::shared_ptr<boost::promise<nlohmann::json>> p =
      boost::make_shared<boost::promise<nlohmann::json>>();
  boost::shared_future<nlohmann::json> f = p->get_future();
  m_smf_app->add_promise(promise_id, p);

  // Handle the SMFConfiguration in smf_app
  std::shared_ptr<itti_sbi_smf_configuration> itti_msg =
      std::make_shared<itti_sbi_smf_configuration>(
          TASK_SMF_SBI, TASK_SMF_APP, promise_id);

  itti_msg->http_version = 1;
  itti_msg->promise_id   = promise_id;

  m_smf_app->handle_sbi_get_configuration(itti_msg);

  boost::future_status status;
  // wait for timeout or ready
  status = f.wait_for(boost::chrono::milliseconds(FUTURE_STATUS_TIMEOUT_MS));
  if (status == boost::future_status::ready) {
    assert(f.is_ready());
    assert(f.has_value());
    assert(!f.has_exception());
    // Wait for the result from APP
    // result includes json content and http response code
    nlohmann::json result = f.get();
    Logger::smf_api_server().debug("Got result for promise ID %d", promise_id);

    // process data
    uint32_t http_response_code = 0;
    nlohmann::json json_data    = {};

    if (result.find(oai::http::kSbiResponseHttpResponseCode) != result.end()) {
      http_response_code =
          result[oai::http::kSbiResponseHttpResponseCode].get<int>();
    }

    if (http_response_code == 200) {
      if (result.find("content") != result.end()) {
        json_data = result["content"];
      }
      response.headers().add<Pistache::Http::Header::ContentType>(
          Pistache::Http::Mime::MediaType("application/json"));
      response.send(Pistache::Http::Code::Ok, json_data.dump().c_str());
    } else {
      // Problem details
      if (result.find("ProblemDetails") != result.end()) {
        json_data = result["ProblemDetails"];
      }

      response.headers().add<Pistache::Http::Header::ContentType>(
          Pistache::Http::Mime::MediaType("application/problem+json"));
      response.send(
          Pistache::Http::Code(http_response_code), json_data.dump().c_str());
    }
  }
}

void SMFConfigurationApiImpl::update_configuration(
    nlohmann::json& configuration_info,
    Pistache::Http::ResponseWriter& response) {
  Logger::smf_api_server().debug("Update SMF Configuration, handling...");

  // Generate a promise and associate this promise to the ITTI message
  uint32_t promise_id = generate_promise_id();
  Logger::smf_n1().debug("Promise ID generated %d", promise_id);

  boost::shared_ptr<boost::promise<nlohmann::json>> p =
      boost::make_shared<boost::promise<nlohmann::json>>();
  boost::shared_future<nlohmann::json> f = p->get_future();
  m_smf_app->add_promise(promise_id, p);

  // Handle the SMFConfiguration in smf_app
  std::shared_ptr<itti_sbi_update_smf_configuration> itti_msg =
      std::make_shared<itti_sbi_update_smf_configuration>(
          TASK_SMF_SBI, TASK_SMF_APP, promise_id);

  itti_msg->http_version  = 1;
  itti_msg->promise_id    = promise_id;
  itti_msg->configuration = configuration_info;

  m_smf_app->handle_sbi_update_configuration(itti_msg);

  boost::future_status status;
  // wait for timeout or ready
  status = f.wait_for(boost::chrono::milliseconds(FUTURE_STATUS_TIMEOUT_MS));
  if (status == boost::future_status::ready) {
    assert(f.is_ready());
    assert(f.has_value());
    assert(!f.has_exception());
    // Wait for the result from APP
    // result includes json content and http response code
    nlohmann::json result = f.get();
    Logger::smf_api_server().debug("Got result for promise ID %d", promise_id);

    // process data
    uint32_t http_response_code = 0;
    nlohmann::json json_data    = {};

    if (result.find(oai::http::kSbiResponseHttpResponseCode) != result.end()) {
      http_response_code =
          result[oai::http::kSbiResponseHttpResponseCode].get<int>();
    }

    if (http_response_code == 200) {
      if (result.find("content") != result.end()) {
        json_data = result["content"];
      }
      response.headers().add<Pistache::Http::Header::ContentType>(
          Pistache::Http::Mime::MediaType("application/json"));
      response.send(Pistache::Http::Code::Ok, json_data.dump().c_str());
    } else {
      // Problem details
      if (result.find("ProblemDetails") != result.end()) {
        json_data = result["ProblemDetails"];
      }

      response.headers().add<Pistache::Http::Header::ContentType>(
          Pistache::Http::Mime::MediaType("application/problem+json"));
      response.send(
          Pistache::Http::Code(http_response_code), json_data.dump().c_str());
    }
  }
}

}  // namespace oai::smf_server::api
