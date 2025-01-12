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

#ifndef __HICN_API_H__
#define __HICN_API_H__

/**
 * @file
 */

#define HICN_STRATEGY_NULL ~0
#define HICN_FIB_TABLE	   10

/* define message structures */
#define vl_typedefs
#include <vpp_plugins/hicn/hicn_all_api_h.h>
#undef vl_typedefs

#endif /* // __HICN_API_H___ */

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables: eval: (c-set-style "gnu") End:
 */
