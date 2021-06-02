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

#include "common_defs.h"
#include "itti.hpp"
#include "logger.hpp"
#include "mime_parser.hpp"
#include "smf.h"
#include "flexcn_app.hpp"
#include "flexcn_config.hpp"

extern "C" {
#include "dynamic_memory_check.h"
}

using namespace Pistache::Http;
using namespace Pistache::Http::Mime;

using namespace flexcn;
using json = nlohmann::json;

extern itti_mw* itti_inst;
extern smf_sbi* smf_sbi_inst;
extern flexcn_config flexcn_cfg;
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

      case N11_SUBSCRIBE_PDU_SESSION_STATUS_NOTIFY:
        smf_sbi_inst->subscribe_pdu_session_status_notify(
            std::static_pointer_cast<
                itti_n11_subscribe_pdu_session_status_notify>(shared_msg));
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
    Logger::smf_sbi().error("Cannot create task TASK_FLEXCN_SBI");
    throw std::runtime_error("Cannot create task TASK_FLEXCN_SBI");
  }
  Logger::smf_sbi().startup("Started");
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

  curl_global_init(CURL_GLOBAL_ALL);
  CURL* curl = curl = curl_easy_init();

  if (curl) {
    CURLcode res               = {};
    struct curl_slist* headers = nullptr;
    // headers = curl_slist_append(headers, "charsets: utf-8");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(
        curl, CURLOPT_URL, sm_context_status->amf_status_uri.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, AMF_CURL_TIMEOUT_MS);

    if (sm_context_status->http_version == 2) {
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      // we use a self-signed test server, skip verification during debugging
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      curl_easy_setopt(
          curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    }

    // Response information.
    long httpCode = {0};
    std::unique_ptr<std::string> httpData(new std::string());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    Logger::smf_sbi().debug("Response from AMF, Http Code: %d", httpCode);
    // TODO: in case of "307 temporary redirect"

    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
}

//-----------------------------------------------------------------------------------------------------
void smf_sbi::notify_subscribed_event(
    std::shared_ptr<itti_n11_notify_subscribed_event> msg) {
  Logger::smf_sbi().debug(
      "Send notification for the subscribed event to the subscription");

  int still_running = 0, numfds = 0, res = 0;
  CURLMsg* curl_msg    = nullptr;
  CURL* curl           = nullptr;
  CURLcode return_code = {};
  int http_status_code = 0, msgs_left = 0;
  CURLM* m_curl_multi = nullptr;
  char* url           = nullptr;

  std::unique_ptr<std::string> httpData(new std::string());
  std::string data;

  curl_global_init(CURL_GLOBAL_ALL);
  m_curl_multi = curl_multi_init();
  // init header
  struct curl_slist* headers = NULL;
  headers = curl_slist_append(headers, "Accept: application/json");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "charsets: utf-8");

  std::vector<std::string> bodys = {};  // store body for all the request
  for (auto i : msg->event_notifs) {
    // Fill the json part
    nlohmann::json json_data   = {};
    json_data["notifId"]       = i.get_notif_id();
    auto event_notifs          = nlohmann::json::array();
    nlohmann::json event_notif = {};
    event_notif["event"]       = i.get_smf_event();
    event_notif["pduSeId"]     = i.get_pdu_session_id();
    event_notif["supi"]        = std::to_string(i.get_supi());
    // timestamp
    std::time_t time_epoch_ntp = std::time(nullptr);
    uint64_t tv_ntp            = time_epoch_ntp + SECONDS_SINCE_FIRST_EPOCH;
    event_notif["timeStamp"]   = std::to_string(tv_ntp);
    event_notifs.push_back(event_notif);
    json_data["eventNotifs"] = event_notifs;
    std::string body         = json_data.dump();
    bodys.push_back(body);
  }

  int index = 0;
  // create and add an easy handle to a  multi curl request
  for (auto i : msg->event_notifs) {
    CURL* curl = curl_easy_init();
    if (curl) {
      std::string url = i.get_notif_uri();
      Logger::smf_sbi().debug(
          "Send notification to NF with URI: %s", url.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPPOST, 1);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 100L);
      // Hook up data handling function.
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &callback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, bodys.at(index).length());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodys.at(index).c_str());
    }
    curl_multi_add_handle(m_curl_multi, curl);
    index++;
  }

  curl_multi_perform(m_curl_multi, &still_running);
  // block until activity is detected on at least one of the handles or
  // MAX_WAIT_MSECS has passed.
  do {
    res = curl_multi_wait(m_curl_multi, NULL, 0, 1000, &numfds);
    if (res != CURLM_OK) {
      Logger::smf_sbi().debug("curl_multi_wait() returned %d!", res);
    }
    curl_multi_perform(m_curl_multi, &still_running);
  } while (still_running);

  // process multiple curl
  // read the messages
  while ((curl_msg = curl_multi_info_read(m_curl_multi, &msgs_left))) {
    if (curl_msg->msg == CURLMSG_DONE) {
      curl        = curl_msg->easy_handle;
      return_code = curl_msg->data.result;
      res         = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url);

      if (return_code != CURLE_OK) {
        Logger::smf_sbi().debug("CURL error code  %d!", curl_msg->data.result);
        continue;
      }
      // Get HTTP status code
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status_code);
      Logger::smf_sbi().debug("HTTP status code  %d!", http_status_code);

      // remove this handle from the multi session and end this handle
      curl_multi_remove_handle(m_curl_multi, curl);
      curl_easy_cleanup(curl);
    } else {
      Logger::smf_sbi().debug(
          "Error after curl_multi_info_read(), CURLMsg %s", curl_msg->msg);
    }
  }
}

