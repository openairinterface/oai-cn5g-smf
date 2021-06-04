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

/*! \file flexcn_sbi.hpp
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2019
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr
 */

#ifndef FILE_FLEXCN_SBI_HPP_SEEN
#define FILE_FLEXCN_SBI_HPP_SEEN

#include <map>
#include <thread>

#include <curl/curl.h>
#include "3gpp_29.503.h"
#include "flexcn.h"
#include "flexcn_context.hpp"

namespace flexcn {

#define TASK_FLEXCN_SBI_TIMEOUT_NRF_HEARTBEAT_REQUEST 1

class flexcn_sbi {
 private:
  std::thread::id thread_id;
  std::thread thread;

 public:
  flexcn_sbi();
  flexcn_sbi(flexcn_sbi const&) = delete;
  void operator=(flexcn_sbi const&) = delete;

  /*
   * Send SM Context Status Notification to AMF
   * @param [std::shared_ptr<itti_n11_notify_sm_context_status>]
   * sm_context_status: Content of message to be sent
   * @return void
   */
  void send_sm_context_status_notification(
      std::shared_ptr<itti_n11_notify_sm_context_status> sm_context_status);

  /*
   * Send Notification for the associated event to the subscribers
   * @param [std::shared_ptr<itti_n11_notify_subscribed_event>] msg: Content of
   * message to be sent
   * @return void
   */
  void notify_subscribed_event(
      std::shared_ptr<itti_n11_notify_subscribed_event> msg);

  /*
   * Send a NFStatusSubscribe to NRF (to be notified when a new UPF becomes
   * available)
   * @param [std::shared_ptr<itti_n11_subscribe_upf_status_notify>] msg: Content
   * of message to be sent
   * @return void
   */
  void subscribe_upf_status_notify(
      std::shared_ptr<itti_n11_subscribe_upf_status_notify> msg);

  // FLEXCN
  void subscribe_pdu_session_status_notify(
      std::shared_ptr<itti_n11_subscribe_pdu_session_status_notify> msg);
  /*
   * Create Curl handle for multi curl
   * @param [event_notification&] ev_notif: content of the event notification
   * @param [std::string *] data: data
   * @return pointer to the created curl
   */
  CURL* curl_create_handle(event_notification& ev_notif, std::string* data);

};
}  // namespace flexcn
#endif /* FILE_FLEXCN_SBI_HPP_SEEN */
