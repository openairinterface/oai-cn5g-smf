/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_SMF_EVENT_SIG_HPP_SEEN
#define FILE_SMF_EVENT_SIG_HPP_SEEN

#include <boost/signals2.hpp>
#include <string>

#include "3gpp_24.007.hpp"
#include "EventNotification.h"

namespace bs2 = boost::signals2;

namespace oai::app::smf {

// Signal for PDU session status
// SCID, PDU Session Status, HTTP version
typedef bs2::signal_type<
    void(scid_t, const std::string&),
    bs2::keywords::mutex_type<bs2::dummy_mutex>>::type sm_context_status_sig_t;

// Signal for Event exposure
// PDU session Release, SCID, HTTP version
typedef bs2::signal_type<
    void(scid_t, uint8_t), bs2::keywords::mutex_type<bs2::dummy_mutex>>::type
    ee_pdu_session_release_sig_t;

// TODO: ee_ue_ip_address_change_sig_t; //UI IP Address, UE ID
// Signal for Event exposure
// UE Addr Change, SUPI, PDU SessionID, HTTP version
typedef bs2::signal_type<
    void(scid_t, uint8_t), bs2::keywords::mutex_type<bs2::dummy_mutex>>::type
    ee_ue_ip_change_sig_t;

// TODO: Access Type Change
// TODO: UP Path Change
// TODO: PLMN Change
typedef bs2::signal_type<
    void(scid_t, uint8_t), bs2::keywords::mutex_type<bs2::dummy_mutex>>::type
    ee_plmn_change_sig_t;

// TODO: Downlink data delivery status
typedef bs2::signal_type<
    void(scid_t, uint8_t), bs2::keywords::mutex_type<bs2::dummy_mutex>>::type
    ee_ddds_sig_t;

// Signal for PDU SESSION ESTABLISHMENT
// SCID, HTTP version
typedef bs2::signal_type<
    void(scid_t, uint8_t), bs2::keywords::mutex_type<bs2::dummy_mutex>>::type
    ee_pdusesest_sig_t;
// Signal for QoS Monitoring Event exposure (Usage Report)
// SEID, Event Notification Model , HTTP version
// TODO: use SCID and access PDU Session ID (need binding SCIDs - PDUSessID)
typedef bs2::signal_type<
    void(seid_t, oai::model::smf::EventNotification, uint8_t),
    bs2::keywords::mutex_type<bs2::dummy_mutex>>::type ee_qos_monitoring_sig_t;

// Signal for FlexCN event (for Event Exposure)
// SCID, HTTP version
typedef bs2::signal_type<
    void(scid_t, uint8_t), bs2::keywords::mutex_type<bs2::dummy_mutex>>::type
    ee_flexcn_sig_t;

}  // namespace oai::app::smf
#endif /* FILE_SMF_EVENT_SIG_HPP_SEEN */
