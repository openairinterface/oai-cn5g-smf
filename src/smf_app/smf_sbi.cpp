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

/*! \file smf_sbi.cpp
 \brief
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2019
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr
 */

#include "smf_sbi.hpp"

#include <stdexcept>

#include <curl/curl.h>
#include <pistache/http.h>
#include <pistache/mime.h>
#include <nlohmann/json.hpp>
#include <boost/algorithm/string/split.hpp>
//#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "common_defs.h"
#include "itti.hpp"
#include "logger.hpp"
#include "mime_parser.hpp"
#include "3gpp_conversions.hpp"
#include "smf.h"
#include "smf_app.hpp"
#include "smf_config.hpp"
#include "http_client.hpp"

extern "C" {
#include "dynamic_memory_check.h"
}

using namespace Pistache::Http;
using namespace Pistache::Http::Mime;

using namespace smf;
using namespace oai::common::sbi;
using namespace oai::http;
using json = nlohmann::json;

extern itti_mw* itti_inst;
extern smf_sbi* smf_sbi_inst;
extern std::unique_ptr<oai::config::smf::smf_config> smf_cfg;
extern std::shared_ptr<oai::http::http_client> http_client_inst;
void smf_sbi_task(void*);

// To read content of the response from AMF
static std::size_t callback(
    const char* in, std::size_t size, std::size_t num, std::string* out) {
  const std::size_t totalBytes(size * num);
  out->append(in, totalBytes);
  return totalBytes;
}

//------------------------------------------------------------------------------
void smf_sbi_task(void* args_p) {
  const task_id_t task_id = TASK_SMF_SBI;
  itti_inst->notify_task_ready(task_id);

  do {
    std::shared_ptr<itti_msg> shared_msg = itti_inst->receive_msg(task_id);
    auto* msg                            = shared_msg.get();
    switch (msg->msg_type) {
      case N11_SESSION_CREATE_SM_CONTEXT_RESPONSE:
        smf_sbi_inst->send_n1n2_message_transfer_request(
            std::static_pointer_cast<itti_n11_create_sm_context_response>(
                shared_msg));
        break;

      case NX_TRIGGER_SESSION_MODIFICATION:
        smf_sbi_inst->send_n1n2_message_transfer_request(
            std::static_pointer_cast<itti_nx_trigger_pdu_session_modification>(
                shared_msg));
        break;

      case N11_SESSION_REPORT_RESPONSE:
        smf_sbi_inst->send_n1n2_message_transfer_request(
            std::static_pointer_cast<itti_n11_session_report_request>(
                shared_msg));
        break;

      case N11_SESSION_NOTIFY_SM_CONTEXT_STATUS:
        smf_sbi_inst->send_sm_context_status_notification(
            std::static_pointer_cast<itti_n11_notify_sm_context_status>(
                shared_msg));
        break;

      case N11_NOTIFY_SUBSCRIBED_EVENT:
        smf_sbi_inst->notify_subscribed_event(
            std::static_pointer_cast<itti_n11_notify_subscribed_event>(
                shared_msg));
        break;

      case N11_REGISTER_NF_INSTANCE_REQUEST:
        smf_sbi_inst->register_nf_instance(
            std::static_pointer_cast<itti_n11_register_nf_instance_request>(
                shared_msg));
        break;

      case N11_UPDATE_NF_INSTANCE_REQUEST:
        smf_sbi_inst->update_nf_instance(
            std::static_pointer_cast<itti_n11_update_nf_instance_request>(
                shared_msg));
        break;

      case N11_DEREGISTER_NF_INSTANCE:
        smf_sbi_inst->deregister_nf_instance(
            std::static_pointer_cast<itti_n11_deregister_nf_instance>(
                shared_msg));
        break;

      case N11_SUBSCRIBE_UPF_STATUS_NOTIFY:
        smf_sbi_inst->subscribe_upf_status_notify(
            std::static_pointer_cast<itti_n11_subscribe_upf_status_notify>(
                shared_msg));
        break;

      case N10_SESSION_GET_SESSION_MANAGEMENT_SUBSCRIPTION:
        break;

      case TERMINATE:
        if (itti_msg_terminate* terminate =
                dynamic_cast<itti_msg_terminate*>(msg)) {
          Logger::smf_sbi().info("Received terminate message");
          return;
        }
        break;

      default:
        Logger::smf_sbi().info("no handler for msg type %d", msg->msg_type);
    }

  } while (true);
}

