#ifndef _UEINFO_H_
#define _UEINFO_H_

#include <pistache/http.h>
#include <pistache/router.h>
#include <pistache/http_headers.h>
#include <pistache/optional.h>

namespace oai {
namespace smf_server {
namespace api {

class UEInfoApi {
 public:
  UEInfoApi(std::shared_ptr<Pistache::Rest::Router>);
  virtual ~UEInfoApi() {}
  void init();

  const std::string base = "/openxg-smf/";

 private:
  void setupRoutes();
  std::shared_ptr<Pistache::Rest::Router> router;

  // Get ue msg
  void get_ue_information_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);
  void ue_information_default_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);
  virtual void get_ue_information(Pistache::Http::ResponseWriter& response) = 0;
};

}  // namespace api
}  // namespace smf_server
}  // namespace oai

#endif