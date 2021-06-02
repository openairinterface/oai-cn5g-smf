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

/*! \file flexcn_app.cpp
 \brief
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2019
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr
 */

#include "flexcn_app.hpp"

#include <boost/uuid/random_generator.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "3gpp_24.007.h"
#include "3gpp_24.501.h"
#include "3gpp_29.500.h"
#include "3gpp_29.502.h"
#include "3gpp_conversions.hpp"
#include "ProblemDetails.h"
#include "RefToBinaryData.h"
#include "SmContextCreateError.h"  //
#include "SmContextCreatedData.h"  //
#include "SmContextMessage.h"      //
#include "SmContextUpdateError.h"  //
#include "async_shell_cmd.hpp"     //
#include "common_defs.h"           //
#include "conversions.hpp"         //
#include "itti.hpp"                //
#include "itti_msg_nx.hpp"         //
#include "logger.hpp"              // common
#include "pfcp.hpp"                // pfcp
#include "smf.h"                   // common
#include "smf_event.hpp"           //smf_app
#include "smf_sbi.hpp"
#include "smf_n4.hpp"
#include "string.hpp"  //

#include "EventNotification.h"

extern "C" {
#include "dynamic_memory_check.h"
}

using namespace flexcn;

extern util::async_shell_cmd* async_shell_cmd_inst;
extern flexcn_app* flexcn_app_inst;
extern flexcn_config flexcn_cfg;
smf_n4* smf_n4_inst   = nullptr;
smf_sbi* smf_sbi_inst = nullptr;
extern itti_mw* itti_inst;

void flexcn_app_task(void*);

nlohmann::json flexcn_app::get_summarization_in_json_format() {
  nlohmann::json j = {};
  j["num_ue"]      = int(m_database.size());
  try {
    j["supi_list"] = nlohmann::json::array();
    j["ue_data"]   = nlohmann::json::array();

    for (const auto& [supi, data] : m_database) {
      j["supi_list"].push_back(supi);  // .c_str()
      nlohmann::json t = data;
      j["ue_data"].push_back(t);
    }
  } catch (nlohmann::detail::exception& e) {
    Logger::smf_api_server().info("Parsing this event failed");
    Logger::smf_api_server().info(e.what());
  }

  return j;
};

// ----------------------------------------------------------
void flexcn_app::add_data_event(const std::string& data_event) {
  Logger::flexcn_app().info("Add/Merge the following record to database : ");
  Logger::flexcn_app().info(data_event.c_str());

  // parse string to json

  // TODO: valid the source of data
  // store the list of valid sources inside the flexcn-app and verify?
  nlohmann::json json_data;
  try {
    json_data = nlohmann::json::parse(data_event.c_str());
  } catch (nlohmann::json::parse_error& ex) {
    Logger::flexcn_app().error("Invalid json format:");
    Logger::flexcn_app().error(data_event.c_str());
    Logger::flexcn_app().error(
        "Invalid json format: parse error at byte %i !", ex.byte);
  }

  if (!json_data.contains("eventNotifs")) {
    Logger::flexcn_app().error("Message don't contains eventNotifs key!");
    Logger::flexcn_app().error(data_event.c_str());
    return;
  }
  if (!json_data["eventNotifs"].is_array()) {
    Logger::flexcn_app().error("Event Notifs field is not an array");
    Logger::flexcn_app().error(data_event.c_str());
    return;
  }

  // TODO: must check of json object of eventNotifs is array
  for (auto field : json_data["eventNotifs"]) {
    if (!field.contains("supi")) {
      Logger::flexcn_app().info("Event doesn't contains supi");
      Logger::flexcn_app().info(field.dump().c_str());
    }
    // adding this to database
    // CNRecord t;
    // field.get_to(t);
    m_database[field["supi"]].push_back(field);
    // todo: field fusion
  }
  // get UE ID from data_event
  // std::string supi = data_event.getSupi();

  // get the item from the list of app that has the same ueid
  // data structure: ueid as key,
  // m_database[supi].merge(data_event);

  // update the data related to this ue with the new data fields.
  // if event == event_0:
  //     update these field of data that related to this ueid address
  // if ueid in m_datase:
  //     current_ue_context = m_database.get(ueid)
  //     current_ue_context.update(data_event)
  // else:
  //     new_context = new ue_context()
  // how far should we update these items.
  // done the process.
}

