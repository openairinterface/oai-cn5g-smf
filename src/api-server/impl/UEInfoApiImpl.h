#ifndef _UEINFOAPIIMPL_H_
#define _UEINFOAPIIMPL_H_

#include "UEInfoApi.h"
#include "smf_app.hpp"

namespace oai {
namespace smf_server {
namespace api {

class UEInfoApiImpl : public oai::smf_server::api::UEInfoApi {
 public:
  UEInfoApiImpl(
      std::shared_ptr<Pistache::Rest::Router>, smf::smf_app* smf_app_inst,
      std::string address);
  ~UEInfoApiImpl() {}

  void get_ue_information(Pistache::Http::ResponseWriter& response) override;

 private:
  smf::smf_app* m_smf_app;
  std::string m_address;
};

}  // namespace api
}  // namespace smf_server
}  // namespace oai

#endif