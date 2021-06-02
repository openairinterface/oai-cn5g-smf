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

/*! \file flexcn_app.hpp
 \brief
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2019
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr
 */

#ifndef FILE_FLEXCN_APP_HPP_SEEN
#define FILE_FLEXCN_APP_HPP_SEEN

#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
#include <future>
#include <map>
#include <set>
#include <shared_mutex>
#include <string>
#include <thread>

#include "3gpp_29.502.h"
#include "itti_msg_n11.hpp"
#include "itti_msg_n4.hpp"
#include "itti_msg_sbi.hpp"
#include "pistache/endpoint.h"
#include "pistache/http.h"
#include "pistache/router.h"
#include "smf.h"
#include "smf_context.hpp"
#include "smf_msg.hpp"
#include "smf_profile.hpp"
#include "smf_subscription.hpp"
#include "ProblemDetails.h"
#include "EventNotification.h"
#include "ueinfo_smf_record.hpp"

namespace flexcn {

#define TASK_FLEXCN_APP_TRIGGER_T3591 (0)
#define TASK_FLEXCN_APP_TIMEOUT_T3591 (1)
#define TASK_FLEXCN_APP_TRIGGER_T3592 (2)
#define TASK_FLEXCN_APP_TIMEOUT_T3592 (3)
#define TASK_FLEXCN_APP_TIMEOUT_NRF_HEARTBEAT (4)
#define TASK_FLEXCN_APP_TIMEOUT_NRF_DEREGISTRATION (5)

// Table 10.3.2 @3GPP TS 24.501 V16.1.0 (2019-06)
#define T3591_TIMER_VALUE_SEC 16
#define T3591_TIMER_MAX_RETRIES 4
#define T3592_TIMER_VALUE_SEC 16
#define T3592_TIMER_MAX_RETRIES 4

typedef enum {
  PDU_SESSION_ESTABLISHMENT = 1,
  PDU_SESSION_MODIFICATION  = 2,
  PDU_SESSION_RELEASE       = 3
} pdu_session_procedure_t;

class flexcn_config;

class flexcn_app {
 private:
  // list of events
  // std::vector<CNRecords> cn_table;
  // merge CNrecords to one

  // Hung_TODO: change the flexcn name to flexcn
  // keep the old name for compiling purpose.

  std::thread::id thread_id;
  std::thread thread;

  // seid generator
  uint64_t seid_n4_generator;
  std::mutex m_seid_n4_generator;
  std::set<uint64_t> set_seid_n4;

  std::map<seid_t, std::shared_ptr<smf_context>> seid2smf_context;
  mutable std::shared_mutex m_seid2smf_context;

  std::map<supi64_t, std::shared_ptr<smf_context>> supi2smf_context;
  mutable std::shared_mutex m_supi2smf_context;

  std::map<
      std::pair<evsub_id_t, smf_event_t>, std::shared_ptr<smf_subscription>>
      smf_event_subscriptions;

  mutable std::shared_mutex m_scid2smf_context;
  mutable std::shared_mutex m_smf_event_subscriptions;
  // Store promise IDs for Create/Update session
  mutable std::shared_mutex m_sm_context_create_promises;
  mutable std::shared_mutex m_sm_context_update_promises;
  mutable std::shared_mutex m_sm_context_release_promises;

  std::map<
      uint32_t,
      boost::shared_ptr<boost::promise<pdu_session_create_sm_context_response>>>
      sm_context_create_promises;
  std::map<
      uint32_t,
      boost::shared_ptr<boost::promise<pdu_session_update_sm_context_response>>>
      sm_context_update_promises;
  std::map<
      uint32_t, boost::shared_ptr<
                    boost::promise<pdu_session_release_sm_context_response>>>
      sm_context_release_promises;

  smf_profile nf_instance_profile;  // SMF profile
  std::string smf_instance_id;      // SMF instance id
  timer_id_t timer_nrf_heartbeat;

  std::map<
      std::string,  // ueid
      std::vector<nlohmann::json>>
      m_database;
  /*
   * Apply the config from the configuration file for DNN pools
   * @param [const flexcn_config &cfg] cfg
   * @return
   */
  // int apply_config(const flexcn_config& cfg);

