/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * NFStatusNotifyApi.h
 *
 *
 */

#ifndef NFStatusNotifyApi_H_
#define NFStatusNotifyApi_H_

#include <pistache/http.h>
#include <pistache/router.h>
#include <pistache/http_headers.h>
#include <pistache/optional.h>

#include "NotificationData.h"
#include "ProblemDetails.h"
#include "smf_sbi_helper.hpp"

namespace oai {
namespace smf_server {
namespace api {

using namespace oai::model::smf;

class NFStatusNotifyApi {
 public:
  NFStatusNotifyApi(std::shared_ptr<Pistache::Rest::Router>);
  virtual ~NFStatusNotifyApi() {}
  void init();

  const std::string base = oai::smf::api::smf_sbi_helper::SmfStatusNotifyBase();

 private:
  void setupRoutes();

  void notify_nf_status_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);
  void notify_nf_status_default_handler(
      const Pistache::Rest::Request& request,
      Pistache::Http::ResponseWriter response);

  std::shared_ptr<Pistache::Rest::Router> router;

  /// <summary>
  ///
  /// </summary>
  /// <remarks>
  ///
  /// </remarks>
  /// <param name="NotificationData"></param>
  virtual void receive_nf_status_notification(
      const NotificationData& notificationData,
      Pistache::Http::ResponseWriter& response) = 0;
};

}  // namespace api
}  // namespace smf_server
}  // namespace oai

#endif /* NFStatusNotifyApi_H_ */
