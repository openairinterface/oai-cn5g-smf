#include "UEInfoApi.h"

#include <nlohmann/json.hpp>

namespace oai {
namespace smf_server {
namespace api {

UEInfoApi::UEInfoApi(
    std::shared_ptr<Pistache::Rest::Router> rtr) {
  router = rtr;
}

void UEInfoApi::init() {
  setupRoutes();
}

void UEInfoApi::setupRoutes() {
  using namespace Pistache::Rest;
  
  Routes::Get(
        *router, base + "/v1/uemsg", 
        Routes::bind(&UEInfoApi::get_ue_information_handler, this));
  router->addCustomHandler(Routes::bind(&UEInfoApi::ue_information_default_handler, this));
  
}

void UEInfoApi::get_ue_information_handler(const Pistache::Rest::Request &request, Pistache::Http::ResponseWriter response) {
    
    try {
         this->get_ue_information(response);
    } catch (nlohmann::detail::exception &e) {
        //send a 400 error
        response.send(Pistache::Http::Code::Bad_Request, e.what());
        return;
    } catch (Pistache::Http::HttpError &e) {
        response.send(static_cast<Pistache::Http::Code>(e.code()), e.what());
        return;
    } catch (std::exception &e) {
        response.send(Pistache::Http::Code::Internal_Server_Error, e.what());
        return;
    }
}
void UEInfoApi::ue_information_default_handler(const Pistache::Rest::Request &, Pistache::Http::ResponseWriter response) {
    response.send(Pistache::Http::Code::Not_Found, "The requested method does not exist");
}

}
}
}