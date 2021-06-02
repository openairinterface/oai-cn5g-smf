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
#include "smf_paa_dynamic.hpp"
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

/*
//------------------------------------------------------------------------------
int flexcn_app::apply_config(const flexcn_config& cfg) {
  Logger::flexcn_app().info("Apply config...");

  paa_t paa = {};
  for (int ia = 0; ia < cfg.num_dnn; ia++) {
    if (cfg.dnn[ia].pool_id_iv4 >= 0) {
      int pool_id = cfg.dnn[ia].pool_id_iv4;
      int range   = be32toh(cfg.ue_pool_range_high[pool_id].s_addr) -
                  be32toh(cfg.ue_pool_range_low[pool_id].s_addr);
      paa_dynamic::get_instance().add_pool(
          cfg.dnn[ia].dnn, pool_id, cfg.ue_pool_range_low[pool_id], range);
      // TODO: check with dnn_label
      //Logger::smf_app().info("Applied config %s", cfg.dnn[ia].dnn.c_str());
      paa.ipv4_address = cfg.ue_pool_range_low[pool_id];
    }
    if (cfg.dnn[ia].pool_id_iv6 >= 0) {
      int pool_id = cfg.dnn[ia].pool_id_iv6;
      paa_dynamic::get_instance().add_pool(
          cfg.dnn[ia].dnn, pool_id, cfg.paa_pool6_prefix[pool_id],
          cfg.paa_pool6_prefix_len[pool_id]);
      paa.ipv6_address = cfg.paa_pool6_prefix[pool_id];

      // TODO: check with dnn_label
    //  Logger::smf_app().info(
    //      "Applied config for IPv6 %s", cfg.dnn[ia].dnn.c_str());
    }
  }

  Logger::flexcn_app().info("Applied config");
  return RETURNok;
}
*/

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

      case N11_REGISTER_NF_INSTANCE_RESPONSE:
        if (itti_n11_register_nf_instance_response* m =
                dynamic_cast<itti_n11_register_nf_instance_response*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case N11_UPDATE_NF_INSTANCE_RESPONSE:
        if (itti_n11_update_nf_instance_response* m =
                dynamic_cast<itti_n11_update_nf_instance_response*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case TIME_OUT:
        if (itti_msg_timeout* to = dynamic_cast<itti_msg_timeout*>(msg)) {
          Logger::flexcn_app().info("TIME-OUT event timer id %d", to->timer_id);
          switch (to->arg1_user) {
            case TASK_FLEXCN_APP_TRIGGER_T3591:
              break;
            case TASK_FLEXCN_APP_TIMEOUT_NRF_HEARTBEAT:
              flexcn_app_inst->timer_nrf_heartbeat_timeout(
                  to->timer_id, to->arg2_user);
              break;
            case TASK_FLEXCN_APP_TIMEOUT_NRF_DEREGISTRATION:
              flexcn_app_inst->timer_nrf_deregistration(
                  to->timer_id, to->arg2_user);
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

  // Register to NRF (if this option is enabled)
  if (flexcn_cfg.register_nrf) {
    unsigned int microsecond = 10000;  // 10ms
    // usleep(microsecond);
    // register_to_nrf();
  }

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
void flexcn_app::handle_itti_msg(itti_n11_register_nf_instance_response& r) {}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(itti_n11_update_nf_instance_response& u) {}

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
void flexcn_app::timer_nrf_heartbeat_timeout(
    timer_id_t timer_id, uint64_t arg2_user) {
  Logger::flexcn_app().debug(
      "Send ITTI msg to N11 task to trigger NRF Heartbeat");

  std::shared_ptr<itti_n11_update_nf_instance_request> itti_msg =
      std::make_shared<itti_n11_update_nf_instance_request>(
          TASK_FLEXCN_APP, TASK_SMF_SBI);

  oai::flexcn_server::model::PatchItem patch_item = {};
  //{"op":"replace","path":"/nfStatus", "value": "REGISTERED"}
  patch_item.setOp("replace");
  patch_item.setPath("/nfStatus");
  patch_item.setValue("REGISTERED");
  itti_msg->patch_items.push_back(patch_item);
  itti_msg->smf_instance_id = smf_instance_id;

  int ret = itti_inst->send_msg(itti_msg);
  if (RETURNok != ret) {
    Logger::flexcn_app().error(
        "Could not send ITTI message %s to task TASK_FLEXCN_SBI",
        itti_msg->get_msg_name());
  } else {
    Logger::flexcn_app().debug(
        "Set a timer to the next Heart-beat (%d)",
        nf_instance_profile.get_nf_heartBeat_timer());
    timer_nrf_heartbeat = itti_inst->timer_setup(
        nf_instance_profile.get_nf_heartBeat_timer(), 0, TASK_FLEXCN_APP,
        TASK_FLEXCN_APP_TIMEOUT_NRF_HEARTBEAT,
        0);  // TODO arg2_user
  }
}

//---------------------------------------------------------------------------------------------
void flexcn_app::timer_nrf_deregistration(
    timer_id_t timer_id, uint64_t arg2_user) {
  Logger::flexcn_app().debug(
      "Send ITTI msg to N11 task to trigger NRF Deregistratino");
  trigger_nf_deregistration();
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

//---------------------------------------------------------------------------------------------
void flexcn_app::generate_smf_profile() {
  // TODO: remove hardcoded values
  // generate UUID
  generate_uuid();
  nf_instance_profile.set_nf_instance_id(smf_instance_id);
  nf_instance_profile.set_nf_instance_name("OAI-FLEXCN");
  nf_instance_profile.set_nf_type("FLEXCN");
  nf_instance_profile.set_nf_status("REGISTERED");
  nf_instance_profile.set_nf_heartBeat_timer(50);
  nf_instance_profile.set_nf_priority(1);
  nf_instance_profile.set_nf_capacity(100);
  nf_instance_profile.add_nf_ipv4_addresses(flexcn_cfg.sbi.addr4);

  // NF services
  nf_service_t nf_service = {};
  // HUNG-TODO: change the name of service to nflexcn
  nf_service.service_instance_id = "nsmf-pdusession";
  nf_service.service_name        = "nsmf-pdusession";
  nf_service_version_t version   = {};
  version.api_version_in_uri     = "v1";
  version.api_full_version       = "1.0.0";  // TODO: to be updated
  nf_service.versions.push_back(version);
  nf_service.scheme            = "http";
  nf_service.nf_service_status = "REGISTERED";
  // IP Endpoint
  ip_endpoint_t endpoint = {};
  std::vector<struct in_addr> addrs;
  nf_instance_profile.get_nf_ipv4_addresses(addrs);
  endpoint.ipv4_address = addrs[0];  // TODO: use first IP ADDR for now
  endpoint.transport    = "TCP";
  endpoint.port         = flexcn_cfg.sbi.port;
  nf_service.ip_endpoints.push_back(endpoint);

  nf_instance_profile.add_nf_service(nf_service);

  // TODO: custom info

  int i = 0;
  for (auto sms : flexcn_cfg.session_management_subscription) {
    if (i < flexcn_cfg.num_session_management_subscription)
      i++;
    else
      break;

    // SNSSAIS
    snssai_t snssai = {};
    snssai.sD       = sms.single_nssai.sD;
    snssai.sST      = sms.single_nssai.sST;
    // Verify if this SNSSAI exist
    std::vector<snssai_t> ss = {};
    nf_instance_profile.get_nf_snssais(ss);
    bool found = false;
    for (auto it : ss) {
      if ((it.sD == snssai.sD) and (it.sST == snssai.sST)) {
        found = true;
        break;
      }
    }
    if (!found) nf_instance_profile.add_snssai(snssai);

    // SMF info
    dnn_smf_info_item_t dnn_item         = {.dnn = sms.dnn};
    snssai_smf_info_item_t smf_info_item = {};
    smf_info_item.dnn_smf_info_list.push_back(dnn_item);
    smf_info_item.snssai.sD  = sms.single_nssai.sD;
    smf_info_item.snssai.sST = sms.single_nssai.sST;
    nf_instance_profile.add_smf_info_item(smf_info_item);
  }

  // Display the profile
  nf_instance_profile.display();
}

//---------------------------------------------------------------------------------------------
void flexcn_app::register_to_nrf() {
  // Create a NF profile to this instance
  generate_smf_profile();
  // Send request to N11 to send NF registration to NRF
  trigger_nf_registration_request();
}

//------------------------------------------------------------------------------
void flexcn_app::generate_uuid() {
  smf_instance_id = to_string(boost::uuids::random_generator()());
}

//------------------------------------------------------------------------------
void flexcn_app::trigger_nf_registration_request() {
  Logger::flexcn_app().debug(
      "Send ITTI msg to N11 task to trigger the registration request to NRF");

  std::shared_ptr<itti_n11_register_nf_instance_request> itti_msg =
      std::make_shared<itti_n11_register_nf_instance_request>(
          TASK_FLEXCN_APP, TASK_SMF_SBI);
  itti_msg->profile = nf_instance_profile;
  int ret           = itti_inst->send_msg(itti_msg);
  if (RETURNok != ret) {
    Logger::flexcn_app().error(
        "Could not send ITTI message %s to task TASK_FLEXCN_SBI",
        itti_msg->get_msg_name());
  }
}

//------------------------------------------------------------------------------
void flexcn_app::trigger_nf_deregistration() {
  Logger::flexcn_app().debug(
      "Send ITTI msg to N11 task to trigger the deregistration request to NRF");

  std::shared_ptr<itti_n11_deregister_nf_instance> itti_msg =
      std::make_shared<itti_n11_deregister_nf_instance>(
          TASK_FLEXCN_APP, TASK_SMF_SBI);
  itti_msg->smf_instance_id = smf_instance_id;
  int ret                   = itti_inst->send_msg(itti_msg);
  if (RETURNok != ret) {
    Logger::flexcn_app().error(
        "Could not send ITTI message %s to task TASK_FLEXCN_SBI",
        itti_msg->get_msg_name());
  }
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
