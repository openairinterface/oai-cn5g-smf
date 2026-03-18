/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef SMF_CONFIGURATION_API_IMPL_H_
#define SMF_CONFIGURATION_API_IMPL_H_

#include "smf_app.hpp"
#include <SMFConfigurationApi.h>
#include <pistache/http.h>
#include <pistache/optional.h>

namespace oai::smf_server::api {

using namespace oai::model::smf;

class SMFConfigurationApiImpl
    : public oai::smf_server::api::SMFConfigurationApi {
 private:
  oai::app::smf::smf_app* m_smf_app;

 public:
  SMFConfigurationApiImpl(
      std::shared_ptr<Pistache::Rest::Router>,
      oai::app::smf::smf_app* smf_app_inst);
  ~SMFConfigurationApiImpl() {}

  void read_configuration(Pistache::Http::ResponseWriter& response);
  void update_configuration(
      nlohmann::json& configuration_info,
      Pistache::Http::ResponseWriter& response);

 protected:
  static uint64_t generate_promise_id() {
    return oai::utils::uint_uid_generator<uint64_t>::get_instance().get_uid();
  }
};

}  // namespace oai::smf_server::api

#endif
