/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_3GPP_29_512_SM_POLICY_SEEN
#define FILE_3GPP_29_512_SM_POLICY_SEEN

#include <string>
#include <vector>

// =========================================================================
// APPLICATION ERRORS (Subclause 5.7.3)
// =========================================================================

// Table 5.7.3-1: Application errors when PCF acts as a server @3GPP TS 29.512 V17.6.0
enum class pcf_server_application_error_e {
    UNKNOWN_ERROR                           = 0,
    USER_UNKNOWN                            = 1,   // The end user specified in the request is unknown to the PCF
    ERROR_INITIAL_PARAMETERS                = 2,   // The set of session/subscriber info needed by the PCF is incomplete or erroneous
    ERROR_TRIGGER_EVENT                     = 3,   // The session info trigger update is incoherent with previous session info
    ERROR_TRAFFIC_MAPPING_INFO_REJECTED     = 4,   // The PCF does not accept one or more traffic mapping filters from the consumer
    ERROR_CONFLICTING_REQUEST               = 5,   // UE-initiated request covers existing network-initiated allocation filters
    LATE_OVERLAPPING_REQUEST                = 6,   // Collides with an existing Policy Association with a more recent timestamp
    POLICY_CONTEXT_DENIED                   = 7,   // Request rejected due to operator policies and/or local configuration
    VALIDATION_CONDITION_NOT_MET            = 8,   // BDT policy validation conditions are not met
    PENDING_TRANSACTION                     = 9,   // Request received while another transaction is ongoing on the same policy association
    INVALID_BDT_POLICY                      = 10,  // Background data transfer policy is invalid
    EXCEEDED_UE_SLICE_DATA_RATE             = 11,  // Authorized data rate exceeds consumed data rate for that UE and network slice
    EXCEEDED_SLICE_DATA_RATE                = 12,  // Authorized data rate exceeds consumed data rate for that slice
    POLICY_ASSOCIATION_NOT_FOUND            = 13   // No policy association corresponding to the request exists in the PCF
};

static const std::vector<std::string> pcf_server_application_error_e2str = {
        "UNKNOWN_ERROR",
        "USER_UNKNOWN",
        "ERROR_INITIAL_PARAMETERS",
        "ERROR_TRIGGER_EVENT",
        "ERROR_TRAFFIC_MAPPING_INFO_REJECTED",
        "ERROR_CONFLICTING_REQUEST",
        "LATE_OVERLAPPING_REQUEST",
        "POLICY_CONTEXT_DENIED",
        "VALIDATION_CONDITION_NOT_MET",
        "PENDING_TRANSACTION",
        "INVALID_BDT_POLICY",
        "EXCEEDED_UE_SLICE_DATA_RATE",
        "EXCEEDED_SLICE_DATA_RATE",
        "POLICY_ASSOCIATION_NOT_FOUND"};

// Table 5.7.3-2: Application errors when NF service consumer (SMF) acts as a server to receive a notification @3GPP TS 29.512 V17.6.0
enum smf_server_application_error_e {
    UNKNOWN_ERROR                           = 0,
    PCC_RULE_EVENT                          = 1,   // All dynamic PCC rules in the request cannot be installed/activated
    PCC_QOS_FLOW_EVENT                      = 2,   // All dynamic PCC rules in the request cannot be enforced or modified successfully
    UE_STATUS_SUSPEND                       = 3,   // UE status is suspended; policy decisions cannot be enforced
    RULE_PERMANENT_ERROR                    = 4,   // All dynamic PCC and/or session rules cannot be installed/activated
    RULE_TEMPORARY_ERROR                    = 5,   // All dynamic PCC and/or session rules cannot be enforced or modified temporarily
    PENDING_TRANSACTION                     = 6,   // Request received on association while SMF has an ongoing transaction
    AN_GW_FAILED                            = 7    // Policy decisions cannot be enforced because the AN-Gateway has failed
};

