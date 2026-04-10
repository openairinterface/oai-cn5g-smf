/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_SMF_SUBSCRIPTION_HPP_SEEN
#define FILE_SMF_SUBSCRIPTION_HPP_SEEN

#include <map>
#include <memory>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "3gpp_24.007.hpp"
#include "3gpp_29.508.h"
#include "common_root_types.h"
#include "itti.hpp"

namespace oai::app::smf {

/*
 * Manage the Subscription Info
 */
class smf_subscription {
 public:
  smf_subscription() {}

 public:
  evsub_id_t sub_id;
  smf_event_t ev_type;
  std::string supi;
  std::string notif_id;
  std::string notif_uri;
  pdu_session_id_t pdu_session_id;
};

}  // namespace oai::app::smf
#endif
