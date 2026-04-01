/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef ITTI_MSG_NX_HPP_INCLUDED_
#define ITTI_MSG_NX_HPP_INCLUDED_

#include "itti_msg.hpp"
#include "smf_msg.hpp"
#include "pistache/http.h"

class itti_nx_msg : public itti_msg {
 public:
  itti_nx_msg(
      const itti_msg_type_t msg_type, const task_id_t orig,
      const task_id_t dest)
      : itti_msg(msg_type, orig, dest) {}
  itti_nx_msg(const itti_nx_msg& i) : itti_msg(i) {}
  itti_nx_msg(const itti_nx_msg& i, const task_id_t orig, const task_id_t dest)
      : itti_nx_msg(i) {
    origin      = orig;
    destination = dest;
  }
};

//-----------------------------------------------------------------------------
class itti_nx_trigger_pdu_session_modification : public itti_nx_msg {
 public:
  itti_nx_trigger_pdu_session_modification(
      const task_id_t orig, const task_id_t dest)
      : itti_nx_msg(NX_TRIGGER_SESSION_MODIFICATION, orig, dest),
        msg(),
        http_version() {}
  itti_nx_trigger_pdu_session_modification(
      const itti_nx_trigger_pdu_session_modification& i)
      : itti_nx_msg(i), msg(i.msg), http_version(i.http_version) {}
  itti_nx_trigger_pdu_session_modification(
      const itti_nx_trigger_pdu_session_modification& i, const task_id_t orig,
      const task_id_t dest)
      : itti_nx_msg(i, orig, dest), msg(), http_version(i.http_version) {}
  const char* get_msg_name() { return "NX_TRIGGER_PDU_SESSION_MODIFICATION"; };
  oai::app::smf::pdu_session_modification_network_requested msg;
  uint8_t http_version;
};

#endif /* ITTI_MSG_NX_HPP_INCLUDED_ */
