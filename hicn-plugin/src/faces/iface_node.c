/*
 * Copyright (c) 2021-2022 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "face.h"
#include "inlines.h"
#include "../strategy_dpo_manager.h"
#include "../hicn.h"
#include "../infra.h"
#include "../cache_policies/cs_lru.h"
#include "../parser.h"

#include <hicn/error.h>
#include <hicn/util/ip_address.h>

/**
 * @File
 *
 * Definition of the nodes for ip incomplete faces.
 */

vlib_node_registration_t hicn4_iface_input_node;
vlib_node_registration_t hicn4_iface_output_node;
vlib_node_registration_t hicn6_iface_input_node;
vlib_node_registration_t hicn6_iface_output_node;

u32 data_fwd_iface_ip4_vlib_edge;
u32 data_fwd_iface_ip6_vlib_edge;

static char *hicn4_iface_input_error_strings[] = {
#define _(sym, string) string,
  foreach_hicnfwd_error
#undef _
};

static char *hicn6_iface_input_error_strings[] = {
#define _(sym, string) string,
  foreach_hicnfwd_error
#undef _
};

/* Trace context struct */
typedef struct
{
  u32 next_index;
  u32 sw_if_index;
  u8 pkt_type;
  hicn_error_t error;
  u8 packet_data[60];
} hicn4_iface_input_trace_t;

typedef enum
{
  HICN4_IFACE_INPUT_NEXT_INTEREST,
  HICN4_IFACE_INPUT_NEXT_INTEREST_MANIFEST,
  HICN4_IFACE_INPUT_NEXT_MAPME,
  HICN4_IFACE_INPUT_NEXT_ERROR_DROP,
  HICN4_IFACE_INPUT_N_NEXT,
} hicn4_iface_input_next_t;

/* Trace context struct */
typedef struct
{
  u32 next_index;
  u32 sw_if_index;
  u8 pkt_type;
  hicn_error_t error;
  u8 packet_data[60];
} hicn6_iface_input_trace_t;

typedef enum
{
  HICN6_IFACE_INPUT_NEXT_INTEREST,
  HICN6_IFACE_INPUT_NEXT_INTEREST_MANIFEST,
  HICN6_IFACE_INPUT_NEXT_MAPME,
  HICN6_IFACE_INPUT_NEXT_ERROR_DROP,
  HICN6_IFACE_INPUT_N_NEXT,
} hicn6_iface_input_next_t;

#define NEXT_MAPME_IP4 HICN4_IFACE_INPUT_NEXT_MAPME
#define NEXT_MAPME_IP6 HICN6_IFACE_INPUT_NEXT_MAPME

#define NEXT_INTEREST_IP4 HICN4_IFACE_INPUT_NEXT_INTEREST
#define NEXT_INTEREST_IP6 HICN6_IFACE_INPUT_NEXT_INTEREST

#define DPO_ADD_LOCK_FACE_IP4 hicn_face_ip4_add_and_lock
#define DPO_ADD_LOCK_FACE_IP6 hicn_face_ip6_add_and_lock

#define IP_HEADER_4 ip4_header_t
#define IP_HEADER_6 ip6_header_t

#define TRACE_INPUT_PKT_IP4 hicn4_iface_input_trace_t
#define TRACE_INPUT_PKT_IP6 hicn6_iface_input_trace_t

// NODE OUTPUT

static char *hicn4_iface_output_error_strings[] = {
#define _(sym, string) string,
  foreach_hicnfwd_error
#undef _
};

static char *hicn6_iface_output_error_strings[] = {
#define _(sym, string) string,
  foreach_hicnfwd_error
#undef _
};

/* Trace context struct */
typedef struct
{
  u32 next_index;
  u32 sw_if_index;
  u8 pkt_type;
  u8 packet_data[60];
} hicn4_iface_output_trace_t;

typedef enum
{
  HICN4_IFACE_OUTPUT_NEXT_DROP,
  HICN4_IFACE_OUTPUT_NEXT_LOOKUP,
  HICN4_IFACE_OUTPUT_NEXT_UDP4_ENCAP,
  HICN4_IFACE_OUTPUT_NEXT_UDP6_ENCAP,
  HICN4_IFACE_OUTPUT_NEXT_PG,
  HICN4_IFACE_OUTPUT_N_NEXT,
} hicn4_iface_output_next_t;

/* Trace context struct */
typedef struct
{
  u32 next_index;
  u32 sw_if_index;
  u8 pkt_type;
  u8 packet_data[60];
} hicn6_iface_output_trace_t;

typedef enum
{
  HICN6_IFACE_OUTPUT_NEXT_DROP,
  HICN6_IFACE_OUTPUT_NEXT_LOOKUP,
  HICN6_IFACE_OUTPUT_NEXT_UDP4_ENCAP,
  HICN6_IFACE_OUTPUT_NEXT_UDP6_ENCAP,
  HICN6_IFACE_OUTPUT_NEXT_PG,
  HICN6_IFACE_OUTPUT_N_NEXT,
} hicn6_iface_output_next_t;

//#define ERROR_OUTPUT_IP4 HICN4_IFACE_OUTPUT_NEXT_ERROR_DROP
//#define ERROR_OUTPUT_IP6 HICN6_IFACE_OUTPUT_NEXT_ERROR_DROP

#define NEXT_DATA_LOOKUP_IP4 HICN4_IFACE_OUTPUT_NEXT_LOOKUP
#define NEXT_DATA_LOOKUP_IP6 HICN6_IFACE_OUTPUT_NEXT_LOOKUP

#define NEXT_UDP_ENCAP_IP4 HICN4_IFACE_OUTPUT_NEXT_UDP4_ENCAP
#define NEXT_UDP_ENCAP_IP6 HICN6_IFACE_OUTPUT_NEXT_UDP6_ENCAP

#define NEXT_PG4 HICN4_IFACE_OUTPUT_NEXT_PG
#define NEXT_PG6 HICN6_IFACE_OUTPUT_NEXT_PG

#define HICN_REWRITE_DATA_IP4 hicn_rewrite_iface_data4
#define HICN_REWRITE_DATA_IP6 hicn_rewrite_iface_data6

#define TRACE_OUTPUT_PKT_IP4 hicn4_iface_output_trace_t
#define TRACE_OUTPUT_PKT_IP6 hicn6_iface_output_trace_t

