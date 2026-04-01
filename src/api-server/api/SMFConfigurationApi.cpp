/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "Helpers.h"
#include "smf_config.hpp"
#include "SMFConfigurationApi.h"

extern std::unique_ptr<oai::config::smf::smf_config> smf_cfg;

namespace oai::smf_server::api {

using namespace oai::model::common::helpers;

SMFConfigurationApi::SMFConfigurationApi(
    std::shared_ptr<Pistache::Rest::Router> rtr) {
  router = rtr;
}

void SMFConfigurationApi::init() {
  setupRoutes();
}

void SMFConfigurationApi::setupRoutes() {
  using namespace Pistache::Rest;

  Routes::Get(
      *router, base + oai::smf::api::smf_sbi_helper::SmfConfPathConfiguration,
      Routes::bind(&SMFConfigurationApi::read_configuration_handler, this));

  Routes::Put(
      *router, base + oai::smf::api::smf_sbi_helper::SmfConfPathConfiguration,
      Routes::bind(&SMFConfigurationApi::update_configuration_handler, this));

  // Default handler, called when a route is not found
  router->addCustomHandler(Routes::bind(
      &SMFConfigurationApi::configuration_api_default_handler, this));
}

void SMFConfigurationApi::read_configuration_handler(
    const Pistache::Rest::Request& request,
    Pistache::Http::ResponseWriter response) {
  try {
    this->read_configuration(response);
  } catch (nlohmann::detail::exception& e) {
    // send a 400 error
    response.send(Pistache::Http::Code::Bad_Request, e.what());
    return;
  } catch (Pistache::Http::HttpError& e) {
    response.send(static_cast<Pistache::Http::Code>(e.code()), e.what());
    return;
  } catch (std::exception& e) {
    // send a 500 error
    response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
    return;
  }
}

void SMFConfigurationApi::update_configuration_handler(
    const Pistache::Rest::Request& request,
    Pistache::Http::ResponseWriter response) {
  try {
    auto configuration_info = nlohmann::json::parse(request.body());
    this->update_configuration(configuration_info, response);
  } catch (nlohmann::detail::exception& e) {
    // send a 400 error
    response.send(Pistache::Http::Code::Bad_Request, e.what());
    return;
  } catch (Pistache::Http::HttpError& e) {
    response.send(static_cast<Pistache::Http::Code>(e.code()), e.what());
    return;
  } catch (std::exception& e) {
    // send a 500 error
    response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
    return;
  }
}

void SMFConfigurationApi::configuration_api_default_handler(
    const Pistache::Rest::Request&, Pistache::Http::ResponseWriter response) {
  response.send(
      Pistache::Http::Code::Not_Found, "The requested method does not exist");
}

}  // namespace oai::smf_server::api
