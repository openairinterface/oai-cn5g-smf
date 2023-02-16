#include "UEInfoApiImpl.h"

namespace oai {
namespace smf_server {
namespace api {

UEInfoApiImpl::UEInfoApiImpl(
    std::shared_ptr<Pistache::Rest::Router> rtr, smf::smf_app* smf_app_inst,
    std::string address)
    : UEInfoApi(rtr), m_smf_app(smf_app_inst), m_address(address) {}
void UEInfoApiImpl::get_ue_information(
    Pistache::Http::ResponseWriter& response) {
  response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
  nlohmann::json response_data_json = {};
  nlohmann::json json_array         = {};

  m_smf_app->handle_ue_msg_request(json_array);

  if (!json_array.empty()) {
    response_data_json["code"] = 200;
    response_data_json["msg"]  = "success";
    response_data_json["data"] = json_array;
  } else {
    response_data_json["code"] = 201;
    response_data_json["msg"]  = "no data";
    response_data_json["data"] = NULL;
  }
  try {
    response.send(Pistache::Http::Code::Ok, response_data_json.dump());
    return;
  } catch (nlohmann::json::exception& e) {
    response.send(Pistache::Http::Code::Not_Found, response_data_json.dump());
    return;
  }
}

}  // namespace api
}  // namespace smf_server
}  // namespace oai