static const std::vector<std::string> smf_server_application_error_e2str = {
        "UNKNOWN_ERROR",
        "PCC_RULE_EVENT",
        "PCC_QOS_FLOW_EVENT",
        "UE_STATUS_SUSPEND",
        "RULE_PERMANENT_ERROR",
        "RULE_TEMPORARY_ERROR",
        "PENDING_TRANSACTION",
        "AN_GW_FAILED"};

// =========================================================================
// STANDARD ENUMERATIONS
// =========================================================================

// 5.6.3.6 Enumeration: PolicyControlRequestTrigger @3GPP TS 29.512 V17.6.0
enum class policy_control_request_trigger_e {
    UNKNOWN_TRIGGER                  = 0,
    PLMN_CH                          = 1,   // PLMN Change
    RES_MO_RE                        = 2,   // UE initiated resource modification
    AN_INFO                          = 3,   // Request and report Access Network Information
    RE_TIMEOUT                       = 4,   // Request the policy based on revalidation time
    SE_AMBR_CH                       = 5,   // Session AMBR change
    DEF_QOS_CH                       = 6,   // Default QoS change
    VPLMN_QOS_CH                     = 7,   // VPLMN QoS constraints change
    RAT_CH                           = 8,   // RAT change
    AC_TY_CH                         = 9,   // Access Type change
    UE_IP_CH                         = 10,  // UE IP address change
    APP_STA                          = 11,  // Application start detection
    APP_STA_MUTED                    = 12,  // Application start detection is muted (Internal use)
    APP_STO                          = 13,  // Application stop detection
    AN_CH_COR                        = 14,  // Access Network Charging Correlation Information
    US_RE                            = 15,  // Usage Report (Accumulated Usage reached threshold)
    PRA_CH                           = 16,  // Presence Reporting Area change
    QOS_NOTIF                        = 17,  // QoS Notification Control
    QOS_MONITORING                   = 18,  // Service Data Flow QoS Monitoring
    TSN_BRIDGE_INFO                  = 19,  // TSC user plane node/port management information
    DDN_FAILURE                      = 20,  // DDN failure events
    DDN_DELIVERY_STATUS              = 21,  // DDN delivery status events
    DDN_FAILURE_CANCELLATION         = 22,  // Cancel subscription to DDN failure
    DDN_DELIVERY_STATUS_CANCELLATION = 23   // Cancel subscription to DDN delivery status
};

static const std::vector<std::string> policy_control_request_trigger_e2str = {
        "UNKNOWN_TRIGGER",
        "PLMN_CH",
        "RES_MO_RE",
        "AN_INFO",
        "RE_TIMEOUT",
        "SE_AMBR_CH",
        "DEF_QOS_CH",
        "VPLMN_QOS_CH",
        "RAT_CH",
        "AC_TY_CH",
        "UE_IP_CH",
        "APP_STA",
        "APP_STA_MUTED",
        "APP_STO",
        "AN_CH_COR",
        "US_RE",
        "PRA_CH",
        "QOS_NOTIF",
        "QOS_MONITORING",
        "TSN_BRIDGE_INFO",
        "DDN_FAILURE",
        "DDN_DELIVERY_STATUS",
        "DDN_FAILURE_CANCELLATION",
        "DDN_DELIVERY_STATUS_CANCELLATION"};

// 5.6.3.8 Enumeration: RuleStatus @3GPP TS 29.512 V17.6.0
enum class rule_status_e {
    UNKNOWN_STATUS = 0,
    ACTIVE         = 1,  // PCC Rule is active
    INACTIVE       = 2   // PCC Rule is inactive
};

static const std::vector<std::string> rule_status_e2str = {
        "UNKNOWN_STATUS", "ACTIVE", "INACTIVE"};