 public:
  explicit flexcn_app(const std::string& config_file);
  flexcn_app(flexcn_app const&) = delete;

  virtual ~flexcn_app() {
    Logger::flexcn_app().debug("Delete FLEXCN_APP instance...");
    // TODO: Unregister NRF
  }

  void operator=(flexcn_app const&) = delete;

  void add_data_event(const std::string& dataEvent);
  void summarize();
  nlohmann::json get_summarization_in_json_format();

  /*
   * Handle ITTI message (N4 Session Modification Response)
   * @param [itti_n4_session_modification_response&] snm
   * @return void
   */
  void handle_itti_msg(itti_n4_session_modification_response& snm);

  /*
   * Handle ITTI message (N4 Session Report Request)
   * @param [itti_n4_association_setup_request&] snd
   * @return void
   */
  void handle_itti_msg(itti_n4_session_deletion_response& snd);

  /*
   * Handle ITTI message (N4 Session Report Request)
   * @param [itti_n4_association_setup_request&] snr
   * @return void
   */
  void handle_itti_msg(std::shared_ptr<itti_n4_session_report_request> snr);

  /*
   * Handle ITTI message (N4 Association Setup Request)
   * @param [itti_n4_association_setup_request&] sna
   * @return void
   */
  void handle_itti_msg(itti_n4_association_setup_request& sna);

  /*
   * Handle ITTI message (N4 Node Failure)
   * @param [itti_n4_node_failure&] snf
   * @return void
   */
  void handle_itti_msg(std::shared_ptr<itti_n4_node_failure> snf);


  /*
   * Restore a N4 Session
   * @param [const seid_t &] seid: Session ID to be restored
   * @return void
   */
  void restore_n4_sessions(const seid_t& seid) const;

  /*
   * Handle Event Exposure Msg from AMF
   * @param [std::shared_ptr<itti_sbi_event_exposure_request>&] Request message
   * @return [evsub_id_t] ID of the created subscription
   */
  evsub_id_t handle_event_exposure_subscription(
      std::shared_ptr<itti_sbi_event_exposure_request> msg);

  bool handle_nf_status_notification(
      std::shared_ptr<itti_sbi_notification_data>& msg,
      oai::flexcn_server::model::ProblemDetails& problem_details,
      uint8_t& http_code);

  // Hung: adding new record to the cntable
  // void add_cn_record(CNRecord &cn_record);

  // dump this table to a file or print these records into screen
  // for further analysis.
  // void print_record();


  /*
   * To store a promise of a PDU Session Create SM Contex Response to be
   * triggered when the result is ready
   * @param [uint32_t] id: promise id
   * @param [boost::shared_ptr<
   * boost::promise<pdu_session_create_sm_context_response> >&] p: pointer to
   * the promise
   * @return void
   */
  void add_promise(
      uint32_t id,
      boost::shared_ptr<boost::promise<pdu_session_create_sm_context_response>>&
          p);

  /*
   * To store a promise of a PDU Session Update SM Contex Response to be
   * triggered when the result is ready
   * @param [uint32_t] id: promise id
   * @param [boost::shared_ptr<
   * boost::promise<pdu_session_update_sm_context_response> >&] p: pointer to
   * the promise
   * @return void
   */
  void add_promise(
      uint32_t id,
      boost::shared_ptr<boost::promise<pdu_session_update_sm_context_response>>&
          p);

  /*
   * To store a promise of a PDU Session Release SM Context Response to be
   * triggered when the result is ready
   * @param [uint32_t] id: promise id
   * @param [boost::shared_ptr<
   * boost::promise<pdu_session_release_sm_context_response> >&] p: pointer to
   * the promise
   * @return void
   */
  void add_promise(
      uint32_t id,
      boost::shared_ptr<
          boost::promise<pdu_session_release_sm_context_response>>& p);

  // FlexCN
  void trigger_pdu_session_status_notification_subscribe();
};
}  // namespace flexcn
#include "flexcn_config.hpp"

#endif /* FILE_FLEXCN_APP_HPP_SEEN */
