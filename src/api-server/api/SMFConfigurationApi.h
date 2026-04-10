/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef SMFConfigurationApi_H_
#define SMFConfigurationApi_H_

#include <pistache/http.h>
#include <pistache/http_headers.h>
#include <pistache/optional.h>
#include <pistache/router.h>

#include <nlohmann/json.hpp>

#include "smf.h"
#include "smf_sbi_helper.hpp"

namespace oai::smf_server::api {

class SMFConfigurationApi {
 public:
  SMFConfigurationApi(std::shared_ptr<Pistache::Rest::Router>);
  virtual ~SMFConfigurationApi() {}
  void init();

  const std::string base = oai::smf::api::smf_sbi_helper::SmfConfBase();

 private:
  void setupRoutes();

  void read_configuration_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);
  void update_configuration_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);
  void configuration_api_default_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);

  std::shared_ptr<Pistache::Rest::Router> router;

  virtual void read_configuration(Pistache::Http::ResponseWriter& response) = 0;
  virtual void update_configuration(
      nlohmann::json& configuration_info,
      Pistache::Http::ResponseWriter& response) = 0;
};

}  // namespace oai::smf_server::api

#endif
