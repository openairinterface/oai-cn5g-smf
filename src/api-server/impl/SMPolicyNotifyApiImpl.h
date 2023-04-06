


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

#ifndef SMPOLICYNOTIFYAPIIMPL_H
#define SMPOLICYNOTIFYAPIIMPL_H

#pragma once


#include <pistache/endpoint.h>
#include <pistache/http.h>
#include <pistache/router.h>
#include <memory>

#include <SMPolicyNotifyApi.h>

#include <pistache/optional.h>

#include "SmPolicyNotification.h"
#include "ProblemDetails.h"
#include "smf_app.hpp"

namespace oai {
namespace smf_server {
namespace api {

using namespace oai::smf_server::model;

class SMPolicyNotifyApiImpl : public oai::smf_server::api::SMPolicyNotifyApi {
 public:
  SMPolicyNotifyApiImpl(
      std::shared_ptr<Pistache::Rest::Router> rtr, smf::smf_app* smf_app_inst,
      std::string address);
  ~SMPolicyNotifyApiImpl() {}

  void receive_sm_policy_update_notification(
      const SmPolicyNotification& smPolicyNotification,
      Pistache::Http::ResponseWriter& response);

 private:
  smf::smf_app* m_smf_app;
  std::string m_address;
};

}  // namespace api
}  // namespace smf_server
}  // namespace oai

#endif