//------------------------------------------------------------------------------
smf_sbi::smf_sbi() {
  Logger::smf_sbi().startup("Starting...");
  if (itti_inst->create_task(TASK_SMF_SBI, smf_sbi_task, nullptr)) {
    Logger::smf_sbi().error("Cannot create task TASK_SMF_SBI");
    throw std::runtime_error("Cannot create task TASK_SMF_SBI");
  }
  Logger::smf_sbi().startup("Started");
}

//------------------------------------------------------------------------------
void smf_sbi::send_n1n2_message_transfer_request(
    std::shared_ptr<itti_n11_create_sm_context_response> sm_context_res) {
  Logger::smf_sbi().debug(
      "Send Communication_N1N2MessageTransfer to AMF (HTTP version %d)",
      sm_context_res->http_version);

  nlohmann::json json_data = {};
  std::string body         = {};

  sm_context_res->res.get_json_data(json_data);
  std::string json_part = json_data.dump();
  // Add N2 content if available
  auto n2_sm_found = json_data.count("n2InfoContainer");
  if (n2_sm_found > 0) {
    mime_parser::create_multipart_related_content(
        body, json_part, CURL_MIME_BOUNDARY,
        sm_context_res->res.get_n1_sm_message(),
        sm_context_res->res.get_n2_sm_information());
  } else {
    mime_parser::create_multipart_related_content(
        body, json_part, CURL_MIME_BOUNDARY,
        sm_context_res->res.get_n1_sm_message(),
        multipart_related_content_part_e::NAS);
  }

  Logger::smf_sbi().debug(
      "Send Communication_N1N2MessageTransfer to AMF, body %s", body.c_str());

  request req = http_client_inst->prepare_multipart_request(
      sm_context_res->res.get_amf_url(), body);
  response resp = http_client_inst->send_http_request(method_e::POST, req);

  Logger::smf_sbi().debug("Response data %s", resp.body);

  // Get cause from the response
  json response_data_json = {};
  try {
    response_data_json = json::parse(resp.body);
  } catch (json::exception& e) {
    Logger::smf_sbi().warn("Could not get the cause from the response");
    // Set the default Cause
    response_data_json["cause"] = "504 Gateway Timeout";
  }
  Logger::smf_sbi().debug(
      "Response from AMF, Http Code: %d, cause %s", resp.body,
      response_data_json["cause"].dump().c_str());

  // Send response to APP to process
  std::shared_ptr<itti_n11_n1n2_message_transfer_response_status> itti_msg =
      std::make_shared<itti_n11_n1n2_message_transfer_response_status>(
          TASK_SMF_SBI, TASK_SMF_APP);

  itti_msg->set_response_code(static_cast<int16_t>(resp.status_code));
  itti_msg->set_scid(sm_context_res->scid);
  itti_msg->set_procedure_type(session_management_procedures_type_e::
                                   PDU_SESSION_ESTABLISHMENT_UE_REQUESTED);
  itti_msg->set_cause(response_data_json["cause"]);
  if (sm_context_res->res.get_cause() ==
      static_cast<uint8_t>(cause_value_5gsm_e::CAUSE_255_REQUEST_ACCEPTED)) {
    itti_msg->set_msg_type(PDU_SESSION_ESTABLISHMENT_ACCEPT);
  } else {
    itti_msg->set_msg_type(PDU_SESSION_ESTABLISHMENT_REJECT);
  }

  int ret = itti_inst->send_msg(itti_msg);
  if (RETURNok != ret) {
    Logger::smf_sbi().error(
        "Could not send ITTI message %s to task TASK_SMF_APP",
        itti_msg->get_msg_name());
  }
}