// NODES IMPLEMENTATIONS

#define iface_input_x1(ipv)                                                   \
  do                                                                          \
    {                                                                         \
      vlib_buffer_t *b0;                                                      \
      u32 bi0, next0, next_iface0, sw_if0 = ~0;                               \
      IP_HEADER_##ipv *ip_hdr = NULL;                                         \
      hicn_buffer_t *hicnb0;                                                  \
      int ret0 = HICN_ERROR_NONE;                                             \
      u8 is_icmp0, is_manifest0;                                              \
      /* Prefetch for next iteration. */                                      \
      if (n_left_from > 1)                                                    \
	{                                                                     \
	  vlib_buffer_t *b1;                                                  \
	  b1 = vlib_get_buffer (vm, from[1]);                                 \
	  CLIB_PREFETCH (b1, 2 * CLIB_CACHE_LINE_BYTES, STORE);               \
	  CLIB_PREFETCH (b1->data, CLIB_CACHE_LINE_BYTES, LOAD);              \
	}                                                                     \
      /* Dequeue a packet buffer */                                           \
      bi0 = from[0];                                                          \
      from += 1;                                                              \
      n_left_from -= 1;                                                       \
      to_next[0] = bi0;                                                       \
      to_next += 1;                                                           \
      n_left_to_next -= 1;                                                    \
                                                                              \
      b0 = vlib_get_buffer (vm, bi0);                                         \
      hicnb0 = hicn_get_buffer (b0);                                          \
      ip_hdr = (IP_HEADER_##ipv *) vlib_buffer_get_current (b0);              \
                                                                              \
      /* Parse packet and cache useful info in opaque2 */                     \
      ret0 =                                                                  \
	hicn_interest_parse_pkt (b0, vlib_buffer_length_in_chain (vm, b0));   \
      is_icmp0 = (ret0 == HICN_ERROR_PARSER_MAPME_PACKET);                    \
      is_manifest0 = hicnb0->payload_type == HPT_MANIFEST;                    \
      ret0 = (ret0 == HICN_ERROR_NONE) ||                                     \
	     (ret0 == HICN_ERROR_PARSER_MAPME_PACKET);                        \
      if (PREDICT_FALSE (!ret0))                                              \
	{                                                                     \
	  next0 = HICN##ipv##_IFACE_INPUT_NEXT_ERROR_DROP;                    \
	}                                                                     \
      else                                                                    \
	{                                                                     \
	  next0 = is_icmp0 * NEXT_MAPME_IP##ipv +                             \
		  (1 - is_icmp0) * (NEXT_INTEREST_IP##ipv + is_manifest0);    \
                                                                              \
	  next_iface0 = NEXT_DATA_LOOKUP_IP##ipv;                             \
	  sw_if0 = vnet_buffer (b0)->sw_if_index[VLIB_RX];                    \
                                                                              \
	  if (hicnb0->flags & HICN_BUFFER_FLAGS_FROM_UDP4_TUNNEL &&           \
	      vnet_buffer (b0)->ip.adj_index[VLIB_RX] != ADJ_INDEX_INVALID)   \
	    {                                                                 \
	      next_iface0 = NEXT_UDP_ENCAP_IP4;                               \
	      sw_if0 = ~0;                                                    \
	    }                                                                 \
	  else if (hicnb0->flags & HICN_BUFFER_FLAGS_FROM_UDP6_TUNNEL &&      \
		   vnet_buffer (b0)->ip.adj_index[VLIB_RX] !=                 \
		     ADJ_INDEX_INVALID)                                       \
	    {                                                                 \
	      next_iface0 = NEXT_UDP_ENCAP_IP6;                               \
	      sw_if0 = ~0;                                                    \
	    }                                                                 \
	  else if (hicnb0->flags & HICN_BUFFER_FLAGS_FROM_PG)                 \
	    {                                                                 \
	      next_iface0 = NEXT_PG##ipv;                                     \
	    }                                                                 \
                                                                              \
	  DPO_ADD_LOCK_FACE_IP##ipv (                                         \
	    &(hicnb0->face_id), &hicnb0->flags, &(ip_hdr->src_address),       \
	    sw_if0, vnet_buffer (b0)->ip.adj_index[VLIB_RX], next_iface0);    \
	}                                                                     \
                                                                              \
      if (PREDICT_FALSE ((node->flags & VLIB_NODE_FLAG_TRACE) &&              \
			 (b0->flags & VLIB_BUFFER_IS_TRACED)))                \
	{                                                                     \
	  TRACE_INPUT_PKT_IP##ipv *t =                                        \
	    vlib_add_trace (vm, node, b0, sizeof (*t));                       \
	  t->pkt_type = HICN_PACKET_TYPE_INTEREST;                            \
	  t->sw_if_index = sw_if0;                                            \
	  t->next_index = next0;                                              \
	  t->error = ret0;                                                    \
	  clib_memcpy_fast (t->packet_data, vlib_buffer_get_current (b0),     \
			    sizeof (t->packet_data));                         \
	}                                                                     \
                                                                              \
      vlib_increment_combined_counter (                                       \
	&counters[hicnb0->face_id * HICN_N_COUNTER], thread_index,            \
	HICN_FACE_COUNTERS_INTEREST_RX, 1,                                    \
	vlib_buffer_length_in_chain (vm, b0));                                \
                                                                              \
      /* Verify speculative enqueue, maybe switch current next frame */       \
      vlib_validate_buffer_enqueue_x1 (vm, node, next_index, to_next,         \
				       n_left_to_next, bi0, next0);           \
    }                                                                         \
  while (0)

#define iface_input_x2(ipv)                                                   \
  do                                                                          \
    {                                                                         \
      vlib_buffer_t *b0, *b1;                                                 \
      u32 bi0, bi1, next0, next1;                                             \
      u32 next_iface0, next_iface1, sw_if0 = ~0, sw_if1 = ~0;                 \
      u8 is_icmp0, is_icmp1, is_manifest0, is_manifest1;                      \
      IP_HEADER_##ipv *ip_hdr0 = NULL;                                        \
      IP_HEADER_##ipv *ip_hdr1 = NULL;                                        \
      int ret0 = HICN_ERROR_NONE, ret1 = HICN_ERROR_NONE;                     \
      hicn_buffer_t *hicnb0, *hicnb1;                                         \
                                                                              \
      /* Prefetch for next iteration. */                                      \
      vlib_buffer_t *b2, *b3;                                                 \
      b2 = vlib_get_buffer (vm, from[2]);                                     \
      b3 = vlib_get_buffer (vm, from[3]);                                     \
      CLIB_PREFETCH (b2, 2 * CLIB_CACHE_LINE_BYTES, STORE);                   \
      CLIB_PREFETCH (b3, 2 * CLIB_CACHE_LINE_BYTES, STORE);                   \
      CLIB_PREFETCH (b2->data, CLIB_CACHE_LINE_BYTES, LOAD);                  \
      CLIB_PREFETCH (b3->data, CLIB_CACHE_LINE_BYTES, LOAD);                  \
                                                                              \
      /* Dequeue a packet buffer */                                           \
      bi0 = from[0];                                                          \
      bi1 = from[1];                                                          \
      from += 2;                                                              \
      n_left_from -= 2;                                                       \
      to_next[0] = bi0;                                                       \
      to_next[1] = bi1;                                                       \
      to_next += 2;                                                           \
      n_left_to_next -= 2;                                                    \
                                                                              \
      b0 = vlib_get_buffer (vm, bi0);                                         \
      b1 = vlib_get_buffer (vm, bi1);                                         \
      hicnb0 = hicn_get_buffer (b0);                                          \
      hicnb1 = hicn_get_buffer (b1);                                          \
      ip_hdr0 = (IP_HEADER_##ipv *) vlib_buffer_get_current (b0);             \
      ip_hdr1 = (IP_HEADER_##ipv *) vlib_buffer_get_current (b1);             \
                                                                              \
      stats.pkts_interest_count += 2;                                         \
                                                                              \
      /* Parse packet and cache useful info in opaque2 */                     \
      ret0 =                                                                  \
	hicn_interest_parse_pkt (b0, vlib_buffer_length_in_chain (vm, b0));   \
      ret1 =                                                                  \
	hicn_interest_parse_pkt (b1, vlib_buffer_length_in_chain (vm, b1));   \
      is_icmp0 = ret0 == HICN_ERROR_PARSER_MAPME_PACKET;                      \
      is_icmp1 = ret1 == HICN_ERROR_PARSER_MAPME_PACKET;                      \
      is_manifest0 = hicnb0->payload_type == HPT_MANIFEST;                    \
      is_manifest1 = hicnb1->payload_type == HPT_MANIFEST;                    \
      ret0 = (ret0 == HICN_ERROR_NONE) ||                                     \
	     (ret0 == HICN_ERROR_PARSER_MAPME_PACKET);                        \
      ret1 = (ret1 == HICN_ERROR_NONE) ||                                     \
	     (ret1 == HICN_ERROR_PARSER_MAPME_PACKET);                        \
                                                                              \
      if (PREDICT_TRUE (ret0 && ret1))                                        \
	{                                                                     \
	  next0 = is_icmp0 * NEXT_MAPME_IP##ipv +                             \
		  (1 - is_icmp0) * (NEXT_INTEREST_IP##ipv + is_manifest0);    \
                                                                              \
	  next1 = is_icmp1 * NEXT_MAPME_IP##ipv +                             \
		  (1 - is_icmp1) * (NEXT_INTEREST_IP##ipv + is_manifest1);    \
                                                                              \
	  next_iface0 = NEXT_DATA_LOOKUP_IP##ipv;                             \
	  sw_if0 = vnet_buffer (b0)->sw_if_index[VLIB_RX];                    \
                                                                              \
	  if (hicnb0->flags & HICN_BUFFER_FLAGS_FROM_UDP4_TUNNEL &&           \
	      vnet_buffer (b0)->ip.adj_index[VLIB_RX] != ADJ_INDEX_INVALID)   \
	    {                                                                 \
	      next_iface0 = NEXT_UDP_ENCAP_IP4;                               \
	      sw_if0 = ~0;                                                    \
	    }                                                                 \
	  else if (hicnb0->flags & HICN_BUFFER_FLAGS_FROM_UDP6_TUNNEL &&      \
		   vnet_buffer (b0)->ip.adj_index[VLIB_RX] !=                 \
		     ADJ_INDEX_INVALID)                                       \
	    {                                                                 \
	      next_iface0 = NEXT_UDP_ENCAP_IP6;                               \
	      sw_if0 = ~0;                                                    \
	    }                                                                 \
	  else if (hicnb0->flags & HICN_BUFFER_FLAGS_FROM_PG)                 \
	    {                                                                 \
	      next_iface0 = NEXT_PG##ipv;                                     \
	    }                                                                 \
                                                                              \
	  next_iface1 = NEXT_DATA_LOOKUP_IP##ipv;                             \
	  sw_if1 = vnet_buffer (b1)->sw_if_index[VLIB_RX];                    \
                                                                              \
	  if (hicnb1->flags & HICN_BUFFER_FLAGS_FROM_UDP4_TUNNEL &&           \
	      vnet_buffer (b1)->ip.adj_index[VLIB_RX] != ADJ_INDEX_INVALID)   \
	    {                                                                 \
	      next_iface1 = NEXT_UDP_ENCAP_IP4;                               \
	      sw_if1 = ~0;                                                    \
	    }                                                                 \
	  else if (hicnb1->flags & HICN_BUFFER_FLAGS_FROM_UDP6_TUNNEL &&      \
		   vnet_buffer (b1)->ip.adj_index[VLIB_RX] !=                 \
		     ADJ_INDEX_INVALID)                                       \
	    {                                                                 \
	      next_iface1 = NEXT_UDP_ENCAP_IP6;                               \
	      sw_if1 = ~0;                                                    \
	    }                                                                 \
	  else if (hicnb0->flags & HICN_BUFFER_FLAGS_FROM_PG)                 \
	    {                                                                 \
	      next_iface1 = NEXT_PG##ipv;                                     \
	    }                                                                 \
                                                                              \
	  DPO_ADD_LOCK_FACE_IP##ipv (                                         \
	    &(hicnb0->face_id), &hicnb0->flags, &(ip_hdr0->src_address),      \
	    sw_if0, vnet_buffer (b0)->ip.adj_index[VLIB_RX], next_iface0);    \
                                                                              \
	  DPO_ADD_LOCK_FACE_IP##ipv (                                         \
	    &(hicnb1->face_id), &hicnb1->flags, &(ip_hdr1->src_address),      \
	    sw_if1, vnet_buffer (b1)->ip.adj_index[VLIB_RX], next_iface1);    \
	}                                                                     \
      else if (ret0 && !ret1)                                                 \
	{                                                                     \
	  next1 = HICN##ipv##_IFACE_INPUT_NEXT_ERROR_DROP;                    \
	  next0 = is_icmp0 * NEXT_MAPME_IP##ipv +                             \
		  (1 - is_icmp0) * NEXT_INTEREST_IP##ipv;                     \
	  next_iface0 = NEXT_DATA_LOOKUP_IP##ipv;                             \
	  sw_if0 = vnet_buffer (b0)->sw_if_index[VLIB_RX];                    \
                                                                              \
	  if (hicnb0->flags & HICN_BUFFER_FLAGS_FROM_UDP4_TUNNEL &&           \
	      vnet_buffer (b0)->ip.adj_index[VLIB_RX] != ADJ_INDEX_INVALID)   \
	    {                                                                 \
	      next_iface0 = NEXT_UDP_ENCAP_IP4;                               \
	      sw_if0 = ~0;                                                    \
	    }                                                                 \
	  else if (hicnb0->flags & HICN_BUFFER_FLAGS_FROM_UDP6_TUNNEL &&      \
		   vnet_buffer (b0)->ip.adj_index[VLIB_RX] !=                 \
		     ADJ_INDEX_INVALID)                                       \
	    {                                                                 \
	      next_iface0 = NEXT_UDP_ENCAP_IP6;                               \
	      sw_if0 = ~0;                                                    \
	    }                                                                 \
	  else if (hicnb0->flags & HICN_BUFFER_FLAGS_FROM_PG)                 \
	    {                                                                 \
	      next_iface0 = NEXT_PG##ipv;                                     \
	    }                                                                 \
                                                                              \
	  DPO_ADD_LOCK_FACE_IP##ipv (                                         \
	    &(hicnb0->face_id), &hicnb0->flags, &(ip_hdr0->src_address),      \
	    sw_if0, vnet_buffer (b0)->ip.adj_index[VLIB_RX], next_iface0);    \
	}                                                                     \
      else if (!ret0 && ret1)                                                 \
	{                                                                     \
	  next0 = HICN##ipv##_IFACE_INPUT_NEXT_ERROR_DROP;                    \
	  next_iface1 = NEXT_DATA_LOOKUP_IP##ipv;                             \
	  sw_if1 = vnet_buffer (b1)->sw_if_index[VLIB_RX];                    \
	  next1 = is_icmp1 * NEXT_MAPME_IP##ipv +                             \
		  (1 - is_icmp1) * NEXT_INTEREST_IP##ipv;                     \
                                                                              \
	  if (hicnb1->flags & HICN_BUFFER_FLAGS_FROM_UDP4_TUNNEL &&           \
	      vnet_buffer (b1)->ip.adj_index[VLIB_RX] != ADJ_INDEX_INVALID)   \
	    {                                                                 \
	      next_iface1 = NEXT_UDP_ENCAP_IP4;                               \
	      sw_if1 = ~0;                                                    \
	    }                                                                 \
	  else if (hicnb1->flags & HICN_BUFFER_FLAGS_FROM_UDP6_TUNNEL &&      \
		   vnet_buffer (b1)->ip.adj_index[VLIB_RX] !=                 \
		     ADJ_INDEX_INVALID)                                       \
	    {                                                                 \
	      next_iface1 = NEXT_UDP_ENCAP_IP6;                               \
	      sw_if1 = ~0;                                                    \
	    }                                                                 \
	  else if (hicnb0->flags & HICN_BUFFER_FLAGS_FROM_PG)                 \
	    {                                                                 \
	      next_iface1 = NEXT_PG##ipv;                                     \
	    }                                                                 \
                                                                              \
	  DPO_ADD_LOCK_FACE_IP##ipv (                                         \
	    &(hicnb1->face_id), &hicnb1->flags, &(ip_hdr1->src_address),      \
	    sw_if1, vnet_buffer (b1)->ip.adj_index[VLIB_RX], next_iface1);    \
	}                                                                     \
      else                                                                    \
	{                                                                     \
	  next0 = HICN##ipv##_IFACE_INPUT_NEXT_ERROR_DROP;                    \
	  next1 = HICN##ipv##_IFACE_INPUT_NEXT_ERROR_DROP;                    \
	}                                                                     \
                                                                              \
      if (PREDICT_FALSE ((node->flags & VLIB_NODE_FLAG_TRACE) &&              \
			 (b0->flags & VLIB_BUFFER_IS_TRACED)))                \
	{                                                                     \
	  TRACE_INPUT_PKT_IP##ipv *t =                                        \
	    vlib_add_trace (vm, node, b0, sizeof (*t));                       \
	  t->pkt_type = HICN_PACKET_TYPE_INTEREST;                            \
	  t->sw_if_index = sw_if0;                                            \
	  t->next_index = next0;                                              \
	  t->error = ret0;                                                    \
	  clib_memcpy_fast (t->packet_data, vlib_buffer_get_current (b0),     \
			    sizeof (t->packet_data));                         \
	}                                                                     \
                                                                              \
      if (PREDICT_FALSE ((node->flags & VLIB_NODE_FLAG_TRACE) &&              \
			 (b1->flags & VLIB_BUFFER_IS_TRACED)))                \
	{                                                                     \
	  TRACE_INPUT_PKT_IP##ipv *t =                                        \
	    vlib_add_trace (vm, node, b1, sizeof (*t));                       \
	  t->pkt_type = HICN_PACKET_TYPE_INTEREST;                            \
	  t->sw_if_index = sw_if1;                                            \
	  t->next_index = next1;                                              \
	  t->error = ret1;                                                    \
	  clib_memcpy_fast (t->packet_data, vlib_buffer_get_current (b1),     \
			    sizeof (t->packet_data));                         \
	}                                                                     \
                                                                              \
      vlib_increment_combined_counter (                                       \
	&counters[hicnb0->face_id * HICN_N_COUNTER], thread_index,            \
	HICN_FACE_COUNTERS_INTEREST_RX, 1,                                    \
	vlib_buffer_length_in_chain (vm, b0));                                \
                                                                              \
      vlib_increment_combined_counter (                                       \
	&counters[hicnb1->face_id * HICN_N_COUNTER], thread_index,            \
	HICN_FACE_COUNTERS_INTEREST_RX, 1,                                    \
	vlib_buffer_length_in_chain (vm, b1));                                \
                                                                              \
      /* Verify speculative enqueue, maybe switch current next frame */       \
      vlib_validate_buffer_enqueue_x2 (vm, node, next_index, to_next,         \
				       n_left_to_next, bi0, bi1, next0,       \
				       next1);                                \
    }                                                                         \
  while (0)

static uword
hicn4_iface_input_node_fn (vlib_main_t *vm, vlib_node_runtime_t *node,
			   vlib_frame_t *frame)
{
  u32 n_left_from, *from, *to_next, next_index;

  from = vlib_frame_vector_args (frame);
  n_left_from = frame->n_vectors;
  next_index = node->cached_next_index;
  vl_api_hicn_api_node_stats_get_reply_t stats = { 0 };
  u32 thread_index = vm->thread_index;

  while (n_left_from > 0)
    {
      u32 n_left_to_next;
      vlib_get_next_frame (vm, node, next_index, to_next, n_left_to_next);

      /* Dual loop, X2 */
      while (n_left_from >= 4 && n_left_to_next >= 2)
	{
	  iface_input_x2 (4);
	}

      /* Dual loop, X1 */
      while (n_left_from > 0 && n_left_to_next > 0)
	{
	  iface_input_x1 (4);
	}
      vlib_put_next_frame (vm, node, next_index, n_left_to_next);
    }

  vlib_node_increment_counter (vm, node->node_index, HICNFWD_ERROR_INTERESTS,
			       stats.pkts_interest_count);

  return (frame->n_vectors);
}

/* packet trace format function */
static u8 *
hicn4_iface_input_format_trace (u8 *s, va_list *args)
{
  CLIB_UNUSED (vlib_main_t * vm) = va_arg (*args, vlib_main_t *);
  CLIB_UNUSED (vlib_node_t * node) = va_arg (*args, vlib_node_t *);
  hicn4_iface_input_trace_t *t = va_arg (*args, hicn4_iface_input_trace_t *);

  s = format (s, "IFACE_IP4_INPUT: pkt: %d, sw_if_index %d, next index %d",
	      (int) t->pkt_type, t->sw_if_index, t->next_index);

  if (t->error)
    {
      s = format (s, " drop reason: %s", get_error_string (t->error));
    }

  s = format (s, "\n%U", format_ip4_header, t->packet_data,
	      sizeof (t->packet_data));

  return (s);
}

/*
 * Node registration for the interest forwarder node
 */
VLIB_REGISTER_NODE (hicn4_iface_input_node) =
{
  .function = hicn4_iface_input_node_fn,
  .name = "hicn4-iface-input",
  .vector_size =  sizeof (u32),
  .format_trace = hicn4_iface_input_format_trace,
  .type = VLIB_NODE_TYPE_INTERNAL,
  .n_errors = ARRAY_LEN (hicn4_iface_input_error_strings),
  .error_strings = hicn4_iface_input_error_strings,
  .n_next_nodes = HICN4_IFACE_INPUT_N_NEXT,
  /* edit / add dispositions*/
  .next_nodes =
  {
    [HICN4_IFACE_INPUT_NEXT_INTEREST] = "hicn-interest-pcslookup",
    [HICN4_IFACE_INPUT_NEXT_INTEREST_MANIFEST] = "hicn-interest-manifest-pcslookup",
    [HICN4_IFACE_INPUT_NEXT_MAPME] = "hicn-mapme-ctrl",
    [HICN4_IFACE_INPUT_NEXT_ERROR_DROP] = "error-drop",
  },
};

static uword
hicn6_iface_input_node_fn (vlib_main_t *vm, vlib_node_runtime_t *node,
			   vlib_frame_t *frame)
{
  u32 n_left_from, *from, *to_next, next_index;

  from = vlib_frame_vector_args (frame);
  n_left_from = frame->n_vectors;
  next_index = node->cached_next_index;
  vl_api_hicn_api_node_stats_get_reply_t stats = { 0 };
  u32 thread_index = vm->thread_index;

  while (n_left_from > 0)
    {
      u32 n_left_to_next;
      vlib_get_next_frame (vm, node, next_index, to_next, n_left_to_next);

      /* Dual loop, X2 */
      while (n_left_from >= 4 && n_left_to_next >= 2)
	{
	  iface_input_x2 (6);
	}

      /* Dual loop, X1 */
      while (n_left_from > 0 && n_left_to_next > 0)
	{
	  iface_input_x1 (6);
	}

      vlib_put_next_frame (vm, node, next_index, n_left_to_next);
    }

  vlib_node_increment_counter (vm, node->node_index, HICNFWD_ERROR_INTERESTS,
			       stats.pkts_interest_count);

  return (frame->n_vectors);
}

/* packet trace format function */
static u8 *
hicn6_iface_input_format_trace (u8 *s, va_list *args)
{
  CLIB_UNUSED (vlib_main_t * vm) = va_arg (*args, vlib_main_t *);
  CLIB_UNUSED (vlib_node_t * node) = va_arg (*args, vlib_node_t *);
  hicn6_iface_input_trace_t *t = va_arg (*args, hicn6_iface_input_trace_t *);

  s = format (s, "IFACE_IP6_INPUT: pkt: %d, sw_if_index %d, next index %d\n%U",
	      (int) t->pkt_type, t->sw_if_index, t->next_index,
	      format_ip6_header, t->packet_data, sizeof (t->packet_data));
  return (s);
}

/*
 * Node registration for the interest forwarder node
 */
VLIB_REGISTER_NODE (hicn6_iface_input_node) =
{
  .function = hicn6_iface_input_node_fn,
  .name = "hicn6-iface-input",
  .vector_size =  sizeof (u32),
  .format_trace = hicn6_iface_input_format_trace,
  .type = VLIB_NODE_TYPE_INTERNAL,
  .n_errors = ARRAY_LEN (hicn6_iface_input_error_strings),
  .error_strings = hicn6_iface_input_error_strings,
  .n_next_nodes = HICN6_IFACE_INPUT_N_NEXT,
  /* edit / add dispositions*/
  .next_nodes =
  {
    [HICN6_IFACE_INPUT_NEXT_INTEREST] = "hicn-interest-pcslookup",
    [HICN6_IFACE_INPUT_NEXT_INTEREST_MANIFEST] = "hicn-interest-manifest-pcslookup",
    [HICN6_IFACE_INPUT_NEXT_MAPME] = "hicn-mapme-ctrl",
    [HICN6_IFACE_INPUT_NEXT_ERROR_DROP] = "error-drop",
  },
};

/**** IFACE OUTPUT *****/

static inline void
hicn_rewrite_iface_data4 (vlib_main_t *vm, vlib_buffer_t *b0,
			  const hicn_face_t *iface, u32 *next)
{
  ip4_header_t *ip0;
  int ret = HICN_ERROR_NONE;

  /* Get the pointer to the old ip and tcp header */
  ip0 = vlib_buffer_get_current (b0);

  /* Set up the ip6 header */
  /* IP4 lenght contains the size of the ip4 header too */
  u16 sval = (vlib_buffer_length_in_chain (vm, b0));
  ip0->length = clib_host_to_net_u16 (sval);
  ip0->ttl = 254; // FIXME TTL

  vnet_buffer (b0)->ip.adj_index[VLIB_TX] = iface->dpo.dpoi_index;
  *next = iface->dpo.dpoi_next_node;

  hicn_packet_buffer_t *pkbuf = &hicn_get_buffer (b0)->pkbuf;

  hicn_ip_address_t temp_addr;
  ip46_address_reset (&(temp_addr.as_ip46));

  hicn_ip_address_t *iface_nat_addr = (hicn_ip_address_t *) &(iface->nat_addr);

  u8 flags = hicn_get_buffer (b0)->flags;
  u8 reset_pl = flags & HICN_BUFFER_FLAGS_FROM_CS;

  ret = hicn_data_rewrite (pkbuf, iface_nat_addr, &(temp_addr), iface->pl_id,
			   reset_pl);

  if (ret == HICN_LIB_ERROR_REWRITE_CKSUM_REQUIRED)
    {
      ensure_offload_flags (b0, 1 /* is_v4 */);
    }
}

static inline void
hicn_rewrite_iface_data6 (vlib_main_t *vm, vlib_buffer_t *b0,
			  const hicn_face_t *iface, u32 *next)
{
  ip6_header_t *ip0;
  int ret = HICN_ERROR_NONE;

  /* Get the pointer to the old ip and tcp header */
  /* Copy the previous ip and tcp header to the new portion of memory */
  ip0 = vlib_buffer_get_current (b0);

  /* Set up the ip6 header */
  /* IP6 lenght does not include the size of the ip6 header */
  u16 sval = (vlib_buffer_length_in_chain (vm, b0) - (sizeof (ip6_header_t)));
  ip0->payload_length = clib_host_to_net_u16 (sval);
  ip0->hop_limit = HICN_IP6_HOP_LIMIT;

  vnet_buffer (b0)->ip.adj_index[VLIB_TX] = iface->dpo.dpoi_index;
  *next = iface->dpo.dpoi_next_node;

  hicn_packet_buffer_t *pkbuf = &hicn_get_buffer (b0)->pkbuf;

  hicn_ip_address_t temp_addr;
  ip46_address_reset (&(temp_addr.as_ip46));

  hicn_ip_address_t *iface_nat_addr = (hicn_ip_address_t *) &(iface->nat_addr);
  u8 flags = hicn_get_buffer (b0)->flags;
  u8 reset_pl = flags & HICN_BUFFER_FLAGS_FROM_CS;

  ret = hicn_data_rewrite (pkbuf, iface_nat_addr, &(temp_addr), iface->pl_id,
			   reset_pl);

  if (ret == HICN_LIB_ERROR_REWRITE_CKSUM_REQUIRED)
    {
      ensure_offload_flags (b0, 0 /* is_v4 */);
    }
}

#define iface_output_x1(ipv)                                                  \
  do                                                                          \
    {                                                                         \
      vlib_buffer_t *b0;                                                      \
      u32 bi0;                                                                \
      u32 next0 = next_index;                                                 \
      hicn_face_t *face = NULL;                                               \
                                                                              \
      /* Prefetch for next iteration. */                                      \
      if (n_left_from > 1)                                                    \
	{                                                                     \
	  vlib_buffer_t *b1;                                                  \
	  b1 = vlib_get_buffer (vm, from[1]);                                 \
	  CLIB_PREFETCH (b1, CLIB_CACHE_LINE_BYTES, STORE);                   \
	  CLIB_PREFETCH (b1->data, CLIB_CACHE_LINE_BYTES, STORE);             \
	}                                                                     \
      /* Dequeue a packet buffer */                                           \
      bi0 = from[0];                                                          \
      from += 1;                                                              \
      n_left_from -= 1;                                                       \
      to_next[0] = bi0;                                                       \
      to_next += 1;                                                           \
      n_left_to_next -= 1;                                                    \
                                                                              \
      b0 = vlib_get_buffer (vm, bi0);                                         \
                                                                              \
      hicn_face_id_t face_id = vnet_buffer (b0)->ip.adj_index[VLIB_TX];       \
      if (PREDICT_TRUE (hicn_dpoi_idx_is_valid (face_id)))                    \
	face = hicn_dpoi_get_from_idx (face_id);                              \
                                                                              \
      if (PREDICT_TRUE (face != NULL))                                        \
	{                                                                     \
	  HICN_REWRITE_DATA_IP##ipv (vm, b0, face, &next0);                   \
	  stats.pkts_data_count += 1;                                         \
	  vlib_increment_combined_counter (                                   \
	    &counters[face_id * HICN_N_COUNTER], thread_index,                \
	    HICN_FACE_COUNTERS_DATA_TX, 1,                                    \
	    vlib_buffer_length_in_chain (vm, b0));                            \
	}                                                                     \
                                                                              \
      if (PREDICT_FALSE ((node->flags & VLIB_NODE_FLAG_TRACE) &&              \
			 (b0->flags & VLIB_BUFFER_IS_TRACED)))                \
	{                                                                     \
	  TRACE_OUTPUT_PKT_IP##ipv *t =                                       \
	    vlib_add_trace (vm, node, b0, sizeof (*t));                       \
	  t->pkt_type = HICN_PACKET_TYPE_INTEREST;                            \
	  t->sw_if_index = vnet_buffer (b0)->sw_if_index[VLIB_RX];            \
	  t->next_index = next0;                                              \
	  clib_memcpy_fast (t->packet_data, vlib_buffer_get_current (b0),     \
			    sizeof (t->packet_data));                         \
	}                                                                     \
                                                                              \
      /* Verify speculative enqueue, maybe switch current next frame */       \
      vlib_validate_buffer_enqueue_x1 (vm, node, next_index, to_next,         \
				       n_left_to_next, bi0, next0);           \
    }                                                                         \
  while (0);

#define iface_output_x2(ipv)                                                  \
  do                                                                          \
    {                                                                         \
      vlib_buffer_t *b0, *b1;                                                 \
      u32 bi0, bi1;                                                           \
      u32 next0 = next_index;                                                 \
      u32 next1 = next_index;                                                 \
      hicn_face_t *face0 = NULL, *face1 = NULL;                               \
                                                                              \
      /* Prefetch for next iteration. */                                      \
      {                                                                       \
	vlib_buffer_t *b2, *b3;                                               \
	b2 = vlib_get_buffer (vm, from[2]);                                   \
	b3 = vlib_get_buffer (vm, from[3]);                                   \
	CLIB_PREFETCH (b2, CLIB_CACHE_LINE_BYTES, STORE);                     \
	CLIB_PREFETCH (b3, CLIB_CACHE_LINE_BYTES, STORE);                     \
	CLIB_PREFETCH (b2->data, CLIB_CACHE_LINE_BYTES, STORE);               \
	CLIB_PREFETCH (b3->data, CLIB_CACHE_LINE_BYTES, STORE);               \
      }                                                                       \
                                                                              \
      /* Dequeue a packet buffer */                                           \
      bi0 = from[0];                                                          \
      bi1 = from[1];                                                          \
      from += 2;                                                              \
      n_left_from -= 2;                                                       \
      to_next[0] = bi0;                                                       \
      to_next[1] = bi1;                                                       \
      to_next += 2;                                                           \
      n_left_to_next -= 2;                                                    \
                                                                              \
      b0 = vlib_get_buffer (vm, bi0);                                         \
      b1 = vlib_get_buffer (vm, bi1);                                         \
                                                                              \
      hicn_face_id_t face_id0 = vnet_buffer (b0)->ip.adj_index[VLIB_TX];      \
      hicn_face_id_t face_id1 = vnet_buffer (b1)->ip.adj_index[VLIB_TX];      \
      if (PREDICT_TRUE (hicn_dpoi_idx_is_valid (face_id0)))                   \
	face0 = hicn_dpoi_get_from_idx (face_id0);                            \
      if (PREDICT_TRUE (hicn_dpoi_idx_is_valid (face_id1)))                   \
	face1 = hicn_dpoi_get_from_idx (face_id1);                            \
                                                                              \
      if (PREDICT_TRUE (face0 != NULL))                                       \
	{                                                                     \
	  HICN_REWRITE_DATA_IP##ipv (vm, b0, face0, &next0);                  \
	  stats.pkts_data_count += 1;                                         \
	  vlib_increment_combined_counter (                                   \
	    &counters[face_id0 * HICN_N_COUNTER], thread_index,               \
	    HICN_FACE_COUNTERS_DATA_TX, 1,                                    \
	    vlib_buffer_length_in_chain (vm, b0));                            \
	}                                                                     \
                                                                              \
      if (PREDICT_TRUE (face1 != NULL))                                       \
	{                                                                     \
	  HICN_REWRITE_DATA_IP##ipv (vm, b1, face1, &next1);                  \
	  stats.pkts_data_count += 1;                                         \
	  vlib_increment_combined_counter (                                   \
	    &counters[face_id1 * HICN_N_COUNTER], thread_index,               \
	    HICN_FACE_COUNTERS_DATA_TX, 1,                                    \
	    vlib_buffer_length_in_chain (vm, b1));                            \
	}                                                                     \
                                                                              \
      if (PREDICT_FALSE ((node->flags & VLIB_NODE_FLAG_TRACE) &&              \
			 (b0->flags & VLIB_BUFFER_IS_TRACED)))                \
	{                                                                     \
	  TRACE_OUTPUT_PKT_IP##ipv *t =                                       \
	    vlib_add_trace (vm, node, b0, sizeof (*t));                       \
	  t->pkt_type = HICN_PACKET_TYPE_INTEREST;                            \
	  t->sw_if_index = vnet_buffer (b0)->sw_if_index[VLIB_RX];            \
	  t->next_index = next0;                                              \
	  clib_memcpy_fast (t->packet_data, vlib_buffer_get_current (b0),     \
			    sizeof (t->packet_data));                         \
	}                                                                     \
                                                                              \
      if (PREDICT_FALSE ((node->flags & VLIB_NODE_FLAG_TRACE) &&              \
			 (b1->flags & VLIB_BUFFER_IS_TRACED)))                \
	{                                                                     \
	  TRACE_OUTPUT_PKT_IP##ipv *t =                                       \
	    vlib_add_trace (vm, node, b1, sizeof (*t));                       \
	  t->pkt_type = HICN_PACKET_TYPE_INTEREST;                            \
	  t->sw_if_index = vnet_buffer (b1)->sw_if_index[VLIB_RX];            \
	  t->next_index = next1;                                              \
	  clib_memcpy_fast (t->packet_data, vlib_buffer_get_current (b1),     \
			    sizeof (t->packet_data));                         \
	}                                                                     \
                                                                              \
      /* Verify speculative enqueue, maybe switch current next frame */       \
      vlib_validate_buffer_enqueue_x2 (vm, node, next_index, to_next,         \
				       n_left_to_next, bi0, bi1, next0,       \
				       next1);                                \
    }                                                                         \
  while (0);

static uword
hicn4_iface_output_node_fn (vlib_main_t *vm, vlib_node_runtime_t *node,
			    vlib_frame_t *frame)
{
  u32 n_left_from, *from, *to_next, next_index;
  vl_api_hicn_api_node_stats_get_reply_t stats = { 0 };
  u32 thread_index = vm->thread_index;

  from = vlib_frame_vector_args (frame);
  n_left_from = frame->n_vectors;
  next_index = HICN4_IFACE_OUTPUT_NEXT_DROP;

  while (n_left_from > 0)
    {
      u32 n_left_to_next;
      vlib_get_next_frame (vm, node, next_index, to_next, n_left_to_next);

      while (n_left_from >= 4 && n_left_to_next >= 2)
	{
	  iface_output_x2 (4);
	}

      while (n_left_from > 0 && n_left_to_next > 0)
	{
	  iface_output_x1 (4);
	}

      vlib_put_next_frame (vm, node, next_index, n_left_to_next);
    }

  vlib_node_increment_counter (vm, node->node_index, HICNFWD_ERROR_DATAS,
			       stats.pkts_data_count);

  return (frame->n_vectors);
}

/* packet trace format function */
static u8 *
hicn4_iface_output_format_trace (u8 *s, va_list *args)
{
  CLIB_UNUSED (vlib_main_t * vm) = va_arg (*args, vlib_main_t *);
  CLIB_UNUSED (vlib_node_t * node) = va_arg (*args, vlib_node_t *);
  hicn4_iface_output_trace_t *t = va_arg (*args, hicn4_iface_output_trace_t *);

  s =
    format (s, "IFACE_IP4_OUTPUT: pkt: %d, sw_if_index %d, next index %d\n%U",
	    (int) t->pkt_type, t->sw_if_index, t->next_index,
	    format_ip4_header, t->packet_data, sizeof (t->packet_data));
  return (s);
}

/*
 * Node registration for the interest forwarder node
 */
VLIB_REGISTER_NODE (hicn4_iface_output_node) = {
  .function = hicn4_iface_output_node_fn,
  .name = "hicn4-iface-output",
  .vector_size = sizeof (u32),
  .format_trace = hicn4_iface_output_format_trace,
  .type = VLIB_NODE_TYPE_INTERNAL,
  .n_errors = ARRAY_LEN (hicn4_iface_output_error_strings),
  .error_strings = hicn4_iface_output_error_strings,
  .n_next_nodes = HICN4_IFACE_OUTPUT_N_NEXT,
  /* edit / add dispositions here */
  .next_nodes = { [HICN4_IFACE_OUTPUT_NEXT_DROP] = "error-drop",
		  [HICN4_IFACE_OUTPUT_NEXT_LOOKUP] = "ip4-lookup",
		  [HICN4_IFACE_OUTPUT_NEXT_UDP4_ENCAP] = "udp4-encap",
		  [HICN4_IFACE_OUTPUT_NEXT_UDP6_ENCAP] = "udp6-encap",
		  [HICN4_IFACE_OUTPUT_NEXT_PG] = "hicnpg-data" },
};

static uword
hicn6_iface_output_node_fn (vlib_main_t *vm, vlib_node_runtime_t *node,
			    vlib_frame_t *frame)
{
  u32 n_left_from, *from, *to_next, next_index;
  vl_api_hicn_api_node_stats_get_reply_t stats = { 0 };
  u32 thread_index = vm->thread_index;

  from = vlib_frame_vector_args (frame);
  n_left_from = frame->n_vectors;
  next_index = HICN6_IFACE_OUTPUT_NEXT_DROP;

  while (n_left_from > 0)
    {
      u32 n_left_to_next;
      vlib_get_next_frame (vm, node, next_index, to_next, n_left_to_next);

      while (n_left_from >= 4 && n_left_to_next >= 2)
	{
	  iface_output_x2 (6);
	}

      while (n_left_from > 0 && n_left_to_next > 0)
	{
	  iface_output_x1 (6);
	}
      vlib_put_next_frame (vm, node, next_index, n_left_to_next);
    }

  vlib_node_increment_counter (vm, node->node_index, HICNFWD_ERROR_DATAS,
			       stats.pkts_data_count);

  return (frame->n_vectors);
}

/* packet trace format function */
static u8 *
hicn6_iface_output_format_trace (u8 *s, va_list *args)
{
  CLIB_UNUSED (vlib_main_t * vm) = va_arg (*args, vlib_main_t *);
  CLIB_UNUSED (vlib_node_t * node) = va_arg (*args, vlib_node_t *);
  hicn6_iface_output_trace_t *t = va_arg (*args, hicn6_iface_output_trace_t *);

  s =
    format (s, "IFACE_IP6_OUTPUT: pkt: %d, sw_if_index %d, next index %d\n%U",
	    (int) t->pkt_type, t->sw_if_index, t->next_index,
	    format_ip6_header, t->packet_data, sizeof (t->packet_data));
  return (s);
}

/*
 * Node registration for the interest forwarder node
 */
VLIB_REGISTER_NODE (hicn6_iface_output_node) = {
  .function = hicn6_iface_output_node_fn,
  .name = "hicn6-iface-output",
  .vector_size = sizeof (u32),
  .format_trace = hicn6_iface_output_format_trace,
  .type = VLIB_NODE_TYPE_INTERNAL,
  .n_errors = ARRAY_LEN (hicn6_iface_output_error_strings),
  .error_strings = hicn6_iface_output_error_strings,
  .n_next_nodes = HICN6_IFACE_OUTPUT_N_NEXT,
  /* edit / add dispositions here */
  .next_nodes = { [HICN6_IFACE_OUTPUT_NEXT_DROP] = "error-drop",
		  [HICN6_IFACE_OUTPUT_NEXT_LOOKUP] = "ip6-lookup",
		  [HICN6_IFACE_OUTPUT_NEXT_UDP4_ENCAP] = "udp4-encap",
		  [HICN6_IFACE_OUTPUT_NEXT_UDP6_ENCAP] = "udp6-encap",
		  [HICN6_IFACE_OUTPUT_NEXT_PG] = "hicnpg-data" },
};

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
