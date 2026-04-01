/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "NFStatusNotifyApi.h"
#include "Helpers.h"
#include "logger.hpp"
#include "smf_config.hpp"

extern std::unique_ptr<oai::config::smf::smf_config> smf_cfg;

namespace oai {
namespace smf_server {
namespace api {

using namespace oai::model::common::helpers;
using namespace oai::model::smf;

NFStatusNotifyApi::NFStatusNotifyApi(
    std::shared_ptr<Pistache::Rest::Router> rtr) {
  router = rtr;
}

void NFStatusNotifyApi::init() {
  setupRoutes();
}

void NFStatusNotifyApi::setupRoutes() {
  using namespace Pistache::Rest;

  Routes::Post(
      *router,
      base + oai::smf::api::smf_sbi_helper::SmfStatusNotifyPathSubscriptions,
      Routes::bind(&NFStatusNotifyApi::notify_nf_status_handler, this));

  // Default handler, called when a route is not found
  router->addCustomHandler(
      Routes::bind(&NFStatusNotifyApi::notify_nf_status_default_handler, this));
}

void NFStatusNotifyApi::notify_nf_status_handler(
    const Pistache::Rest::Request& request,
    Pistache::Http::ResponseWriter response) {
  Logger::smf_api_server().info("Received a NFStatusNotify message");
  Logger::smf_api_server().debug("Message body: %s", request.body().c_str());

  // Getting the body param
  NotificationData notificationData;

  try {
    nlohmann::json::parse(request.body()).get_to(notificationData);
    this->receive_nf_status_notification(notificationData, response);
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

void NFStatusNotifyApi::notify_nf_status_default_handler(
    const Pistache::Rest::Request&, Pistache::Http::ResponseWriter response) {
  response.send(
      Pistache::Http::Code::Not_Found, "The requested method does not exist");
}

}  // namespace api
}  // namespace smf_server
}  // namespace oai