//------------------------------------------------------------------------------
void smf_sbi::send_n1n2_message_transfer_request(
    std::shared_ptr<itti_nx_trigger_pdu_session_modification>
        sm_session_modification) {
  Logger::smf_sbi().debug("Send Communication_N1N2MessageTransfer to AMF");

  std::string body         = {};
  nlohmann::json json_data = {};
  std::string json_part    = {};
  sm_session_modification->msg.get_json_data(json_data);
  json_part = json_data.dump();

  // add N2 content if available
  auto n2_sm_found = json_data.count("n2InfoContainer");
  if (n2_sm_found > 0) {
    mime_parser::create_multipart_related_content(
        body, json_part, CURL_MIME_BOUNDARY,
        sm_session_modification->msg.get_n1_sm_message(),
        sm_session_modification->msg.get_n2_sm_information());
  } else {
    mime_parser::create_multipart_related_content(
        body, json_part, CURL_MIME_BOUNDARY,
        sm_session_modification->msg.get_n1_sm_message(),
        multipart_related_content_part_e::NAS);
  }

  request req = http_client_inst->prepare_multipart_request(
      sm_session_modification->msg.get_amf_url(), body);
  response resp = http_client_inst->send_http_request(method_e::POST, req);

  Logger::smf_sbi().debug("Response data %s", resp.body);

  json response_data_json = {};
  try {
    response_data_json = json::parse(resp.body);
  } catch (json::exception& e) {
    Logger::smf_sbi().warn("Could not get the cause from the response");
  }
  Logger::smf_sbi().debug("Response from AMF, Http Code: %d", resp.status_code);
}

//------------------------------------------------------------------------------
void smf_sbi::send_n1n2_message_transfer_request(
    std::shared_ptr<itti_n11_session_report_request> report_msg) {
  Logger::smf_sbi().debug(
      "Send Communication_N1N2MessageTransfer to AMF (Network-initiated "
      "Service Request)");

  std::string n2_message   = report_msg->res.get_n2_sm_information();
  nlohmann::json json_data = {};
  std::string body         = {};
  report_msg->res.get_json_data(json_data);
  std::string json_part = json_data.dump();

  // Add N1 content if available
  auto n1_sm_found = json_data.count("n1MessageContainer");
  if (n1_sm_found > 0) {
    std::string n1_message = report_msg->res.get_n1_sm_message();
    // prepare the body content for Curl
    mime_parser::create_multipart_related_content(
        body, json_part, CURL_MIME_BOUNDARY, n1_message, n2_message);
  } else {
    mime_parser::create_multipart_related_content(
        body, json_part, CURL_MIME_BOUNDARY, n2_message,
        multipart_related_content_part_e::NGAP);
  }

  request req = http_client_inst->prepare_multipart_request(
      report_msg->res.get_amf_url(), body);
  response resp = http_client_inst->send_http_request(method_e::POST, req);

  Logger::smf_sbi().debug("Response data %s", resp.body);

  json response_data_json = {};
  try {
    response_data_json = json::parse(resp.body);
  } catch (json::exception& e) {
    Logger::smf_sbi().warn("Could not get the cause from the response");
    // Set the default Cause
    response_data_json["cause"] = "504 Gateway Timeout";
  }
  Logger::smf_sbi().debug(
      "Response from AMF, Http Code: %d, cause %s", resp.status_code,
      response_data_json["cause"].dump().c_str());

  // Send response to APP to process
  std::shared_ptr<itti_n11_n1n2_message_transfer_response_status> itti_msg =
      std::make_shared<itti_n11_n1n2_message_transfer_response_status>(
          TASK_SMF_SBI, TASK_SMF_APP);

  itti_msg->set_response_code(static_cast<int16_t>(resp.status_code));
  itti_msg->set_procedure_type(
      session_management_procedures_type_e::SERVICE_REQUEST_NETWORK_TRIGGERED);
  itti_msg->set_cause(response_data_json["cause"]);
  itti_msg->set_seid(report_msg->res.get_seid());
  itti_msg->set_trxn_id(report_msg->res.get_trxn_id());

  int ret = itti_inst->send_msg(itti_msg);
  if (RETURNok != ret) {
    Logger::smf_sbi().error(
        "Could not send ITTI message %s to task TASK_SMF_APP",
        itti_msg->get_msg_name());
  }
}

