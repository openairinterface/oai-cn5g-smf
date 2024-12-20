/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this
 * file except in compliance with the License. You may obtain a copy of the
 * License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "TLVEncoder.h"
#include "TLVDecoder.h"
#include "QOSRules.h"

int encode_qos_rules(
    QOSRules qosrules, uint8_t iei, uint8_t* buffer, uint32_t len) {
  uint8_t* len_qosrulesie = NULL;
  uint8_t bitstream       = 0;
  uint32_t encoded        = 0;
  int encode_result       = 0;
  int i = 0, j = 0;
  uint32_t temp = 0;

  CHECK_PDU_POINTER_AND_LENGTH_ENCODER(
      buffer,
      ((iei > 0) ? QOS_RULES_MINIMUM_LENGTH_TLVE :
                   QOS_RULES_MINIMUM_LENGTH_LVE),
      len);

  if (iei > 0) {
    *buffer = iei;
    encoded++;
  }

  len_qosrulesie = buffer + encoded;
  encoded +=
      2;  // ENCODE_U16(buffer + encoded, qosrules.lengthofqosrulesie, encoded);
  uint8_t len_pos_qos_rulesie = encoded;

  for (i = 0; i < qosrules.lengthofqosrulesie; i++) {
    ENCODE_U8(
        buffer + encoded, qosrules.qosrulesie[i].qosruleidentifer, encoded);

    uint8_t* len_qosrule     = NULL;
    uint8_t len_pos_qos_rule = 0;

    len_qosrule = buffer + encoded;
    encoded++;
    encoded++;
    len_pos_qos_rule = encoded;

    bitstream = (uint8_t) (qosrules.qosrulesie[i].ruleoperationcode << 5);
    bitstream |= (uint8_t) (qosrules.qosrulesie[i].dqrbit << 4);
    bitstream |= (uint8_t) qosrules.qosrulesie[i].numberofpacketfilters;
    ENCODE_U8(buffer + encoded, bitstream, encoded);

    if ((bitstream >> 5) ==
        MODIFY_EXISTING_QOS_RULE_AND_DELETE_PACKET_FILTERS) {
      for (j = 0; j < (bitstream & 0x0f); j++) {
        ENCODE_U8(
            buffer + encoded,
            (uint8_t) qosrules.qosrulesie[i]
                .packetfilterlist.modifyanddelete[j]
                .packetfilteridentifier,
            encoded);
      }
      ENCODE_U8(
          buffer + encoded, qosrules.qosrulesie[i].qosruleprecedence, encoded);
      ENCODE_U8(
          buffer + encoded,
          (uint8_t)(
              (qosrules.qosrulesie[i].segregation << 6) |
              (qosrules.qosrulesie[i].qosflowidentifer & 0x3f)),
          encoded);
    } else if (
        ((bitstream >> 5) == CREATE_NEW_QOS_RULE) ||
        ((bitstream >> 5) == MODIFY_EXISTING_QOS_RULE_AND_ADD_PACKET_FILTERS) ||
        ((bitstream >> 5) ==
         MODIFY_EXISTING_QOS_RULE_AND_REPLACE_ALL_PACKET_FILTERS)) {
      for (j = 0; j < (bitstream & 0x0f); j++) {
        ENCODE_U8(
            buffer + encoded,
            (uint8_t)(
                (qosrules.qosrulesie[i]
                     .packetfilterlist.create_modifyandadd_modifyandreplace[j]
                     .packetfilterdirection
                 << 4) |
                (qosrules.qosrulesie[i]
                     .packetfilterlist.create_modifyandadd_modifyandreplace[j]
                     .packetfilteridentifier &
                 0x0f)),
            encoded);

        uint8_t* len_packetfiltercontents = buffer + encoded;
        encoded++;

        *len_packetfiltercontents =
            qosrules.qosrulesie[i]
                .packetfilterlist.create_modifyandadd_modifyandreplace[j]
                .lenghtofpacketfiltercontents;

        int packetfilterconten_encoded = 0;
        int packetfiltercontent_index  = 0;

        while (packetfilterconten_encoded < *len_packetfiltercontents) {
          ENCODE_U8(
              buffer + encoded,
              qosrules.qosrulesie[i]
                  .packetfilterlist.create_modifyandadd_modifyandreplace[j]
                  .packetfiltercontents[packetfiltercontent_index]
                  .component_type,
              encoded);
          packetfilterconten_encoded++;

          if (qosrules.qosrulesie[i]
                  .packetfilterlist.create_modifyandadd_modifyandreplace[j]
                  .packetfiltercontents[packetfiltercontent_index]
                  .component_type != QOS_RULE_MATCHALL_TYPE) {
            if ((encode_result = encode_bstring(
                     qosrules.qosrulesie[i]
                         .packetfilterlist
                         .create_modifyandadd_modifyandreplace[j]
                         .packetfiltercontents[packetfiltercontent_index]
                         .component_value,
                     buffer + encoded, len - encoded)) < 0) {
              return encode_result;
            } else {
              packetfilterconten_encoded += encode_result;
              encoded += encode_result;
            }
          }
          packetfiltercontent_index++;
        }
      }

      ENCODE_U8(
          buffer + encoded, qosrules.qosrulesie[i].qosruleprecedence, encoded);
      ENCODE_U8(
          buffer + encoded,
          (uint8_t)(
              (qosrules.qosrulesie[i].segregation << 6) |
              (qosrules.qosrulesie[i].qosflowidentifer & 0x3f)),
          encoded);
    }

    // len of qos rule
    ENCODE_U16(len_qosrule, encoded - len_pos_qos_rule, temp);
  }

  // len of qos rule ie
  ENCODE_U16(len_qosrulesie, encoded - len_pos_qos_rulesie, temp);

  return encoded;
}

