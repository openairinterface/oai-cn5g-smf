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

/*! \file flexcn-http2-server.cpp
 \brief
 \author  Tien-Thinh NGUYEN
 \company Eurecom
 \date 2020
 \email: tien-thinh.nguyen@eurecom.fr
 */

#include "flexcn-http2-server.h"
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
#include <nlohmann/json.hpp>

#include "logger.hpp"
#include "flexcn_msg.hpp"
#include "3gpp_29.502.h"
#include "mime_parser.hpp"
#include "3gpp_29.500.h"
#include "flexcn_config.hpp"
#include "flexcn.h"
#include "3gpp_conversions.hpp"

using namespace nghttp2::asio_http2;
using namespace nghttp2::asio_http2::server;
using namespace oai::flexcn_server::model;

extern flexcn::flexcn_config flexcn_cfg;

//------------------------------------------------------------------------------
void flexcn_http2_server::start() {
  boost::system::error_code ec;

  Logger::flexcn_api_server().info("HTTP2 server started");
  // Create SM Context Request
  server.handle(
      NSMF_PDU_SESSION_BASE + flexcn_cfg.sbi_api_version +
          NSMF_PDU_SESSION_SM_CONTEXT_CREATE_URL,
      [&](const request& request, const response& response) {
        request.on_data([&](const uint8_t* data, std::size_t len) {
          if (len > 0) {
            std::string msg((char*) data, len);
            Logger::flexcn_api_server().debug("");
            Logger::flexcn_api_server().info(
                "Received a SM context create request from AMF.");
            Logger::flexcn_api_server().debug(
                "Message content \n %s", msg.c_str());
            // check HTTP method manually
            if (request.method().compare("POST") != 0) {
              // error
              Logger::flexcn_api_server().debug(
                  "This method (%s) is not supported",
                  request.method().c_str());
              response.write_head(
                  http_status_code_e::HTTP_STATUS_CODE_405_METHOD_NOT_ALLOWED);
              response.end();
              return;
            }

            SmContextMessage smContextMessage       = {};
            SmContextCreateData smContextCreateData = {};

            // simple parser
            mime_parser sp = {};
            sp.parse(msg);

            std::vector<mime_part> parts = {};
            sp.get_mime_parts(parts);
            uint8_t size = parts.size();
            Logger::flexcn_api_server().debug("Number of MIME parts %d", size);
            // at least 2 parts for Json data and N1 (+ N2)
            if (size < 2) {
              // send reply!!!
              response.write_head(
                  http_status_code_e::HTTP_STATUS_CODE_400_BAD_REQUEST);
              response.end();
              return;
            }

            // step 2. process the request
            try {
              nlohmann::json::parse(parts[0].body.c_str())
                  .get_to(smContextCreateData);
              smContextMessage.setJsonData(smContextCreateData);
              if (parts[1].content_type.compare("application/vnd.3gpp.5gnas") ==
                  0) {
                smContextMessage.setBinaryDataN1SmMessage(parts[1].body);
              } else if (
                  parts[1].content_type.compare("application/vnd.3gpp.ngap") ==
                  0) {
                smContextMessage.setBinaryDataN2SmInformation(parts[1].body);
              }
              // process the request
              this->create_sm_contexts_handler(smContextMessage, response);
            } catch (nlohmann::detail::exception& e) {
              Logger::flexcn_api_server().warn(
                  "Can not parse the json data (error: %s)!", e.what());
              response.write_head(
                  http_status_code_e::HTTP_STATUS_CODE_400_BAD_REQUEST);
              response.end();
              return;
            } catch (std::exception& e) {
              Logger::flexcn_api_server().warn("Error: %s!", e.what());
              response.write_head(
                  http_status_code_e::
                      HTTP_STATUS_CODE_500_INTERNAL_SERVER_ERROR);
              response.end();
              return;
            }
          }
        });
      });

  // Update SM Context Request
  server.handle(
      NSMF_PDU_SESSION_BASE + flexcn_cfg.sbi_api_version +
          NSMF_PDU_SESSION_SM_CONTEXT_UPDATE_URL,
      [&](const request& request, const response& response) {
        request.on_data([&](const uint8_t* data, std::size_t len) {
          if (len > 0) {
            std::string msg((char*) data, len);
            Logger::flexcn_api_server().debug("");
            Logger::flexcn_api_server().info(
                "Received a SM context update request from AMF.");
            Logger::flexcn_api_server().debug(
                "Message content \n %s", msg.c_str());

            // Get the flexcn reference context and method
            std::vector<std::string> split_result;
            boost::split(
                split_result, request.uri().path, boost::is_any_of("/"));
            if (split_result.size() != 6) {
              Logger::flexcn_api_server().warn("Requested URL is not implemented");
              response.write_head(
                  http_status_code_e::HTTP_STATUS_CODE_501_NOT_IMPLEMENTED);
              response.end();
              return;
            }

            std::string flexcn_ref = split_result[split_result.size() - 2];
            std::string method  = split_result[split_result.size() - 1];
            Logger::flexcn_api_server().info(
                "flexcn_ref %s, method %s",
                split_result[split_result.size() - 2].c_str(),
                split_result[split_result.size() - 1].c_str());

            if (method.compare("modify") == 0) {  // Update SM Context Request
              Logger::flexcn_api_server().info(
                  "Handle Update SM Context Request from AMF");

              SmContextUpdateMessage smContextUpdateMessage = {};
              SmContextUpdateData smContextUpdateData       = {};

              // simple parser
              mime_parser sp = {};
              sp.parse(msg);

              std::vector<mime_part> parts = {};
              sp.get_mime_parts(parts);
              uint8_t size = parts.size();
              Logger::flexcn_api_server().debug("Number of MIME parts %d", size);

              try {
                if (size > 0) {
                  nlohmann::json::parse(parts[0].body.c_str())
                      .get_to(smContextUpdateData);
                } else {
                  nlohmann::json::parse(msg.c_str())
                      .get_to(smContextUpdateData);
                }
                smContextUpdateMessage.setJsonData(smContextUpdateData);

                for (int i = 1; i < size; i++) {
                  if (parts[i].content_type.compare(
                          "application/vnd.3gpp.5gnas") == 0) {
                    smContextUpdateMessage.setBinaryDataN1SmMessage(
                        parts[i].body);
                    Logger::flexcn_api_server().debug("N1 SM message is set");
                  } else if (
                      parts[i].content_type.compare(
                          "application/vnd.3gpp.ngap") == 0) {
                    smContextUpdateMessage.setBinaryDataN2SmInformation(
                        parts[i].body);
                    Logger::flexcn_api_server().debug("N2 SM information is set");
                  }
                }
                this->update_sm_context_handler(
                    flexcn_ref, smContextUpdateMessage, response);

              } catch (nlohmann::detail::exception& e) {
                Logger::flexcn_api_server().warn(
                    "Can not parse the json data (error: %s)!", e.what());
                response.write_head(
                    http_status_code_e::HTTP_STATUS_CODE_400_BAD_REQUEST);
                response.end();
                return;
              } catch (std::exception& e) {
                Logger::flexcn_api_server().warn("Error: %s!", e.what());
                response.write_head(
                    http_status_code_e::
                        HTTP_STATUS_CODE_500_INTERNAL_SERVER_ERROR);
                response.end();
                return;
              }

            } else if (
                method.compare("release") == 0) {  // smContextReleaseMessage
              Logger::flexcn_api_server().info(
                  "Handle Release SM Context Request from AMF");

              SmContextReleaseMessage smContextReleaseMessage = {};

              // simple parser
              mime_parser sp = {};
              sp.parse(msg);

              std::vector<mime_part> parts = {};
              sp.get_mime_parts(parts);
              uint8_t size = parts.size();
              Logger::flexcn_api_server().debug("Number of MIME parts %d", size);

              // Getting the body param
              SmContextReleaseData smContextReleaseData = {};
              try {
                if (size > 0) {
                  nlohmann::json::parse(parts[0].body.c_str())
                      .get_to(smContextReleaseData);
                } else {
                  nlohmann::json::parse(msg.c_str())
                      .get_to(smContextReleaseData);
                }

                smContextReleaseMessage.setJsonData(smContextReleaseData);

                for (int i = 1; i < size; i++) {
                  if (parts[i].content_type.compare(
                          "application/vnd.3gpp.ngap") == 0) {
                    smContextReleaseMessage.setBinaryDataN2SmInformation(
                        parts[i].body);
                    Logger::flexcn_api_server().debug("N2 SM information is set");
                  }
                }

                this->release_sm_context_handler(
                    flexcn_ref, smContextReleaseMessage, response);

              } catch (nlohmann::detail::exception& e) {
                Logger::flexcn_api_server().warn(
                    "Can not parse the json data (error: %s)!", e.what());
                response.write_head(
                    http_status_code_e::HTTP_STATUS_CODE_400_BAD_REQUEST);
                response.end();
                return;
              } catch (std::exception& e) {
                Logger::flexcn_api_server().warn("Error: %s!", e.what());
                response.write_head(
                    http_status_code_e::
                        HTTP_STATUS_CODE_500_INTERNAL_SERVER_ERROR);
                response.end();
                return;
              }

            } else if (
                method.compare("retrieve") == 0) {  // smContextRetrieveData
              // TODO: retrieve_sm_context_handler

            } else {  // Unknown method
              Logger::flexcn_api_server().warn("Unknown method");
              response.write_head(
                  http_status_code_e::HTTP_STATUS_CODE_405_METHOD_NOT_ALLOWED);
              response.end();
              return;
            }
          }
        });
      });

  if (server.listen_and_serve(ec, m_address, std::to_string(m_port))) {
    std::cerr << "HTTP Server error: " << ec.message() << std::endl;
  }
}

//------------------------------------------------------------------------------
void flexcn_http2_server::create_sm_contexts_handler(
    const SmContextMessage& smContextMessage, const response& response) {
  }

//------------------------------------------------------------------------------
void flexcn_http2_server::update_sm_context_handler(
    const std::string& flexcn_ref,
    const SmContextUpdateMessage& smContextUpdateMessage,
    const response& response) {
}

//------------------------------------------------------------------------------
void flexcn_http2_server::release_sm_context_handler(
    const std::string& flexcn_ref,
    const SmContextReleaseMessage& smContextReleaseMessage,
    const response& response) {
}

//------------------------------------------------------------------------------
void flexcn_http2_server::stop() {
  server.stop();
}