//------------------------------------------------------------------------------
void smf_sbi::send_sm_context_status_notification(
    std::shared_ptr<itti_n11_notify_sm_context_status> sm_context_status) {
  Logger::smf_sbi().debug(
      "Send SM Context Status Notification to AMF(HTTP version %d)",
      sm_context_status->http_version);
  Logger::smf_sbi().debug(
      "AMF URI: %s", sm_context_status->amf_status_uri.c_str());

  nlohmann::json json_data = {};
  // Fill the json part
  json_data["statusInfo"]["resourceStatus"] =
      sm_context_status->sm_context_status;
  std::string body = json_data.dump();

  request req = http_client_inst->prepare_json_request(
      sm_context_status->amf_status_uri, body);
  response resp = http_client_inst->send_http_request(method_e::POST, req);

  Logger::smf_sbi().debug("Response code %d", resp.status_code);
  // TODO: in case of "307 temporary redirect"
}

//-----------------------------------------------------------------------------------------------------
void smf_sbi::notify_subscribed_event(
    std::shared_ptr<itti_n11_notify_subscribed_event> msg) {
  Logger::smf_sbi().debug(
      "Send notification for the subscribed event to the subscription");

  // Create and add an easy handle to a  multi curl request
  for (auto i : msg->event_notifs) {
    // Fill the json part
    nlohmann::json json_data   = {};
    json_data["notifId"]       = i.get_notif_id();
    auto event_notifs          = nlohmann::json::array();
    nlohmann::json event_notif = {};
    event_notif["event"]       = smf_event_from_enum(i.get_smf_event());
    event_notif["pduSeId"]     = i.get_pdu_session_id();
    event_notif["supi"]        = std::to_string(i.get_supi());

    if (i.is_ad_ipv4_addr_is_set()) {
      event_notif["adIpv4Addr"] = i.get_ad_ipv4_addr();
    }
    if (i.is_re_ipv4_addr_is_set()) {
      event_notif["reIpv4Addr"] = i.get_re_ipv4_addr();
    }

    // add support for plmn change.
    if (i.is_plmnid_is_set()) {
      event_notif["plmnId"] = i.get_plmnid();
    }

    // add support for ddds
    if (i.is_ddds_is_set()) {
      // TODO: change this one to the real value when finished the event for
      // ddds
      // event_notif["dddStatus"] = i.get_ddds();
      event_notif["dddStatus"] = "TRANSMITTED";
    }
    if (i.is_dnn_set()) event_notif["dnn"] = i.get_dnn();
    if (i.is_pdu_session_type_set())
      event_notif["pduSessType"] = i.get_pdu_session_type();
    if (i.is_sst_set()) {
      nlohmann::json snssai_data = {};
      snssai_data["sst"]         = i.get_sst();
      if (i.is_sd_set()) snssai_data["sd"] = i.get_sd();
      event_notif["snssai"] = snssai_data;
    }

    // customized data
    nlohmann::json customized_data = {};
    i.get_custom_info(customized_data);
    if (!customized_data.is_null())
      event_notif["customized_data"] = customized_data;
    // timestamp
    std::time_t time_epoch_ntp = std::time(nullptr);
    uint64_t tv_ntp            = time_epoch_ntp + SECONDS_SINCE_FIRST_EPOCH;
    event_notif["timeStamp"]   = std::to_string(tv_ntp);
    event_notifs.push_back(event_notif);
    json_data["eventNotifs"] = event_notifs;
    std::string body         = json_data.dump();

    request req =
        http_client_inst->prepare_json_request(i.get_notif_uri(), body);
    response resp = http_client_inst->send_http_request(method_e::POST, req);

    Logger::smf_sbi().debug("Response code %d", resp.status_code);
    Logger::smf_sbi().debug("Response data %s", resp.body);
  }
  return;
}

