#include <math.h>
#include <hicn/ctrl/command.h>
#include "../objects/route.h"

/* Parameters */

#define symbolic_or_id                                                        \
  {                                                                           \
    .name = "symbolic_or_id",                                                 \
    .help =                                                                   \
        "The symbolic name for an egress, or the egress route id (see 'help " \
        "list routes')",                                                      \
    .type = TYPE_SYMBOLIC_OR_ID, .offset = offsetof(hc_route_t, face_name),   \
  }

#define prefix                                                           \
  {                                                                      \
    .name = "prefix",                                                    \
    .help = "The hicn name as IPv4 or IPv6 address (e.g 1234::0/64).",   \
    .type = TYPE_IP_PREFIX, .offset = offsetof(hc_route_t, remote_addr), \
    .offset2 = offsetof(hc_route_t, len),                                \
    .offset3 = offsetof(hc_route_t, family),                             \
  }

#define cost                                                        \
  {                                                                 \
    .name = "cost", .help = "Positive integer representing cost.",  \
    .type = TYPE_INT(1, 255), .offset = offsetof(hc_route_t, cost), \
  }

/* Face parameters */

#define type_tcp_udp                                                         \
  {                                                                          \
    .name = "type", .help = "face type [tcp | udp]",                         \
    .type = TYPE_ENUM(face_type), .offset = offsetof(hc_route_t, face.type), \
  }

#define local_address                                                         \
  {                                                                           \
    .name = "local_addr", .help = "local IP address on which to bind.",       \
    .type = TYPE_IP_ADDRESS, .offset = offsetof(hc_route_t, face.local_addr), \
    .offset2 = offsetof(hc_route_t, face.family),                             \
  }

#define local_port                                   \
  {                                                  \
    .name = "local_port", .help = "Local port.",     \
    .type = TYPE_UINT16(1, UINT16_MAX),              \
    .offset = offsetof(hc_route_t, face.local_port), \
  }

#define remote_address                                                         \
  {                                                                            \
    .name = "remote_address",                                                  \
    .help = "The IPv4 or IPv6 or hostname of the remote system.",              \
    .type = TYPE_IP_ADDRESS, .offset = offsetof(hc_route_t, face.remote_addr), \
    .offset2 = offsetof(hc_route_t, face.family),                              \
  }

#define remote_port                                   \
  {                                                   \
    .name = "remote_port", .help = "Remote port.",    \
    .type = TYPE_UINT16(1, UINT16_MAX),               \
    .offset = offsetof(hc_route_t, face.remote_port), \
  }

#define interface                                              \
  {                                                            \
    .name = "interface", .help = "Interface on which to bind", \
    .type = TYPE_INTERFACE_NAME,                               \
    .offset = offsetof(hc_route_t, face.netdevice.name),       \
  }

/* Commands */
int on_route_create(hc_route_t* route) {
  if (hc_route_has_face(route)) {
    route->face.admin_state = FACE_STATE_UP;
    route->face.id = INVALID_FACE_ID;
  }
  return 0;
}

static const command_parser_t command_route_create3 = {
    .action = ACTION_CREATE,
    .object_type = OBJECT_TYPE_ROUTE,
    .nparams = 3,
    .parameters = {symbolic_or_id, prefix, cost},
};
COMMAND_REGISTER(command_route_create3);

static const command_parser_t command_route_create5 = {
    .action = ACTION_CREATE,
    .object_type = OBJECT_TYPE_ROUTE,
    .nparams = 5,
    .parameters = {prefix, cost, type_tcp_udp, remote_address, remote_port},
    .post_hook = (parser_hook_t)on_route_create,
};
COMMAND_REGISTER(command_route_create5);

static const command_parser_t command_route_create6 = {
    .action = ACTION_CREATE,
    .object_type = OBJECT_TYPE_ROUTE,
    .nparams = 6,
    .parameters = {prefix, cost, type_tcp_udp, remote_address, remote_port,
                   interface},
    .post_hook = (parser_hook_t)on_route_create,
};
COMMAND_REGISTER(command_route_create6);

static const command_parser_t command_route_create7 = {
    .action = ACTION_CREATE,
    .object_type = OBJECT_TYPE_ROUTE,
    .nparams = 7,
    .parameters = {prefix, cost, type_tcp_udp, local_address, local_port,
                   remote_address, remote_port},
    .post_hook = (parser_hook_t)on_route_create,
};
COMMAND_REGISTER(command_route_create7);

static const command_parser_t command_route_create8 = {
    .action = ACTION_CREATE,
    .object_type = OBJECT_TYPE_ROUTE,
    .nparams = 8,
    .parameters = {prefix, cost, type_tcp_udp, local_address, local_port,
                   remote_address, remote_port, interface},
    .post_hook = (parser_hook_t)on_route_create,
};
COMMAND_REGISTER(command_route_create8);

static const command_parser_t command_route_list = {
    .action = ACTION_LIST,
    .object_type = OBJECT_TYPE_ROUTE,
    .nparams = 0,
};
COMMAND_REGISTER(command_route_list);

static const command_parser_t command_route_remove = {
    .action = ACTION_DELETE,
    .object_type = OBJECT_TYPE_ROUTE,
    .nparams = 2,
    .parameters = {symbolic_or_id, prefix},
};
COMMAND_REGISTER(command_route_remove);
