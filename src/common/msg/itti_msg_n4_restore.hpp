/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef ITTI_MSG_N4_RESTORE_HPP_INCLUDED_
#define ITTI_MSG_N4_RESTORE_HPP_INCLUDED_

#include "3gpp_29.244.h"
#include "itti_msg.hpp"
#include <set>

class itti_n4_restore : public itti_msg {
 public:
  itti_n4_restore(const task_id_t origin, const task_id_t destination)
      : itti_msg(RESTORE_N4_SESSIONS, origin, destination), sessions() {}
  itti_n4_restore(const itti_n4_restore& i)
      : itti_msg(i), sessions(i.sessions) {}
  itti_n4_restore(
      const itti_n4_restore& i, const task_id_t orig, const task_id_t dest)
      : itti_n4_restore(i) {
    origin      = orig;
    destination = dest;
  }
  const char* get_msg_name() { return "N4_RESTORE"; };

  std::set<pfcp::fseid_t> sessions;
};

#endif /* ITTI_MSG_N4_RESTORE_HPP_INCLUDED_ */