//-----------------------------------------------------------------------------------------------------
void smf_sbi::register_nf_instance(
    std::shared_ptr<itti_n11_register_nf_instance_request> msg) {
  Logger::smf_sbi().debug(
      "Send NF Instance Registration to NRF (HTTP version %d)",
      msg->http_version);
  nlohmann::json json_data = {};
  msg->profile.to_json(json_data);

  std::string url = get_nrf_base_url() + msg->profile.get_nf_instance_id();

  Logger::smf_sbi().debug(
      "Send NF Instance Registration to NRF, NRF URL %s", url.c_str());

  std::string body = json_data.dump();
  Logger::smf_sbi().debug(
      "Send NF Instance Registration to NRF, msg body: \n %s (bytes %d)",
      body.c_str(), body.size());

  request req   = http_client_inst->prepare_json_request(url, body);
  response resp = http_client_inst->send_http_request(method_e::PUT, req);

  Logger::smf_sbi().debug("Response data %s", resp.body);
  Logger::smf_sbi().debug(
      "NF Instance Registration, response from NRF, HTTP Code: %d",
      resp.status_code);

  std::shared_ptr<itti_n11_register_nf_instance_response> itti_msg_response =
      std::make_shared<itti_n11_register_nf_instance_response>(
          TASK_SMF_SBI, TASK_SMF_APP);
  itti_msg_response->http_response_code =
      static_cast<int16_t>(resp.status_code);
  itti_msg_response->http_version = msg->http_version;
  Logger::smf_app().debug("Registered SMF profile (from NRF)");

  if (resp.status_code == http_status_code::CREATED) {
    json response_json = {};
    try {
      response_json = json::parse(resp.body);
    } catch (json::exception& e) {
      Logger::smf_sbi().warn(
          "NF Instance Registration, could not parse json from the NRF "
          "response");
    }
    Logger::smf_sbi().debug(
        "NF Instance Registration, response from NRF, json data: \n %s",
        response_json.dump().c_str());

    itti_msg_response->profile.from_json(response_json);
  } else {
    Logger::smf_sbi().warn(
        "NF Instance Registration, could not get response from NRF");
  }

  // Send response to APP to process
  int ret = itti_inst->send_msg(itti_msg_response);
  if (RETURNok != ret) {
    Logger::smf_sbi().error(
        "Could not send ITTI message %s to task TASK_SMF_APP",
        itti_msg_response->get_msg_name());
  }
}

//-----------------------------------------------------------------------------------------------------
void smf_sbi::update_nf_instance(
    std::shared_ptr<itti_n11_update_nf_instance_request> msg) {
  Logger::smf_sbi().debug(
      "Send NF Update to NRF (HTTP version %d)", msg->http_version);

  nlohmann::json json_data = nlohmann::json::array();
  for (auto i : msg->patch_items) {
    nlohmann::json item = {};
    to_json(item, i);
    json_data.push_back(item);
  }
  std::string body = json_data.dump();
  Logger::smf_sbi().debug("Send NF Update to NRF, Msg body %s", body.c_str());

  std::string url = get_nrf_base_url() + msg->smf_instance_id;

  Logger::smf_sbi().debug("Send NF Update to NRF, NRF URL %s", url.c_str());

  request req   = http_client_inst->prepare_json_request(url, body);
  response resp = http_client_inst->send_http_request(method_e::PATCH, req);

  Logger::smf_sbi().debug("Response data %s", resp.body);
  Logger::smf_sbi().debug(
      "NF Instance Registration, response from NRF, HTTP Code: %u",
      resp.status_code);

  if ((resp.status_code == http_status_code::OK) or
      (resp.status_code == http_status_code::NO_CONTENT)) {
    Logger::smf_sbi().debug("NF Update, got successful response from NRF");

    // TODO: In case of response containing NF profile
    // Send response to APP to process
    std::shared_ptr<itti_n11_update_nf_instance_response> itti_msg =
        std::make_shared<itti_n11_update_nf_instance_response>(
            TASK_SMF_SBI, TASK_SMF_APP);
    itti_msg->http_response_code = static_cast<int16_t>(resp.status_code);
    itti_msg->http_version       = msg->http_version;
    itti_msg->smf_instance_id    = msg->smf_instance_id;

    int ret = itti_inst->send_msg(itti_msg);
    if (RETURNok != ret) {
      Logger::smf_sbi().error(
          "Could not send ITTI message %s to task TASK_SMF_APP",
          itti_msg->get_msg_name());
    }
  } else {
    Logger::smf_sbi().warn("NF Update, could not get response from NRF");
  }
}

