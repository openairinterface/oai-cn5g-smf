/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_3GPP_29_518_SMF_SEEN
#define FILE_3GPP_29_518_SMF_SEEN
#include "3gpp_29.571.h"
#include "3gpp_23.003.h"

typedef struct ng_ran_target_id_s {
  global_ran_node_id_t global_ran_node_id;
  tai_t tai;
} ng_ran_target_id_t;

#endif
