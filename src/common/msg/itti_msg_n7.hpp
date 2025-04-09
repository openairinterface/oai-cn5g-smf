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

/*! \file itti_msg_n7.hpp
 \brief
 \author  Tariro Mukute
 \company phine.tech, University of Cape Town
 \date 2025
 \email: tariro.mukute@phine.tech
 */

#ifndef ITTI_MSG_N7_HPP_INCLUDED_
#define ITTI_MSG_N7_HPP_INCLUDED_

#include <nlohmann/json.hpp>
#include "itti_msg.hpp"
#include "smf_msg.hpp"
#include "pistache/http.h"
#include "smf_profile.hpp"
#include "PatchItem.h"

class itti_n7_msg : public itti_msg {
 public:
  itti_n7_msg(
      const itti_msg_type_t msg_type, const task_id_t orig,
      const task_id_t dest)
      : itti_msg(msg_type, orig, dest) {}
  itti_n7_msg(const itti_n7_msg& i) : itti_msg(i) {}
  itti_n7_msg(
      const itti_n7_msg& i, const task_id_t orig, const task_id_t dest)
      : itti_n7_msg(i) {
    origin      = orig;
    destination = dest;
  }
};

//-----------------------------------------------------------------------------

// Create class for N7 update policy notification request
class itti_n7_update_policy_notification_request : public itti_n7_msg {
 public:
  itti_n7_update_policy_notification_request(
      const task_id_t orig, const task_id_t dest, uint32_t promise_id, const std::string& smf_ref)
      : itti_n7_msg(N7_UPDATE_POLICY_NOTIFICATION_REQUEST, orig, dest),
        pid(promise_id),
        scid(smf_ref),
        http_version(1) {}
  itti_n7_update_policy_notification_request(
      const itti_n7_update_policy_notification_request& i)
      : itti_n7_msg(i),
        pid(i.pid),
        http_version(i.http_version) {}
  itti_n7_update_policy_notification_request(
      const itti_n7_update_policy_notification_request& i, const task_id_t orig,
      const task_id_t dest)
      : itti_n7_msg(i, orig, dest),
        pid(i.pid),
        http_version(i.http_version) {}
  const char* get_msg_name() {
    return "N7_UPDATE_POLICY_NOTIFICATION_REQUEST";
  };
  
  // TODO: Add request message
  smf::pdu_session_sm_policy_notificatiion req;
  uint32_t pid;
  uint8_t http_version;
  std::string scid;  // SM Context ID
};

//-----------------------------------------------------------------------------
// Create class for N7 update policy notification response
class itti_n7_update_policy_notification_response : public itti_n7_msg {
 public:
  itti_n7_update_policy_notification_response(
      const task_id_t orig, const task_id_t dest, uint32_t promise_id)
      : itti_n7_msg(N7_UPDATE_POLICY_NOTIFICATION_RESPONSE, orig, dest),
        pid(promise_id),
        res() {}
  itti_n7_update_policy_notification_response(
      const itti_n7_update_policy_notification_response& i)
      : itti_n7_msg(i), res(i.res), pid(i.pid) {}
  itti_n7_update_policy_notification_response(
      const itti_n7_update_policy_notification_response& i,
      const task_id_t orig, const task_id_t dest)
      : itti_n7_msg(i, orig, dest), res(i.res), pid(i.pid) {}
  const char* get_msg_name() {
    return "N7_UPDATE_POLICY_NOTIFICATION_RESPONSE";
  };

  // TODO: Add response message
  smf::pdu_session_update_sm_context_response res;
  uint32_t pid;
};

#endif /* ITTI_MSG_N7_HPP_INCLUDED_ */