//-----------------------------------------------------------------------------------------------------
void smf_sbi::deregister_nf_instance(
    std::shared_ptr<itti_n11_deregister_nf_instance> msg) {
  Logger::smf_sbi().debug(
      "Send NF De-register to NRF (HTTP version %d)", msg->http_version);

  std::string url = get_nrf_base_url() + msg->smf_instance_id;

  Logger::smf_sbi().debug(
      "Send NF De-register to NRF (NRF URL %s)", url.c_str());

  request req;
  req.uri       = url;
  response resp = http_client_inst->send_http_request(method_e::DELETE, req);

  Logger::smf_sbi().debug("Response data %s", resp.body);
  Logger::smf_sbi().debug(
      "NF Instance Registration, response from NRF, HTTP Code: %d",
      resp.status_code);

  if ((resp.status_code == http_status_code::OK) or
      (resp.status_code == http_status_code::NO_CONTENT)) {
    Logger::smf_sbi().debug("NF De-register, got successful response from NRF");

  } else {
    Logger::smf_sbi().warn("NF De-register, could not get response from NRF");
  }
}

//-----------------------------------------------------------------------------------------------------
void smf_sbi::subscribe_upf_status_notify(
    std::shared_ptr<itti_n11_subscribe_upf_status_notify> msg) {
  Logger::smf_sbi().debug(
      "Send NFSubscribeNotify to NRF to be notified when a new UPF becomes "
      "available (HTTP version %d)",
      msg->http_version);

  Logger::smf_sbi().debug("NRF's URL: %s", msg->url.c_str());

  std::string body = msg->json_data.dump();
  Logger::smf_sbi().debug("Message body: %s", body.c_str());

  request req   = http_client_inst->prepare_json_request(msg->url, body);
  response resp = http_client_inst->send_http_request(method_e::POST, req);

  Logger::smf_sbi().debug("Response data %s", resp.body);
  Logger::smf_sbi().debug(
      "NF Instance Registration, response from NRF, HTTP Code: %d",
      resp.status_code);

  std::shared_ptr<itti_n11_subscribe_upf_status_notify_response>
      itti_msg_response =
          std::make_shared<itti_n11_subscribe_upf_status_notify_response>(
              TASK_SMF_SBI, TASK_SMF_APP);
  itti_msg_response->http_response_code =
      static_cast<int16_t>(resp.status_code);

  if ((resp.status_code == http_status_code::CREATED) or
      (resp.status_code == http_status_code::NO_CONTENT)) {
    Logger::smf_sbi().debug(
        "NFSubscribeNotify, got successful response from NRF");
    return;
  } else {
    Logger::smf_sbi().warn(
        "NFSubscribeNotify, could not get response from NRF");
  }

  // Send response to APP to process
  int ret = itti_inst->send_msg(itti_msg_response);
  if (RETURNok != ret) {
    Logger::smf_sbi().error(
        "Could not send ITTI message %s to task TASK_SMF_APP",
        itti_msg_response->get_msg_name());
  }
}

