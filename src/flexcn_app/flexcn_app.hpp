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

  std::map<std::string, // ueid
                std::vector<nlohmann::json>> m_database;
  /*
   * Apply the config from the configuration file for DNN pools
   * @param [const flexcn_config &cfg] cfg
   * @return
   */
  int apply_config(const flexcn_config& cfg);


 public:
  explicit flexcn_app(const std::string& config_file);
  flexcn_app(flexcn_app const&) = delete;

  virtual ~flexcn_app() {
    Logger::flexcn_app().debug("Delete FLEXCN_APP instance...");
    // TODO: Unregister NRF
  }

  void operator=(flexcn_app const&) = delete;

  void add_data_event(const std::string &dataEvent);
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
   * Handle ITTI message from N11 to update PDU session status
   * @param [itti_n11_update_pdu_session_status&] snu
   * @return void
   */
  void handle_itti_msg(itti_n11_update_pdu_session_status& snu);

  /*
   * Handle ITTI message N11 Create SM Context Response to trigger the response
   * to AMF
   * @param [itti_n11_create_sm_context_response&] snc
   * @return void
   */
  void handle_itti_msg(itti_n11_create_sm_context_response& snc);

  /*
   * Handle ITTI message N11 Update SM Context Response to trigger the response
   * to AMF
   * @param [itti_n11_update_sm_context_response&] m
   * @return void
   */
  void handle_itti_msg(itti_n11_update_sm_context_response& m);

  /*
   * Handle ITTI message N11 Release SM Context Response to trigger the response
   * to AMF
   * @param [itti_n11_release_sm_context_response&] m
   * @return void
   */
  void handle_itti_msg(itti_n11_release_sm_context_response& m);

  /*
   * Handle ITTI message from N11 (N1N2MessageTransfer Response)
   * @param [itti_n11_n1n2_message_transfer_response_status&] snm
   * @return void
   */
  void handle_itti_msg(itti_n11_n1n2_message_transfer_response_status& snm);

  /*
   * Handle ITTI message from N11 (NFRegiser Response)
   * @param [itti_n11_register_nf_instance_response&] r
   * @return void
   */
  void handle_itti_msg(itti_n11_register_nf_instance_response& r);

  /*
   * Handle ITTI message from N11 (NFUpdate Response)
   * @param [itti_n11_update_nf_instance_response&] u
   * @return void
   */
  void handle_itti_msg(itti_n11_update_nf_instance_response& u);

  /*
   * Restore a N4 Session
   * @param [const seid_t &] seid: Session ID to be restored
   * @return void
   */
  void restore_n4_sessions(const seid_t& seid) const;
  
  /*
   * Handle PDUSession_UpdateSMContextRequest from AMF
   * @param [std::shared_ptr<itti_n11_update_sm_context_request>&] Request
   * message
   * @return void
   */
  void handle_pdu_session_update_sm_context_request(
      std::shared_ptr<itti_n11_update_sm_context_request> smreq);
  /*
   * Handle PDUSession_ReleaseSMContextRequest from AMF
   * @param [std::shared_ptr<itti_n11_release_sm_context_request>&] Request
   * message
   * @return void
   */
  void handle_pdu_session_release_sm_context_request(
      std::shared_ptr<itti_n11_release_sm_context_request> smreq);

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
   * Trigger pdu session modification
   * @param [const supi_t &] supi
   * @param [const std::string &] dnn
   * @param [const pdu_session_id_t] pdu_session_id
   * @param [const snssai_t &] snssai
   * @param [const pfcp::qfi_t &] qfi
   * @return void
   */
  void trigger_pdu_session_modification(
      const supi_t& supi, const std::string& dnn,
      const pdu_session_id_t pdu_session_id, const snssai_t& snssai,
      const pfcp::qfi_t& qfi, const uint8_t& http_version);

  /*
   * Verify if SM Context is existed for this Supi
   * @param [supi_t] supi
   * @return True if existed, otherwise false
   */
  bool is_supi_2_smf_context(const supi64_t& supi) const;

  /*
   * Create/Update SMF context with the corresponding supi
   * @param [const supi_t&] supi
   * @param [std::shared_ptr<smf_context>] sc Shared_ptr Pointer to an SMF
   * context
   * @return True if existed, otherwise false
   */
  void set_supi_2_smf_context(
      const supi64_t& supi, std::shared_ptr<smf_context> sc);

  /*
   * Get SM Context
   * @param [supi_t] Supi
   * @return Shared pointer to SM context
   */
  std::shared_ptr<smf_context> supi_2_smf_context(const supi64_t& supi) const;

  /*
   * Check whether SMF uses local configuration instead of retrieving Session
   * Management Data from UDM
   * @param [const std::string&] dnn_selection_mode
   * @return True if SMF uses the local configuration to check the validity of
   * the UE request, False otherwise
   */
  bool use_local_configuration_subscription_data(
      const std::string& dnn_selection_mode);

  /*
   * Verify whether the Session Management Data is existed
   * @param [const supi_t&] SUPI
   * @param [const std::string&] DNN
   * @param [const snssai_t&] S-NSSAI
   * @return True if SMF uses the local configuration to check the validity of
   * the UE request, False otherwise
   */
  bool is_supi_dnn_snssai_subscription_data(
      const supi_t& supi, const std::string& dnn, const snssai_t& snssai) const;

  /*
   * Get the Session Management Subscription data from local configuration
   * @param [const supi_t &] SUPI
   * @param [const std::string &] DNN
   * @param [const snssai_t &] S-NSSAI
   * @param [std::shared_ptr<session_management_subscription>] subscription:
   * store subscription data if exist
   * @return True if local configuration for this session management
   * subscription exists, False otherwise
   */
  bool get_session_management_subscription_data(
      const supi64_t& supi, const std::string& dnn, const snssai_t& snssai,
      std::shared_ptr<session_management_subscription> subscription);

  /*
   * Verify whether the UE request is valid according to the user subscription
   * and with local policies
   * @param [..]
   * @return True if the request is valid, otherwise False
   */
  bool is_create_sm_context_request_valid() const;

  /*
   * Convert a string to hex representing this string
   * @param [const std::string&] input_str Input string
   * @param [std::string&] output_str String represents string in hex format
   * @return void
   */
  void convert_string_2_hex(
      const std::string& input_str, std::string& output_str);

  /*
   * Update PDU session status
   * @param [const scid_t &] id SM Context ID
   * @param [const pdu_session_status_e &] status PDU Session Status
   * @return void
   */
  void update_pdu_session_status(
      const scid_t& id, const pdu_session_status_e& status);

  /*
   * Convert N2 Info type representing by a string to  n2_sm_info_type_e
   * @param [const std::string] n2_info_type
   * @return representing of N2 info type in a form of emum
   */
  n2_sm_info_type_e n2_sm_info_type_str2e(
      const std::string& n2_info_type) const;

  /*
   * Update PDU session UpCnxState
   * @param [const scid_t] id SM Context ID
   * @param [const upCnx_state_e] status PDU Session UpCnxState
   * @return void
   */
  void update_pdu_session_upCnx_state(
      const scid_t& scid, const upCnx_state_e& state);

  /*
   * will be executed when timer T3591 expires
   * @param [timer_id_t] timer_id
   * @param [uint64_t] arg2_user
   * @return void
   */
  void timer_t3591_timeout(timer_id_t timer_id, uint64_t arg2_user);

  /*
   * will be executed when NRF Heartbeat timer expires
   * @param [timer_id_t] timer_id
   * @param [uint64_t] arg2_user
   * @return void
   */
  void timer_nrf_heartbeat_timeout(timer_id_t timer_id, uint64_t arg2_user);

  void timer_nrf_deregistration(timer_id_t timer_id, uint64_t arg2_user);
  
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

  /*
   * To trigger the response to the HTTP server by set the value of the
   * corresponding promise to ready
   * @param [const uint32_t &] http_code: Status code of HTTP response
   * @param [const uint8_t&] cause: Error cause
   * @param [const std::string &] n1_sm_msg: N1 SM message
   * @param [uint32_t &] promise_id: Promise Id
   * @return void
   */
  void trigger_create_context_error_response(
      const uint32_t& http_code, const uint8_t& cause,
      const std::string& n1_sm_msg, uint32_t& promise_id);

  /*
   * To trigger the response to the HTTP server by set the value of the
   * corresponding promise to ready
   * @param [const uint32_t &] http_code: Status code of HTTP response
   * @param [const uint8_t &] cause: Error cause
   * @param [uint32_t &] promise_id: Promise Id
   * @param [uint8_t] msg_type: Type of HTTP message (Update/Release)
   * @return void
   */
  void trigger_update_context_error_response(
      const uint32_t& http_code, const uint8_t& cause, uint32_t& promise_id);

  /*
   * To trigger the response to the HTTP server by set the value of the
   * corresponding promise to ready
   * @param [const uint32_t &] http_code: Status code of HTTP response
   * @param [const uint8_t &] cause: cause
   * @param [const std::string &] n1_sm_msg: N1 SM message
   * @param [uint32_t &] promise_id: Promise Id
   * @param [uint8_t] msg_type: Type of HTTP message (Update/Release)
   * @return void
   */
  void trigger_update_context_error_response(
      const uint32_t& http_code, const uint8_t& cause,
      const std::string& n1_sm_msg, uint32_t& promise_id);

  /*
   * To trigger the response to the HTTP server by set the value of the
   * corresponding promise to ready
   * @param [const uint32_t &] http_code: Status code of HTTP response
   * @param [uint32_t &] promise_id: Promise Id
   * @param [uint8_t] msg_type: Type of HTTP message (Create/Update/Release)
   * @return void
   */
  void trigger_http_response(
      const uint32_t& http_code, uint32_t& promise_id, uint8_t msg_type);

  /*
   * Trigger NF instance registration to NRF
   * @param [void]
   * @return void
   */
  void register_to_nrf();

  /*
   * Generate a random UUID for SMF instance
   * @param [void]
   * @return void
   */
  void generate_uuid();

  /*
   * Generate a SMF profile for this instance
   * @param [void]
   * @return void
   */
  void generate_smf_profile();

  /*
   * Send request to N11 task to trigger NF instance registration to NRF
   * @param [void]
   * @return void
   */
  void trigger_nf_registration_request();

  /*
   * Send request to N11 task to trigger NF instance deregistration to NRF
   * @param [void]
   * @return void
   */
  void trigger_nf_deregistration();

  /*
   * Send request to N11 task to trigger NFSubscribeStatus to NRF
   * @param [void]
   * @return void
   */
  void trigger_upf_status_notification_subscribe();
  //FlexCN
  void trigger_pdu_session_status_notification_subscribe();
};
}  // namespace flexcn
#include "flexcn_config.hpp"

#endif /* FILE_FLEXCN_APP_HPP_SEEN */