//------------------------------------------------------------------------------
void flexcn_app::restore_n4_sessions(const seid_t& seid) const {
  std::shared_lock lock(m_seid2smf_context);
  // TODO
}

//------------------------------------------------------------------------------
void flexcn_app_task(void*) {
  const task_id_t task_id = TASK_FLEXCN_APP;
  itti_inst->notify_task_ready(task_id);

  do {
    std::shared_ptr<itti_msg> shared_msg = itti_inst->receive_msg(task_id);
    auto* msg                            = shared_msg.get();
    switch (msg->msg_type) {
      case N4_SESSION_MODIFICATION_RESPONSE:
        if (itti_n4_session_modification_response* m =
                dynamic_cast<itti_n4_session_modification_response*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case N4_SESSION_DELETION_RESPONSE:
        if (itti_n4_session_deletion_response* m =
                dynamic_cast<itti_n4_session_deletion_response*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case N4_SESSION_REPORT_REQUEST:
        flexcn_app_inst->handle_itti_msg(
            std::static_pointer_cast<itti_n4_session_report_request>(
                shared_msg));
        break;

      case N4_NODE_FAILURE:
        flexcn_app_inst->handle_itti_msg(
            std::static_pointer_cast<itti_n4_node_failure>(shared_msg));
        break;

      case TIME_OUT:
        if (itti_msg_timeout* to = dynamic_cast<itti_msg_timeout*>(msg)) {
          Logger::flexcn_app().info("TIME-OUT event timer id %d", to->timer_id);
          switch (to->arg1_user) {
            case TASK_FLEXCN_APP_TRIGGER_T3591:
              break;
            default:;
          }
        }
        break;

      case TERMINATE:
        if (itti_msg_terminate* terminate =
                dynamic_cast<itti_msg_terminate*>(msg)) {
          Logger::flexcn_app().info("Received terminate message");
          flexcn_app_inst->summarize();
          return;
        }
        break;

      case HEALTH_PING:
        break;

      default:
        Logger::flexcn_app().info("no handler for msg type %d", msg->msg_type);
    }
  } while (true);
}

void flexcn_app::summarize() {
  Logger::flexcn_app().info("Number of ue: %d", m_database.size());
  Logger::flexcn_app().info("List of ue's supi:");
  for (const auto& [supi, data] : m_database) {
    Logger::flexcn_app().info("   %s", supi.c_str());
    if (data.size() > 0) {
      Logger::flexcn_app().info(
          "The first event of this ue: \n %s", data[0].dump(4).c_str());
    }
  }
}

//------------------------------------------------------------------------------
flexcn_app::flexcn_app(const std::string& config_file)
    : m_seid2smf_context(),
      m_supi2smf_context(),
      m_scid2smf_context(),
      m_sm_context_create_promises(),
      m_sm_context_update_promises(),
      m_sm_context_release_promises() {
  Logger::flexcn_app().startup("Starting...");

  supi2smf_context  = {};
  set_seid_n4       = {};
  seid_n4_generator = 0;

  // apply_config(flexcn_cfg);

  if (itti_inst->create_task(TASK_FLEXCN_APP, flexcn_app_task, nullptr)) {
    Logger::flexcn_app().error("Cannot create task TASK_FLEXCN_APP");
    throw std::runtime_error("Cannot create task TASK_FLEXCN_APP");
  }

  try {
    // smf_n4_inst  = new smf_n4();
    smf_sbi_inst = new smf_sbi();
  } catch (std::exception& e) {
    Logger::flexcn_app().error("Cannot create FLEXCN_APP: %s", e.what());
    throw;
  }

  // FLEXCN
  // Trigger SMFEventExpose subscription / to be noticed when a PDU session
  // status changes
  trigger_pdu_session_status_notification_subscribe();

  Logger::flexcn_app().startup("Started");
}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(
    itti_n4_session_modification_response& smresp) {}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(itti_n4_session_deletion_response& smresp) {}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(
    std::shared_ptr<itti_n4_session_report_request> snr) {}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(std::shared_ptr<itti_n4_node_failure> snf) {}

//------------------------------------------------------------------------------
evsub_id_t flexcn_app::handle_event_exposure_subscription(
    std::shared_ptr<itti_sbi_event_exposure_request> msg) {}

//------------------------------------------------------------------------------
bool flexcn_app::handle_nf_status_notification(
    std::shared_ptr<itti_sbi_notification_data>& msg,
    oai::flexcn_server::model::ProblemDetails& problem_details,
    uint8_t& http_code) {
  return true;
}

//---------------------------------------------------------------------------------------------
void flexcn_app::add_promise(
    uint32_t id,
    boost::shared_ptr<boost::promise<pdu_session_create_sm_context_response>>&
        p) {
  std::unique_lock lock(m_sm_context_create_promises);
  sm_context_create_promises.emplace(id, p);
}

//---------------------------------------------------------------------------------------------
void flexcn_app::add_promise(
    uint32_t id,
    boost::shared_ptr<boost::promise<pdu_session_update_sm_context_response>>&
        p) {
  std::unique_lock lock(m_sm_context_update_promises);
  sm_context_update_promises.emplace(id, p);
}

//---------------------------------------------------------------------------------------------
void flexcn_app::add_promise(
    uint32_t id,
    boost::shared_ptr<boost::promise<pdu_session_release_sm_context_response>>&
        p) {
  std::unique_lock lock(m_sm_context_release_promises);
  sm_context_release_promises.emplace(id, p);
}


//------------------------------------------------------------------------------
void flexcn_app::trigger_pdu_session_status_notification_subscribe() {
  Logger::flexcn_app().debug(
      "Send ITTI msg to SBI task to subscribe to PDU Session Status "
      "notification "
      "from FLEXCN");

  std::shared_ptr<itti_n11_subscribe_pdu_session_status_notify> itti_msg =
      std::make_shared<itti_n11_subscribe_pdu_session_status_notify>(
          TASK_FLEXCN_APP, TASK_SMF_SBI);

  nlohmann::json json_data = {};
  json_data["notifUri"] =
      std::string(inet_ntoa(*((struct in_addr*) &flexcn_cfg.sbi.addr4))) + ":" +
      std::to_string(flexcn_cfg.sbi.port) +
      "/flexcn-status-notify/v1/notifid01";

  json_data["notifId"] = "notifid01";

  json_data["supi"]      = "208950000000031";
  json_data["eventSubs"] = nlohmann::json::array();
  nlohmann::json tmp     = {};
  tmp["event"]           = "PDU_SES_REL";

  nlohmann::json tmp2 = {};
  tmp2["event"]       = "UE_IP_CH";

  nlohmann::json tmp3 = {};
  tmp3["event"]       = "DDDS";

  nlohmann::json tmp4 = {};
  tmp4["event"]       = "PLMN_CH";

  nlohmann::json tmp5 = {};
  tmp5["event"]       = "FLEXCN";

  json_data["eventSubs"].push_back(tmp);
  json_data["eventSubs"].push_back(tmp2);
  json_data["eventSubs"].push_back(tmp3);
  json_data["eventSubs"].push_back(tmp4);
  json_data["eventSubs"].push_back(tmp5);

  std::string url = std::string(inet_ntoa(
                        *((struct in_addr*) &flexcn_cfg.nrf_addr.ipv4_addr))) +
                    ":" + std::to_string(flexcn_cfg.nrf_addr.port) +
                    "/nsmf_event-exposure/v1/subscriptions";

  itti_msg->url       = url;
  itti_msg->json_data = json_data;
  Logger::flexcn_app().info(json_data.dump().c_str());
  int ret = itti_inst->send_msg(itti_msg);
  if (RETURNok != ret) {
    Logger::flexcn_app().error(
        "Could not send ITTI message %s to task TASK_FLEXCN_SBI",
        itti_msg->get_msg_name());
  }
}