//------------------------------------------------------------------------------
bool smf_sbi::get_sm_data(
    const supi64_t& supi, const std::string& dnn, const snssai_t& snssai,
    std::shared_ptr<session_management_subscription>& subscription,
    plmn_t plmn) {
  nlohmann::json jsonData = {};
  std::string query_str   = {};
  std::string mcc         = plmn.mcc;
  std::string mnc         = plmn.mnc;

  query_str = "?single-nssai={\"sst\":" + std::to_string(snssai.sst) +
              ",\"sd\":\"" + snssai.sd + "\"}&dnn=" + dnn +
              "&plmn-id={\"mcc\":\"" + mcc + "\",\"mnc\":\"" + mnc + "\"}";
  std::string url =
      smf_cfg->get_nf(oai::config::UDM_CONFIG_NAME)->get_sbi().get_url() +
      NUDM_SDM_BASE +
      smf_cfg->get_nf(oai::config::UDM_CONFIG_NAME)
          ->get_sbi()
          .get_api_version() +
      fmt::format(NUDM_SDM_GET_SM_DATA_URL, smf_supi64_to_string(supi)) +
      query_str;

  Logger::smf_sbi().debug("UDM's URL: %s ", url.c_str());

  request req;
  req.uri       = url;
  response resp = http_client_inst->send_http_request(method_e::GET, req);

  Logger::smf_sbi().debug("Response data %s", resp.body);
  Logger::smf_sbi().debug(
      "Session Management Subscription Data Retrieval, response from UDM, HTTP "
      "Code: %d",
      resp.status_code);

  if (resp.status_code == http_status_code::OK) {
    Logger::smf_sbi().debug("Got successful response from UDM, URL: %s ", url);
    try {
      jsonData = nlohmann::json::parse(resp.body);
    } catch (json::exception& e) {
      Logger::smf_sbi().warn("Could not parse json data from UDM");
    }
  } else {
    Logger::smf_sbi().warn(
        "Could not get response from UDM, URL %s, retry ...", url);
    // retry
    // TODO
  }

  // Process the response
  if (!jsonData.empty()) {
    if (jsonData.type() == json::value_t::array) {
      if (!jsonData[0].empty())
        jsonData = jsonData[0];  // Array with only 1 member!
    }

    Logger::smf_sbi().debug("Response from UDM %s", jsonData.dump().c_str());
    // Verify SNSSAI
    if (jsonData.find("singleNssai") == jsonData.end()) return false;
    if (jsonData["singleNssai"].find("sst") != jsonData["singleNssai"].end()) {
      uint8_t sst = jsonData["singleNssai"]["sst"].get<uint8_t>();
      if (sst != snssai.sst) {
        return false;
      }
    }
    if (jsonData["singleNssai"].find("sd") != jsonData["singleNssai"].end()) {
      std::string sd_str = jsonData["singleNssai"]["sd"];
      if (sd_str != snssai.sd) {
        return false;
      }
    }

    // Verify DNN configurations
    if (jsonData.find("dnnConfigurations") == jsonData.end()) return false;
    Logger::smf_sbi().debug(
        "DNN Configurations %s", jsonData["dnnConfigurations"].dump().c_str());

    // Retrieve SessionManagementSubscription and store in the context
    for (nlohmann::json::iterator it = jsonData["dnnConfigurations"].begin();
         it != jsonData["dnnConfigurations"].end(); ++it) {
      Logger::smf_sbi().debug("DNN %s", it.key().c_str());
      if (it.key().compare(dnn) == 0) {
        // Get DNN configuration
        try {
          std::shared_ptr<dnn_configuration_t> dnn_configuration =
              std::make_shared<dnn_configuration_t>();
          // PDU Session Type (Mandatory)
          std::string default_session_type =
              it.value()["pduSessionTypes"]["defaultSessionType"];
          Logger::smf_sbi().debug(
              "Default session type %s", default_session_type.c_str());
          pdu_session_type_t pdu_session_type(default_session_type);
          dnn_configuration->pdu_session_types.default_session_type =
              pdu_session_type;

          // SSC_Mode (Mandatory)
          std::string default_ssc_mode =
              it.value()["sscModes"]["defaultSscMode"];
          Logger::smf_sbi().debug(
              "Default SSC Mode %s", default_ssc_mode.c_str());
          ssc_mode_t ssc_mode(default_ssc_mode);
          dnn_configuration->ssc_modes.default_ssc_mode = ssc_mode;

          // 5gQosProfile (Optional)
          if (it.value().find("5gQosProfile") != it.value().end()) {
            dnn_configuration->_5g_qos_profile._5qi =
                it.value()["5gQosProfile"]["5qi"];
            dnn_configuration->_5g_qos_profile.arp.priority_level =
                it.value()["5gQosProfile"]["arp"]["priorityLevel"];
            dnn_configuration->_5g_qos_profile.arp.preempt_cap =
                it.value()["5gQosProfile"]["arp"]["preemptCap"];
            dnn_configuration->_5g_qos_profile.arp.preempt_vuln =
                it.value()["5gQosProfile"]["arp"]["preemptVuln"];
            // Optinal
            if (it.value()["5gQosProfile"].find("") !=
                it.value()["5gQosProfile"].end()) {
              dnn_configuration->_5g_qos_profile.priority_level =
                  it.value()["5gQosProfile"]["5QiPriorityLevel"];
            }
          }

          // session_ambr (Optional)
          if (it.value().find("sessionAmbr") != it.value().end()) {
            dnn_configuration->session_ambr.uplink =
                it.value()["sessionAmbr"]["uplink"];
            dnn_configuration->session_ambr.downlink =
                it.value()["sessionAmbr"]["downlink"];
            Logger::smf_sbi().debug(
                "Session AMBR Uplink %s, Downlink %s",
                dnn_configuration->session_ambr.uplink.c_str(),
                dnn_configuration->session_ambr.downlink.c_str());
          }

          // Static IP Addresses (Optional)
          if (it.value().find("staticIpAddress") != it.value().end()) {
            for (const auto& ip_addr : it.value()["staticIpAddress"]) {
              if (ip_addr.find("ipv4Addr") != ip_addr.end()) {
                std::string ue_ip_str = ip_addr["ipv4Addr"].get<std::string>();
                in_addr ue_ipv4_addr =
                    oai::utils::conv::fromString(oai::utils::trim(ue_ip_str));
                ip_address_t ue_ip = {};
                ue_ip              = ue_ipv4_addr;
                dnn_configuration->static_ip_addresses.push_back(ue_ip);
              } else if (ip_addr.find("ipv6Addr") != ip_addr.end()) {
                std::string ue_ip_str = ip_addr["ipv6Addr"].get<std::string>();
                in6_addr ue_ipv6_addr =
                    oai::utils::conv::fromStringV6(oai::utils::trim(ue_ip_str));

                ip_address_t ue_ip = {};
                ue_ip              = ue_ipv6_addr;
                dnn_configuration->static_ip_addresses.push_back(ue_ip);
              } else if (ip_addr.find("ipv6Prefix") != ip_addr.end()) {
                in6_addr ipv6_prefix;
                std::string prefix_str =
                    ip_addr["ipv6Prefix"].get<std::string>();
                std::vector<std::string> words = {};
                boost::split(
                    words, prefix_str, boost::is_any_of("/"),
                    boost::token_compress_on);
                if (words.size() != 2) {
                  Logger::smf_app().error(
                      "Bad value for UE IPv6 Prefix %s", prefix_str.c_str());
                  return RETURNerror;
                }
                ipv6_prefix = oai::utils::conv::fromStringV6(
                    oai::utils::trim(words.at(0)));

                ip_address_t ue_ip           = {};
                ipv6_prefix_t ue_ipv6_prefix = {};
                ue_ipv6_prefix.prefix_len =
                    std::stoi(oai::utils::trim(words.at(1)));
                ue_ipv6_prefix.prefix = ipv6_prefix;
                ue_ip                 = ue_ipv6_prefix;
                dnn_configuration->static_ip_addresses.push_back(ue_ip);
              }
            }
          }

          subscription->insert_dnn_configuration(it.key(), dnn_configuration);
          return true;
        } catch (nlohmann::json::exception& e) {
          Logger::smf_sbi().warn(
              "Exception message %s, exception id %d ", e.what(), e.id);
          return false;
        } catch (std::exception& e) {
          Logger::smf_sbi().warn("Exception message %s", e.what());
          return false;
        }
      }
    }
    return true;
  } else {
    return false;
  }
}

//------------------------------------------------------------------------------
void smf_sbi::subscribe_sm_data() {
  // TODO:
}

//------------------------------------------------------------------------------
std::string smf_sbi::get_nrf_base_url() {
  auto nrf_sbi = smf_cfg->get_nf(oai::config::NRF_CONFIG_NAME)->get_sbi();
  return nrf_sbi.get_url() + NNRF_NFM_BASE + nrf_sbi.get_api_version() +
         NNRF_NF_REGISTER_URL;
}
