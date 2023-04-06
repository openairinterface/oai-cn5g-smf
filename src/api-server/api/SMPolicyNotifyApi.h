
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

/*
 * SMPolicyNotifyApi.h
 *
 *
 */

#ifndef SMPOLICYNOTIFYAPI_H
#define SMPOLICYNOTIFYAPI_H

#pragma once

#include <pistache/http.h>
#include <pistache/router.h>
#include <pistache/http_headers.h>
#include <pistache/optional.h>

#include "SmPolicyNotification.h"
#include "ProblemDetails.h"

namespace oai {
namespace smf_server {
namespace api {

using namespace oai::smf_server::model;

class SMPolicyNotifyApi {
 public:
  SMPolicyNotifyApi(std::shared_ptr<Pistache::Rest::Router> rtr);
  virtual ~SMPolicyNotifyApi() {}
  void init();

  const std::string base = "/callbacks/npcf-smpolicycontrol/";

 private:
  void setupRoutes();

  void notify_sm_policy_update_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);
  void notify_sm_policy_update_default_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);

  std::shared_ptr<Pistache::Rest::Router> router;

  /// <summary>
  ///
  /// </summary>
  /// <remarks>
  ///
  /// </remarks>
  /// <param name="NotificationData"></param>
  virtual void receive_sm_policy_update_notification(
      const SmPolicyNotification& smPolicyNotification,
      Pistache::Http::ResponseWriter& response) = 0;
};

}  // namespace api
}  // namespace smf_server
}  // namespace oai

#endif /* SMPolicyNotifyApi_H_ */
