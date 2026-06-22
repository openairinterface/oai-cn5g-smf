/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_SMF_HTTP2_SERVER_SEEN
#define FILE_SMF_HTTP2_SERVER_SEEN

#include <nghttp2/asio_http2_server.h>

#include "NFStatusNotifyApiImpl.h"
#include "NsmfEventExposure.h"
#include "SmContextMessage.h"
#include "SmContextReleaseMessage.h"
#include "SmContextUpdateMessage.h"
#include "SmPolicyNotification.h"
#include "SmPolicyDecision.h"
#include "N1N2MsgTxfrFailureNotification.h"
#include "smf.h"
#include "smf_app.hpp"
#include "uint_generator.hpp"
#include "TerminationNotification.h"

using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::server;
using namespace oai::_3gpp::model;
using namespace oai::_3gpp::model;

class smf_http2_server {
 public:
  smf_http2_server(
      std::string addr, uint32_t port, oai::app::smf::smf_app* smf_app_inst)
      : m_address(addr),
        m_port(port),
        server(),
        m_smf_app(smf_app_inst),
        running_server(false) {}
  virtual ~smf_http2_server() = default;
  void start();
  void init(size_t thr) {}
  void create_sm_contexts_handler(
      const SmContextMessage& smContextMessage, const response& response);
  void update_sm_context_handler(
      const std::string& smf_ref,
      const SmContextUpdateMessage& smContextUpdateMessage,
      const response& response);

  void release_sm_context_handler(
      const std::string& smf_ref,
      const SmContextReleaseMessage& smContextReleaseMessage,
      const response& response);

  void nf_status_notify_handler(
      const NotificationData& notificationData, const response& response);

  void create_event_subscription_handler(
      const NsmfEventExposure& smfCreateEventSubscription,
      const response& response);

  void get_configuration_handler(const response& response);
  void update_configuration_handler(
      nlohmann::json& configuration_info, const response& response);

  void modify_sm_context_handler(
      const std::string& scid,
      const oai::_3gpp::model::SmPolicyNotification& smPolicyNotification,
      const response& response);

  void terminate_policy_notification_handler(
      const std::string& scid,
      const oai::_3gpp::model::TerminationNotification& terminationNotification,
      const response& response);

  // N1N2 Message Transfer Failure Notification callback (from AMF).
  // Posts an ITTI message to TASK_SMF_APP (paging-state mutations must run on
  // TASK_SMF_APP) and replies 204 No Content. Does NOT call smf_app inline.
  void n1n2_message_transfer_failure_handler(
      const std::string& ue_id,
      const oai::_3gpp::model::N1N2MsgTxfrFailureNotification& notif,
      const response& response);

  void stop();

 private:
  oai::utils::uint_generator<uint32_t> m_promise_id_generator;
  std::string m_address;
  uint32_t m_port;
  http2 server;
  oai::app::smf::smf_app* m_smf_app;
  bool running_server;

 protected:
};

#endif