//-----------------------------------------------------------------------------------------------------
CURL* smf_sbi::curl_create_handle(
    event_notification& ev_notif, std::string* httpData) {
  // create handle for a curl request
  struct curl_slist* headers = NULL;
  headers = curl_slist_append(headers, "Accept: application/json");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "charsets: utf-8");

  CURL* curl = curl_easy_init();

  if (curl) {
    std::string url = ev_notif.get_notif_uri();
    Logger::smf_sbi().debug("Send notification to NF with URI: %s", url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // curl_easy_setopt(curl, CURLOPT_PRIVATE, str);
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 100L);
    // Hook up data handling function.
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  }
  return curl;
}

//-----------------------------------------------------------------------------------------------------
void smf_sbi::register_nf_instance(
    std::shared_ptr<itti_n11_register_nf_instance_request> msg) {
  Logger::smf_sbi().debug(
      "Send NF Instance Registration to NRF (HTTP version %d)",
      msg->http_version);
  nlohmann::json json_data = {};
  msg->profile.to_json(json_data);

  std::string url = std::string(inet_ntoa(
                        *((struct in_addr*) &flexcn_cfg.nrf_addr.ipv4_addr))) +
                    ":" + std::to_string(flexcn_cfg.nrf_addr.port) +
                    NNRF_NFM_BASE + flexcn_cfg.nrf_addr.api_version +
                    NNRF_NF_REGISTER_URL + msg->profile.get_nf_instance_id();

  Logger::smf_sbi().debug(
      "Send NF Instance Registration to NRF, NRF URL %s", url.c_str());

  std::string body = json_data.dump();
  Logger::smf_sbi().debug(
      "Send NF Instance Registration to NRF, msg body: \n %s", body.c_str());

  curl_global_init(CURL_GLOBAL_ALL);
  CURL* curl = curl = curl_easy_init();

  if (curl) {
    CURLcode res               = {};
    struct curl_slist* headers = nullptr;
    // headers = curl_slist_append(headers, "charsets: utf-8");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, NRF_CURL_TIMEOUT_MS);

    if (msg->http_version == 2) {
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      // we use a self-signed test server, skip verification during debugging
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      curl_easy_setopt(
          curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    }

    // Response information.
    long httpCode = {0};
    std::unique_ptr<std::string> httpData(new std::string());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    Logger::smf_sbi().debug(
        "NF Instance Registration, response from NRF, HTTP Code: %d", httpCode);

    if (static_cast<http_response_codes_e>(httpCode) ==
        http_response_codes_e::HTTP_RESPONSE_CODE_CREATED) {
      json response_data = {};
      try {
        response_data = json::parse(*httpData.get());
      } catch (json::exception& e) {
        Logger::smf_sbi().warn(
            "NF Instance Registration, could not parse json from the NRF "
            "response");
      }
      Logger::smf_sbi().debug(
          "NF Instance Registration, response from NRF, json data: \n %s",
          response_data.dump().c_str());

      // send response to APP to process
      std::shared_ptr<itti_n11_register_nf_instance_response> itti_msg =
          std::make_shared<itti_n11_register_nf_instance_response>(
              TASK_SMF_SBI, TASK_FLEXCN_APP);
      itti_msg->http_response_code = httpCode;
      itti_msg->http_version       = msg->http_version;
      Logger::flexcn_app().debug("Registered FLEXCN profile (from NRF)");
      itti_msg->profile.from_json(response_data);

      int ret = itti_inst->send_msg(itti_msg);
      if (RETURNok != ret) {
        Logger::smf_sbi().error(
            "Could not send ITTI message %s to task TASK_FLEXCN_APP",
            itti_msg->get_msg_name());
      }
    } else {
      Logger::smf_sbi().warn(
          "NF Instance Registration, could not get response from NRF");
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
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

  std::string url = std::string(inet_ntoa(
                        *((struct in_addr*) &flexcn_cfg.nrf_addr.ipv4_addr))) +
                    ":" + std::to_string(flexcn_cfg.nrf_addr.port) +
                    NNRF_NFM_BASE + flexcn_cfg.nrf_addr.api_version +
                    NNRF_NF_REGISTER_URL + msg->smf_instance_id;

  Logger::smf_sbi().debug("Send NF Update to NRF, NRF URL %s", url.c_str());

  curl_global_init(CURL_GLOBAL_ALL);
  CURL* curl = curl = curl_easy_init();

  if (curl) {
    CURLcode res               = {};
    struct curl_slist* headers = nullptr;
    // headers = curl_slist_append(headers, "charsets: utf-8");
    headers = curl_slist_append(
        headers, "content-type: application/json");  // TODO: json-patch+json
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, NRF_CURL_TIMEOUT_MS);

    if (msg->http_version == 2) {
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      // we use a self-signed test server, skip verification during debugging
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      curl_easy_setopt(
          curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    }

    // Response information.
    long httpCode = {0};
    std::unique_ptr<std::string> httpData(new std::string());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    Logger::smf_sbi().debug(
        "NF Update, response from NRF, HTTP Code: %d", httpCode);

    if ((static_cast<http_response_codes_e>(httpCode) ==
         http_response_codes_e::HTTP_RESPONSE_CODE_OK) or
        (static_cast<http_response_codes_e>(httpCode) ==
         http_response_codes_e::HTTP_RESPONSE_CODE_NO_CONTENT)) {
      Logger::smf_sbi().debug("NF Update, got successful response from NRF");

      // TODO: in case of response containing NF profile
      // send response to APP to process
      std::shared_ptr<itti_n11_update_nf_instance_response> itti_msg =
          std::make_shared<itti_n11_update_nf_instance_response>(
              TASK_SMF_SBI, TASK_FLEXCN_APP);
      itti_msg->http_response_code = httpCode;
      itti_msg->http_version       = msg->http_version;
      itti_msg->smf_instance_id    = msg->smf_instance_id;

      int ret = itti_inst->send_msg(itti_msg);
      if (RETURNok != ret) {
        Logger::smf_sbi().error(
            "Could not send ITTI message %s to task TASK_FLEXCN_APP",
            itti_msg->get_msg_name());
      }
    } else {
      Logger::smf_sbi().warn("NF Update, could not get response from NRF");
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
}

//-----------------------------------------------------------------------------------------------------
void smf_sbi::deregister_nf_instance(
    std::shared_ptr<itti_n11_deregister_nf_instance> msg) {
  Logger::smf_sbi().debug(
      "Send NF De-register to NRF (HTTP version %d)", msg->http_version);

  std::string url = std::string(inet_ntoa(
                        *((struct in_addr*) &flexcn_cfg.nrf_addr.ipv4_addr))) +
                    ":" + std::to_string(flexcn_cfg.nrf_addr.port) +
                    NNRF_NFM_BASE + flexcn_cfg.nrf_addr.api_version +
                    NNRF_NF_REGISTER_URL + msg->smf_instance_id;

  Logger::smf_sbi().debug(
      "Send NF De-register to NRF (NRF URL %s)", url.c_str());

  curl_global_init(CURL_GLOBAL_ALL);
  CURL* curl = curl = curl_easy_init();

  if (curl) {
    CURLcode res               = {};
    struct curl_slist* headers = nullptr;
    // headers = curl_slist_append(headers, "charsets: utf-8");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, NRF_CURL_TIMEOUT_MS);

    if (msg->http_version == 2) {
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      // we use a self-signed test server, skip verification during debugging
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      curl_easy_setopt(
          curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    }

    // Response information.
    long httpCode = {0};
    std::unique_ptr<std::string> httpData(new std::string());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());
    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    Logger::smf_sbi().debug(
        "NF De-register, response from NRF, HTTP Code: %d", httpCode);

    if ((static_cast<http_response_codes_e>(httpCode) ==
         http_response_codes_e::HTTP_RESPONSE_CODE_OK) or
        (static_cast<http_response_codes_e>(httpCode) ==
         http_response_codes_e::HTTP_RESPONSE_CODE_NO_CONTENT)) {
      Logger::smf_sbi().debug(
          "NF De-register, got successful response from NRF");

    } else {
      Logger::smf_sbi().warn("NF De-register, could not get response from NRF");
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
}

//-----------------------------------------------------------------------------------------------------
void smf_sbi::subscribe_upf_status_notify(
    std::shared_ptr<itti_n11_subscribe_upf_status_notify> msg) {
  Logger::smf_sbi().debug(
      "Send NFSubscribeNotify to NRF to be notified when a new UPF becomes "
      "available (HTTP version %d)",
      msg->http_version);

  Logger::smf_sbi().debug(
      "Send NFStatusNotify to NRF, NRF URL %s", msg->url.c_str());

  std::string body = msg->json_data.dump();
  Logger::smf_sbi().debug(
      "Send NFStatusNotify to NRF, msg body: %s", body.c_str());

  curl_global_init(CURL_GLOBAL_ALL);
  CURL* curl = curl = curl_easy_init();

  if (curl) {
    CURLcode res               = {};
    struct curl_slist* headers = nullptr;
    // headers = curl_slist_append(headers, "charsets: utf-8");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, msg->url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, NRF_CURL_TIMEOUT_MS);

    if (msg->http_version == 2) {
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      // we use a self-signed test server, skip verification during debugging
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      curl_easy_setopt(
          curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    }

    // Response information.
    long httpCode = {0};
    std::unique_ptr<std::string> httpData(new std::string());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    Logger::smf_sbi().debug(
        "NFSubscribeNotify, response from NRF, HTTP Code: %d", httpCode);

    if ((static_cast<http_response_codes_e>(httpCode) ==
         http_response_codes_e::HTTP_RESPONSE_CODE_CREATED) or
        (static_cast<http_response_codes_e>(httpCode) ==
         http_response_codes_e::HTTP_RESPONSE_CODE_NO_CONTENT)) {
      Logger::smf_sbi().debug(
          "NFSubscribeNotify, got successful response from NRF");

    } else {
      Logger::smf_sbi().warn(
          "NFSubscribeNotify, could not get response from NRF");
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
}

//-----------------------------------------------------------------------------------------------------
// For FLEXCN
void smf_sbi::subscribe_pdu_session_status_notify(
    std::shared_ptr<itti_n11_subscribe_pdu_session_status_notify> msg) {
  Logger::smf_sbi().debug(
      "Send PDUSessionStatusSubscribeNotify to SMF to be notified when a new "
      "PDU Session status changes");

  Logger::smf_sbi().debug(
      "Send PDUSessionStatusSubscribeNotify to SMF, SMF URL %s",
      msg->url.c_str());

  std::string body = msg->json_data.dump();
  Logger::smf_sbi().debug(
      "Send PDUSessionStatusSubscribeNotify to SMF, msg body: %s",
      body.c_str());

  curl_global_init(CURL_GLOBAL_ALL);
  CURL* curl = curl = curl_easy_init();

  if (curl) {
    CURLcode res               = {};
    struct curl_slist* headers = nullptr;
    // headers = curl_slist_append(headers, "charsets: utf-8");
    headers = curl_slist_append(headers, "content-type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, msg->url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, NRF_CURL_TIMEOUT_MS);

    if (msg->http_version == 2) {
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      // we use a self-signed test server, skip verification during debugging
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      curl_easy_setopt(
          curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    }

    // Response information.
    long httpCode = {0};
    std::unique_ptr<std::string> httpData(new std::string());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    Logger::smf_sbi().debug(
        "PDUSessionStatusSubscribeNotify, response from SMF, HTTP Code: %d",
        httpCode);

    if ((static_cast<http_response_codes_e>(httpCode) ==
         http_response_codes_e::HTTP_RESPONSE_CODE_CREATED) or
        (static_cast<http_response_codes_e>(httpCode) ==
         http_response_codes_e::HTTP_RESPONSE_CODE_NO_CONTENT)) {
      Logger::smf_sbi().debug(
          "PDUSessionStatusSubscribeNotify, got successful response from SMF");

    } else {
      Logger::smf_sbi().warn(
          "PDUSessionStatusSubscribeNotify, could not get response from SMF");
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
}

//------------------------------------------------------------------------------
void smf_sbi::subscribe_sm_data() {
  // TODO:
}