int decode_qos_rules(
    QOSRules* qosrules, uint8_t iei, uint8_t* buffer, uint32_t len) {
  int decoded       = 0;
  int decode_result = 0;
  uint8_t bitstream = 0;
  int i = 0, j = 0;

  if (iei > 0) {
    CHECK_IEI_DECODER(iei, *buffer);
    decoded++;
  }

  DECODE_U16(buffer + decoded, qosrules->lengthofqosrulesie, decoded);

  CHECK_LENGTH_DECODER(len - decoded, qosrules->lengthofqosrulesie);

  QOSRulesIE* qosrulesie = (QOSRulesIE*) calloc(1, sizeof(QOSRulesIE));

  int size            = 0;
  i                   = 0;
  int pre_decoded_pos = decoded;
  while (decoded < qosrules->lengthofqosrulesie) {
    DECODE_U8(buffer + decoded, qosrulesie->qosruleidentifer, decoded);
    DECODE_U16(buffer + decoded, qosrulesie->LengthofQoSrule, decoded);

    DECODE_U8(buffer + decoded, bitstream, decoded);
    qosrulesie->ruleoperationcode     = (bitstream >> 5);
    qosrulesie->dqrbit                = (bitstream >> 4) & 0x01;
    qosrulesie->numberofpacketfilters = bitstream & 0x0f;

    if (qosrulesie->ruleoperationcode ==
        MODIFY_EXISTING_QOS_RULE_AND_DELETE_PACKET_FILTERS) {
      qosrulesie->packetfilterlist.modifyanddelete = (ModifyAndDelete*) calloc(
          qosrulesie->numberofpacketfilters, sizeof(ModifyAndDelete));
      for (j = 0; j < qosrulesie->numberofpacketfilters; j++) {
        DECODE_U8(buffer + decoded, bitstream, decoded);
        qosrulesie->packetfilterlist.modifyanddelete[j].packetfilteridentifier =
            bitstream & 0x0f;
      }
      DECODE_U8(buffer + decoded, bitstream, decoded);  // QoS rule precedence
      qosrulesie->qosruleprecedence = bitstream;
      DECODE_U8(
          buffer + decoded, bitstream, decoded);  // QoS flow identifier (QFI)
      qosrulesie->segregation      = (bitstream >> 6) & 0x01;
      qosrulesie->qosflowidentifer = bitstream & 0x3f;
    } else if (
        (qosrulesie->ruleoperationcode == CREATE_NEW_QOS_RULE) ||
        (qosrulesie->ruleoperationcode ==
         MODIFY_EXISTING_QOS_RULE_AND_ADD_PACKET_FILTERS) ||
        (qosrulesie->ruleoperationcode ==
         MODIFY_EXISTING_QOS_RULE_AND_REPLACE_ALL_PACKET_FILTERS)) {
      qosrulesie->packetfilterlist.create_modifyandadd_modifyandreplace =
          (Create_ModifyAndAdd_ModifyAndReplace*) calloc(
              qosrulesie->numberofpacketfilters,
              sizeof(Create_ModifyAndAdd_ModifyAndReplace));
      for (j = 0; j < qosrulesie->numberofpacketfilters; j++) {
        DECODE_U8(buffer + decoded, bitstream, decoded);
        qosrulesie->packetfilterlist.create_modifyandadd_modifyandreplace[j]
            .packetfilterdirection = (bitstream >> 4) & 0x03;
        qosrulesie->packetfilterlist.create_modifyandadd_modifyandreplace[j]
            .packetfilteridentifier = bitstream & 0x0f;

        uint8_t lenghtofpacketfiltercontents = *(buffer + decoded) - 1;
        decoded++;

        int packetfilterconten_decoded = 0;
        int packetfiltercontent_index  = 0;
        while (packetfilterconten_decoded < lenghtofpacketfiltercontents) {
          DECODE_U8(buffer + decoded, bitstream, decoded);
          qosrulesie->packetfilterlist.create_modifyandadd_modifyandreplace[j]
              .packetfiltercontents[packetfiltercontent_index]
              .component_type = bitstream;
          packetfilterconten_decoded++;
          if (qosrulesie->packetfilterlist
                  .create_modifyandadd_modifyandreplace[j]
                  .packetfiltercontents[packetfiltercontent_index]
                  .component_type != QOS_RULE_MATCHALL_TYPE) {
            int component_len = 0;
            switch (qosrulesie->packetfilterlist
                        .create_modifyandadd_modifyandreplace[j]
                        .packetfiltercontents[packetfiltercontent_index]
                        .component_type) {
              case QOS_RULE_REMOTE_PORT_RANGE_TYPE:
                component_len = 4;
                break;
              case QOS_RULE_SINGLE_REMOTE_PORT_TYPE:
                component_len = 2;
                break;
              case QOS_RULE_IPV4_REMOTE_ADDRESS_TYPE:
                component_len = 8;
                break;
              case QOS_RULE_PROTOCOL_IDENTIFIERORNEXT_HEADER_TYPE:
                component_len = 1;
            }
            if ((decode_result = decode_bstring(
                     &qosrulesie->packetfilterlist
                          .create_modifyandadd_modifyandreplace[j]
                          .packetfiltercontents[packetfiltercontent_index]
                          .component_value,
                     component_len, buffer + decoded, len - decoded)) < 0) {
              return decode_result;
            } else {
              packetfilterconten_decoded += decode_result;
              decoded += decode_result;
            }
          }
          packetfiltercontent_index++;
        }
      }
      DECODE_U8(buffer + decoded, bitstream, decoded);
      qosrulesie->qosruleprecedence = bitstream;
      DECODE_U8(buffer + decoded, bitstream, decoded);
      qosrulesie->segregation      = (bitstream >> 6) & 0x01;
      qosrulesie->qosflowidentifer = bitstream & 0x3f;
    }

    i++;
    size++;
  }

  free(qosrulesie);
  qosrulesie = NULL;

  decoded = pre_decoded_pos;

  qosrules->qosrulesie = (QOSRulesIE*) calloc(size, sizeof(QOSRulesIE));

  i = 0;

  while (decoded < qosrules->lengthofqosrulesie) {
    DECODE_U8(
        buffer + decoded, qosrules->qosrulesie[i].qosruleidentifer, decoded);

    DECODE_U16(
        buffer + decoded, qosrules->qosrulesie[i].LengthofQoSrule, decoded);

    DECODE_U8(buffer + decoded, bitstream, decoded);
    qosrules->qosrulesie[i].ruleoperationcode     = (bitstream >> 5);
    qosrules->qosrulesie[i].dqrbit                = (bitstream >> 4) & 0x01;
    qosrules->qosrulesie[i].numberofpacketfilters = bitstream & 0x0f;

    if (qosrules->qosrulesie[i].ruleoperationcode ==
        MODIFY_EXISTING_QOS_RULE_AND_DELETE_PACKET_FILTERS) {
      qosrules->qosrulesie[i].packetfilterlist.modifyanddelete =
          (ModifyAndDelete*) calloc(
              qosrules->qosrulesie[i].numberofpacketfilters,
              sizeof(ModifyAndDelete));
      for (j = 0; j < qosrules->qosrulesie[i].numberofpacketfilters; j++) {
        DECODE_U8(buffer + decoded, bitstream, decoded);
        qosrules->qosrulesie[i]
            .packetfilterlist.modifyanddelete[j]
            .packetfilteridentifier = bitstream & 0x0f;
      }
      DECODE_U8(buffer + decoded, bitstream, decoded);  // QoS rule precedence
      qosrules->qosrulesie[i].qosruleprecedence = bitstream;
      DECODE_U8(
          buffer + decoded, bitstream, decoded);  // QoS flow identifier (QFI)
      qosrules->qosrulesie[i].segregation      = (bitstream >> 6) & 0x01;
      qosrules->qosrulesie[i].qosflowidentifer = bitstream & 0x3f;
    } else if (
        (qosrules->qosrulesie[i].ruleoperationcode == CREATE_NEW_QOS_RULE) ||
        (qosrules->qosrulesie[i].ruleoperationcode ==
         MODIFY_EXISTING_QOS_RULE_AND_ADD_PACKET_FILTERS) ||
        (qosrules->qosrulesie[i].ruleoperationcode ==
         MODIFY_EXISTING_QOS_RULE_AND_REPLACE_ALL_PACKET_FILTERS)) {
      qosrules->qosrulesie[i]
          .packetfilterlist.create_modifyandadd_modifyandreplace =
          (Create_ModifyAndAdd_ModifyAndReplace*) calloc(
              qosrules->qosrulesie[i].numberofpacketfilters,
              sizeof(Create_ModifyAndAdd_ModifyAndReplace));
      for (j = 0; j < qosrules->qosrulesie[i].numberofpacketfilters; j++) {
        DECODE_U8(buffer + decoded, bitstream, decoded);
        qosrules->qosrulesie[i]
            .packetfilterlist.create_modifyandadd_modifyandreplace[j]
            .packetfilterdirection = (bitstream >> 4) & 0x03;
        qosrules->qosrulesie[i]
            .packetfilterlist.create_modifyandadd_modifyandreplace[j]
            .packetfilteridentifier = bitstream & 0x0f;

        uint8_t lenghtofpacketfiltercontents = *(buffer + decoded) - 1;
        decoded++;

        int packetfilterconten_decoded = 0;
        int packetfiltercontent_index  = 0;
        while (packetfilterconten_decoded < lenghtofpacketfiltercontents) {
          DECODE_U8(buffer + decoded, bitstream, decoded);
          qosrulesie->packetfilterlist.create_modifyandadd_modifyandreplace[j]
              .packetfiltercontents[packetfiltercontent_index]
              .component_type = bitstream;
          packetfilterconten_decoded++;
          if (qosrulesie->packetfilterlist
                  .create_modifyandadd_modifyandreplace[j]
                  .packetfiltercontents[packetfiltercontent_index]
                  .component_type != QOS_RULE_MATCHALL_TYPE) {
            int component_len = 0;
            switch (qosrulesie->packetfilterlist
                        .create_modifyandadd_modifyandreplace[j]
                        .packetfiltercontents[packetfiltercontent_index]
                        .component_type) {
              case QOS_RULE_REMOTE_PORT_RANGE_TYPE:
                component_len = 4;
                break;
              case QOS_RULE_SINGLE_REMOTE_PORT_TYPE:
                component_len = 2;
                break;
              case QOS_RULE_IPV4_REMOTE_ADDRESS_TYPE:
                component_len = 8;
                break;
              case QOS_RULE_PROTOCOL_IDENTIFIERORNEXT_HEADER_TYPE:
                component_len = 1;
            }
            if ((decode_result = decode_bstring(
                     &qosrulesie->packetfilterlist
                          .create_modifyandadd_modifyandreplace[j]
                          .packetfiltercontents[packetfiltercontent_index]
                          .component_value,
                     component_len, buffer + decoded, len - decoded)) < 0) {
              return decode_result;
            } else {
              packetfilterconten_decoded += decode_result;
              decoded += decode_result;
            }
          }
          packetfiltercontent_index++;
        }
      }
      DECODE_U8(buffer + decoded, bitstream, decoded);
      qosrules->qosrulesie[i].qosruleprecedence = bitstream;
      DECODE_U8(buffer + decoded, bitstream, decoded);
      qosrules->qosrulesie[i].segregation      = (bitstream >> 6) & 0x01;
      qosrules->qosrulesie[i].qosflowidentifer = bitstream & 0x3f;
    }

    i++;
  }

  return decoded;
}

