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

/*
 * SMFConfiguration.h
 *
 *
 */

#ifndef SMFConfigurationApi_H_
#define SMFConfigurationApi_H_

#include <pistache/http.h>
#include <pistache/router.h>
#include <pistache/http_headers.h>
#include <pistache/optional.h>

#include "ProblemDetails.h"
#include "smf.h"
#include "DnnConfigurationMessage.h"

namespace oai {
namespace smf_server {
namespace api {

using namespace oai::smf_server::model;

class SMFConfigurationApi {
 public:
  SMFConfigurationApi(std::shared_ptr<Pistache::Rest::Router>);
  virtual ~SMFConfigurationApi() {}
  void init();

  const std::string base = NSMF_SMF_CONFIGURATION_BASE;

 private:
  void setupRoutes();

  // Add a new DNN configuration
  void add_dnn_configuration_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);
  /*  // Update a DNN configuration
    void update_dnn_configuration_handler(
        const Pistache::Rest::Request& request,
        Pistache::Http::ResponseWriter response);
    // Remove a DNN configuration
    void delete_dnn_configuration_handler(
        const Pistache::Rest::Request& request,
        Pistache::Http::ResponseWriter response);
    // Retrieve a DNN configuration
    void retrieve_dnn_configuration_handler(
        const Pistache::Rest::Request& request,
        Pistache::Http::ResponseWriter response);
        */
  // Retrieve all DNN configuration
  void retrieve_dnn_configurations_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);

  void dnn_configuration_api_default_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);

  std::shared_ptr<Pistache::Rest::Router> router;

  // Add a new DNN configuration
  virtual void add_dnn_configuration(
      const DnnConfigurationMessage& dnnConfigurationMessage,
      Pistache::Http::ResponseWriter& response) = 0;

  /*
    // Update a DNN configuration
    virtual void update_dnn_configuration(
                    const std::string& dnnConfigurationRef,
                    const DnnConfigurationMessage& dnnConfigurationMessage,
                             Pistache::Http::ResponseWriter& response);
    // Remove a DNN configuration
    virtual void delete_dnn_configuration(
                    const std::string& dnnConfigurationRef,
                    const DnnConfigurationMessage& dnnConfigurationMessage,
                             Pistache::Http::ResponseWriter& response);
    // Retrieve a DNN configuration
  virtual void retrieve_dnn_configuration(
      const std::string& dnnConfigurationRef,
      const DnnConfigurationMessage& dnnConfigurationMessage,
      Pistache::Http::ResponseWriter& response);
  */
  // Retrieve all DNN configuration
  virtual void retrieve_dnn_configurations(
      const DnnConfigurationMessage& dnnConfigurationMessage,
      Pistache::Http::ResponseWriter& response) = 0;
};

}  // namespace api
}  // namespace smf_server
}  // namespace oai

#endif /* SMFConfigurationApi_H_ */
