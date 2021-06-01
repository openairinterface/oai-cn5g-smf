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

#include "EventNotifyApi.h"
#include "Helpers.h"
#include "flexcn_config.hpp"

extern flexcn::flexcn_config flexcn_cfg;

namespace oai {
namespace flexcn_server {
namespace api {

using namespace oai::flexcn_server::helpers;
using namespace oai::flexcn_server::model;

EventNotifyApi::EventNotifyApi(
    std::shared_ptr<Pistache::Rest::Router> rtr) {
  router = rtr;
}

void EventNotifyApi::init() {
  setupRoutes();
}

void EventNotifyApi::setupRoutes() {
  using namespace Pistache::Rest;

  Routes::Post(
      *router, base + flexcn_cfg.sbi_api_version + "/:notifRef",
      Routes::bind(&EventNotifyApi::notify_status_handler, this));

  // Default handler, called when a route is not found
  router->addCustomHandler(
      Routes::bind(&EventNotifyApi::notify_status_default_handler, this));
}

void EventNotifyApi::notify_status_handler(
    const Pistache::Rest::Request& request,
    Pistache::Http::ResponseWriter response) {
  // Getting the body param
  // CNRecord eventExposureNotification = {};
	NsmfEventExposureNotification eventExposureNotification = {};

   Logger::smf_api_server().debug(
	 "Received a notification from SMF.");
	  Logger::smf_api_server().debug("body: %s\n", request.body().c_str());



  try {
    nlohmann::json::parse(request.body()).get_to(eventExposureNotification);
    this->receive_status_notification(request.body(), response);
    nlohmann::json j = eventExposureNotification;

    Logger::smf_api_server().info("Parsing this event success!!");
    Logger::smf_api_server().info("After parsing: ");
    Logger::smf_api_server().info(j.dump(4).c_str());
    
  } catch (nlohmann::detail::exception& e) {
    // send a 400 error
    response.send(Pistache::Http::Code::Bad_Request, e.what());
    Logger::smf_api_server().info("Parsing this event failed");
    Logger::smf_api_server().info(e.what());
    return;
  } catch (Pistache::Http::HttpError& e) {
    response.send(static_cast<Pistache::Http::Code>(e.code()), e.what());
        Logger::smf_api_server().info("Parsing this event failed");

    return;
  } catch (std::exception& e) {
    // send a 500 error
    response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
        Logger::smf_api_server().info("Parsing this event failed");

    return;
  }
}

void EventNotifyApi::notify_status_default_handler(
    const Pistache::Rest::Request&, Pistache::Http::ResponseWriter response) {
  response.send(
      Pistache::Http::Code::Not_Found, "The requested method does not exist");
}

}  // namespace api
}  // namespace flexcn_server
}  // namespace oai