void free_decode_qos_rules(QOSRules* qosrules) {
  int i;

  for (i = 0; i < qosrules->lengthofqosrulesie; i++) {
    if (qosrules->qosrulesie[i].ruleoperationcode ==
        MODIFY_EXISTING_QOS_RULE_AND_DELETE_PACKET_FILTERS) {
      free(qosrules->qosrulesie[i].packetfilterlist.modifyanddelete);
      qosrules->qosrulesie[i].packetfilterlist.modifyanddelete = NULL;
    } else if (
        (qosrules->qosrulesie[i].ruleoperationcode == CREATE_NEW_QOS_RULE) ||
        (qosrules->qosrulesie[i].ruleoperationcode ==
         MODIFY_EXISTING_QOS_RULE_AND_ADD_PACKET_FILTERS) ||
        (qosrules->qosrulesie[i].ruleoperationcode ==
         MODIFY_EXISTING_QOS_RULE_AND_REPLACE_ALL_PACKET_FILTERS)) {
      free(qosrules->qosrulesie[i]
               .packetfilterlist.create_modifyandadd_modifyandreplace);
      qosrules->qosrulesie[i]
          .packetfilterlist.create_modifyandadd_modifyandreplace = NULL;
    }
  }
  free(qosrules->qosrulesie);
  qosrules->qosrulesie = NULL;
}
