/*
 * Copyright (c) 2021 Cisco and/or its affiliates.
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

/*
 * Copyright (c) 2021 by Cisco Systems Inc. All Rights Reserved.
 *
 */

#ifndef HICN_MAPME_ACK_H
#define HICN_MAPME_ACK_H

#include <vlib/vlib.h>
#include <vnet/vnet.h>

/**
 * @file
 *
 */

/* Node context data */
typedef struct hicn_mapme_ack_runtime_s
{
  int id;
} hicn_mapme_ack_runtime_t;

/* Trace context struct */
typedef struct
{
  u32 next_index;
  u32 sw_if_index;
  u8 pkt_type;
} hicn_mapme_ack_trace_t;

typedef enum
{
  HICN_MAPME_ACK_NEXT_ERROR_DROP,
  HICN_MAPME_ACK_N_NEXT,
} hicn_mapme_ack_next_t;

#endif /* HICN_MAPME_ACK_H */

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables: eval: (c-set-style "gnu") End:
 */