// 5.6.3.9 Enumeration: FailureCode @3GPP TS 29.512 V17.6.0
enum class failure_code_e {
    UNKNOWN_FAILURE        = 0,
    UNK_RULE_ID            = 1,   // Unknown rule identifier
    RA_GR_ERR              = 2,   // Rating group error
    SER_ID_ERR             = 3,   // Service identifier error
    LATE_OVERLAPPING       = 4,   // Overlapping request
    AN_GW_FAILED           = 5,   // Access Node/Gateway failure
    MISS_REDI_SER_ADDR     = 6,   // Missing redirect server address
    CM_FREE_TEXT           = 7,   // Credit management free text
    CAN_NOT_BIND           = 8,   // Cannot bind SDF filters to any QoS flow
    NO_QOS_FLOW_BOUND      = 9,   // No QoS flow bound
    FILTER_LIMIT_EXCEEDED  = 10,  // Filter limit exceeded
    RES_ALLO_FAIL          = 11,  // Resource allocation failure
    UNSUCC_QOS_VAL         = 12,  // Unsuccessful QoS validation
    INCORRECT_COND_DATA    = 13,  // Incorrect condition data
    DNAI_STEERING_ERROR    = 14,  // DNAI steering error
    TRAFFIC_STEERING_ERROR = 15,  // Traffic steering error
    APP_ID_ERR             = 16   // Application Identifier error
};

static const std::vector<std::string> failure_code_e2str = {
        "UNKNOWN_FAILURE",
        "UNK_RULE_ID",
        "RA_GR_ERR",
        "SER_ID_ERR",
        "LATE_OVERLAPPING",
        "AN_GW_FAILED",
        "MISS_REDI_SER_ADDR",
        "CM_FREE_TEXT",
        "CAN_NOT_BIND",
        "NO_QOS_FLOW_BOUND",
        "FILTER_LIMIT_EXCEEDED",
        "RES_ALLO_FAIL",
        "UNSUCC_QOS_VAL",
        "INCORRECT_COND_DATA",
        "DNAI_STEERING_ERROR",
        "TRAFFIC_STEERING_ERROR",
        "APP_ID_ERR"};

// 5.6.3.17 Enumeration: SessionRuleFailureCode @3GPP TS 29.512 V17.6.0
enum class session_rule_failure_code_e {
    UNKNOWN_SESS_FAILURE   = 0,
    UNK_SESS_RULE_ID       = 1,  // Unknown session rule identifier
    INCORRECT_UM           = 2,  // Incorrect usage monitoring
    SESS_AMBR_OUT_OF_RANGE = 3,  // Session-AMBR out of range
    DEF_QOS_OUT_OF_RANGE   = 4,  // Default QoS out of range
    AN_GW_FAILED           = 5   // Access Node/Gateway failure
};

static const std::vector<std::string> session_rule_failure_code_e2str = {
        "UNKNOWN_SESS_FAILURE",
        "UNK_SESS_RULE_ID",
        "INCORRECT_UM",
        "SESS_AMBR_OUT_OF_RANGE",
        "DEF_QOS_OUT_OF_RANGE",
        "AN_GW_FAILED"};

// 5.6.3.18 Enumeration: SteeringFunctionality @3GPP TS 29.512 V17.6.0
enum class steering_functionality_e {
    UNKNOWN_STEER_FUNC = 0,
    MPTCP              = 1,  // Multi-Path TCP Protocol steering
    ATSSS_LL           = 2   // ATSSS Low-Layer steering
};

static const std::vector<std::string> steering_functionality_e2str = {
        "UNKNOWN_STEER_FUNC", "MPTCP", "ATSSS_LL"};

// 5.6.3.19 Enumeration: SteerModeValue @3GPP TS 29.512 V17.6.0
enum class steer_mode_value_e {
    UNKNOWN_STEER_MODE = 0,
    ACTIVE_STANDBY     = 1,  // Active-Standby steering mode
    LOAD_BALANCING     = 2,  // Load-Balancing steering mode
    SMALLEST_DELAY     = 3,  // Smallest Delay steering mode
    PRIORITY_BASED     = 4   // Priority-Based steering mode
};

static const std::vector<std::string> steer_mode_value_e2str = {
        "UNKNOWN_STEER_MODE", "ACTIVE_STANDBY", "LOAD_BALANCING", "SMALLEST_DELAY", "PRIORITY_BASED"};

#endif // FILE_3GPP_29_512_SM_POLICY_SEEN