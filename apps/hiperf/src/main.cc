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

#include <client.h>
#include <hicn/apps/utils/logger.h>
#include <server.h>

namespace hiperf {

static std::unordered_map<std::string, hicn_packet_format_t> const
    packet_format_map = {{"ipv4_tcp", HICN_PACKET_FORMAT_IPV4_TCP},
                         {"ipv6_tcp", HICN_PACKET_FORMAT_IPV6_TCP},
                         {"new", HICN_PACKET_FORMAT_NEW}};

#define TO_LOWER(s)                             \
  std::transform(s.begin(), s.end(), s.begin(), \
                 [](unsigned char c) { return std::tolower(c); });

void usage() {
  LoggerInfo() << "HIPERF - Instrumentation tool for performing active network"
                  "measurements with hICN";
  LoggerInfo() << "usage: hiperf [-S|-C] [options] [prefix|name]";
  LoggerInfo();
  LoggerInfo() << "SERVER OR CLIENT:";
#ifndef _WIN32
  LoggerInfo() << "-D\t\t\t\t\t"
               << "Run as a daemon";
  LoggerInfo() << "-R\t\t\t\t\t"
               << "Run RTC protocol (client or server)";
  LoggerInfo() << "-f\t<filename>\t\t\t"
               << "Log file";
  LoggerInfo() << "-z\t<io_module>\t\t\t"
               << "IO module to use. Default: hicnlight_module";
  LoggerInfo() << "-F\t<conf_file>\t\t\t"
               << "Path to optional configuration file for libtransport";
  LoggerInfo() << "-a\t\t\t\t\t"
               << "Enables data packet aggregation. "
               << "Works only in RTC mode";
  LoggerInfo() << "-X\t<param>\t\t\t\t"
               << "Set FEC params. Options are Rely_K#_N# or RS_K#_N#";
  LoggerInfo()
      << "-J\t<passphrase>\t\t\t"
      << "Set the passphrase used to sign/verify aggregated interests. "
         "If set on the client, aggregated interests are enable automatically.";
#endif
  LoggerInfo();
  LoggerInfo() << "SERVER SPECIFIC:";
  LoggerInfo()
      << "-A\t<content_size>\t\t\t"
         "Sends an application data unit in bytes that is published once "
         "before exit";
  LoggerInfo() << "-E\t<expiry_time>\t\t\t"
                  "Expiration time for data packets generated by the producer "
                  "socket";
  LoggerInfo() << "-s\t<packet_size>\t\t\tData packet payload size.";
  LoggerInfo() << "-r\t\t\t\t\t"
               << "Produce real content of <content_size> bytes";
  LoggerInfo()
      << "-m\t<manifest_max_capacity>\t\t"
      << "The maximum number of entries a manifest can contain. Set it "
         "to 0 to disable manifests. Default is 30, max is 255.";
  LoggerInfo() << "-l\t\t\t\t\t"
               << "Start producing content upon the reception of the "
                  "first interest";
  LoggerInfo() << "-K\t<keystore_path>\t\t\t"
               << "Path of p12 file containing the "
                  "crypto material used for signing packets";
  LoggerInfo() << "-k\t<passphrase>\t\t\t"
               << "String from which a 128-bit symmetric key will be "
                  "derived for signing packets";
  LoggerInfo() << "-p\t<password>\t\t\t"
               << "Password for p12 keystore";
  LoggerInfo() << "-y\t<hash_algorithm>\t\t"
               << "Use the selected hash algorithm for "
                  "computing manifest digests (default: SHA256)";
  LoggerInfo() << "-x\t\t\t\t\t"
               << "Produces application data units of size <content_size> "
               << "without resetting the name suffix to 0.";
  LoggerInfo() << "-B\t<bitrate>\t\t\t"
               << "RTC producer data bitrate, to be used with the -R option.";
#ifndef _WIN32
  LoggerInfo() << "-I\t\t\t\t\t"
                  "Interactive mode, start/stop real time content production "
                  "by pressing return. To be used with the -R option";
  LoggerInfo()
      << "-T\t<filename>\t\t\t"
         "Trace based mode, hiperf takes as input a file with a trace. "
         "Each line of the file indicates the timestamp and the size of "
         "the packet to generate. To be used with the -R option. -B and -I "
         "will be ignored.";
  LoggerInfo() << "-G\t<port>\t\t\t\t"
               << "Input stream from localhost at the specified port";
#endif
  LoggerInfo();
  LoggerInfo() << "CLIENT SPECIFIC:";
  LoggerInfo() << "-b\t<beta_parameter>\t\t"
               << "RAAQM beta parameter";
  LoggerInfo() << "-d\t<drop_factor_parameter>\t\t"
               << "RAAQM drop factor "
                  "parameter";
  LoggerInfo() << "-L\t<interest lifetime>\t\t"
               << "Set interest lifetime.";
  LoggerInfo() << "-U\t<factor>\t\t\t"
               << "Update the relevance threshold: if an unverified packet has "
                  "been received before the last U * manifest_max_capacity_ "
                  "packets received (verified or not), it will be flushed out. "
                  "Should be > 1, default is 100.";
  LoggerInfo()
      << "-u\t<factor>\t\t\t"
      << "Update the alert threshold: if the "
         "number of unverified packet is > u * manifest_max_capacity_, "
         "an alert is raised. Should be set such that U > u >= 1, "
         "default is 20. If u >= U, no alert will ever be raised.";
  LoggerInfo() << "-M\t<input_buffer_size>\t\t"
               << "Size of consumer input buffer. If 0, reassembly of packets "
                  "will be disabled.";
  LoggerInfo()
      << "-N\t\t\t\t\t"
      << "Enable aggregated interests; the number of suffixes (including "
         "the one in the header) can be set through the env variable "
         "`MAX_AGGREGATED_INTERESTS`.";
  LoggerInfo() << "-W\t<window_size>\t\t\t"
               << "Use a fixed congestion window "
                  "for retrieving the data.";
  LoggerInfo() << "-i\t<stats_interval>\t\t"
               << "Show the statistics every <stats_interval> milliseconds.";
  LoggerInfo()
      << "-c\t<certificate_path>\t\t"
      << "Path of the producer certificate to be used for verifying the "
         "origin of the packets received.";
  LoggerInfo()
      << "-k\t<passphrase>\t\t\t"
      << "String from which is derived the symmetric key used by the "
         "producer to sign packets and by the consumer to verify them.";
  LoggerInfo() << "-t\t\t\t\t\t"
                  "Test mode, check if the client is receiving the "
                  "correct data. This is an RTC specific option, to be "
                  "used with the -R (default: false)";
  LoggerInfo()
      << "-P\t\t\t\t\t"
      << "Number of parallel streams. For hiperf client, this is the "
         "number of consumer to create, while for hiperf server this is "
         "the number of producers to create.";
  LoggerInfo() << "-j\t<relay_name>\t\t\t"
               << "Publish received content under the name relay_name."
                  "This is an RTC specific option, to be "
                  "used with the -R (default: false)";
  LoggerInfo() << "-g\t<port>\t\t\t\t"
               << "Output stream to localhost at the specified port";
  LoggerInfo()
      << "-o\t\t\t\t\t"
      << "Content sharing mode: if set the socket work in content sharing"
      << "mode. It works only in RTC mode";
  LoggerInfo() << "-e\t<strategy>\t\t\t"
               << "Enhance the network with a reliability strategy. Options";
  LoggerInfo() << "\t\t\t\t\t\t1: unreliable ";
  LoggerInfo() << "\t\t\t\t\t\t2: rtx only ";
  LoggerInfo() << "\t\t\t\t\t\t3: fec only ";
  LoggerInfo() << "\t\t\t\t\t\t4: delay based ";
  LoggerInfo() << "\t\t\t\t\t\t5: low rate ";
  LoggerInfo() << "\t\t\t\t\t\t6: low rate and best path ";
  LoggerInfo() << "\t\t\t\t\t\t7: low rate and replication";
  LoggerInfo() << "\t\t\t\t\t\t8: low rate and best path/replication ";
  LoggerInfo() << "\t\t\t\t\t\t9: only fec low residual losses ";
  LoggerInfo() << "\t\t\t\t\t\t10: delay and best path ";
  LoggerInfo() << "\t\t\t\t\t\t11: delay and replication ";
  LoggerInfo() << "\t\t\t\t\t\t(default: 2 = rtx only) ";
  LoggerInfo() << "-H\t\t\t\t\t"
               << "Disable periodic print headers in stats report.";
  LoggerInfo() << "-n\t<nb_iterations>\t\t\t"
               << "Print the stats report <nb_iterations> times and exit.\n"
               << "\t\t\t\t\tThis option limits the duration of the run to "
                  "<nb_iterations> * <stats_interval> milliseconds.";
  LoggerInfo() << "-w <packet_format> Packet format (without signature, "
                  "defaults to IPV6_TCP)";
}

int main(int argc, char *argv[]) {
#ifndef _WIN32
  // Common
  bool daemon = false;
#else
  WSADATA wsaData = {0};
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

  transport::interface::global_config::GlobalConfigInterface global_conf;

  // -1 server, 0 undefined, 1 client
  int role = 0;
  int options = 0;

  char *log_file = nullptr;
  transport::interface::global_config::IoModuleConfiguration config;
  std::string conf_file;
  config.name = "hicnlight_module";

  // Consumer
  ClientConfiguration client_configuration;

  // Producer
  ServerConfiguration server_configuration;

  int opt;
#ifndef _WIN32
  // Please keep in alphabetical order.
  while (
      (opt = getopt(argc, argv,
                    "A:B:CDE:F:G:HIJ:K:L:M:NP:RST:U:W:X:ab:c:d:e:f:g:hi:j:k:lm:"
                    "n:op:qrs:tu:vw:xy:z:")) != -1) {
    switch (opt) {
      // Common
      case 'D': {
        daemon = true;
        break;
      }
      case 'I': {
        server_configuration.interactive_ = true;
        server_configuration.trace_based_ = false;
        server_configuration.input_stream_mode_ = false;
        break;
      }
      case 'T': {
        server_configuration.interactive_ = false;
        server_configuration.trace_based_ = true;
        server_configuration.input_stream_mode_ = false;
        server_configuration.trace_file_ = optarg;
        break;
      }
      case 'G': {
        server_configuration.interactive_ = false;
        server_configuration.trace_based_ = false;
        server_configuration.input_stream_mode_ = true;
        server_configuration.port_ = std::stoul(optarg);
        break;
      }
      case 'g': {
        client_configuration.output_stream_mode_ = true;
        client_configuration.port_ = std::stoul(optarg);
        break;
      }
#else
  // Please keep in alphabetical order.
  while ((opt = getopt(argc, argv,
                       "A:B:CE:F:HK:L:M:P:RSU:W:X:ab:c:d:e:f:hi:j:k:lm:n:op:rs:"
                       "tu:vwxy:z:")) != -1) {
    switch (opt) {
#endif
      case 'E': {
        server_configuration.content_lifetime_ = std::stoul(optarg);
        break;
      }
      case 'f': {
        log_file = optarg;
        break;
      }
      case 'R': {
        client_configuration.rtc_ = true;
        server_configuration.rtc_ = true;
        break;
      }
      case 'a': {
        client_configuration.aggregated_data_ = true;
        server_configuration.aggregated_data_ = true;
        break;
      }
      case 'o': {
        client_configuration.content_sharing_mode_ = true;
        break;
      }
      case 'w': {
        std::string packet_format_s = std::string(optarg);
        TO_LOWER(packet_format_s);
        auto it = packet_format_map.find(std::string(optarg));
        if (it == packet_format_map.end())
          throw std::runtime_error("Bad packet format");
        client_configuration.packet_format_ = it->second;
        server_configuration.packet_format_ = it->second;
        break;
      }
      case 'k': {
        server_configuration.passphrase_ = std::string(optarg);
        client_configuration.passphrase_ = std::string(optarg);
        break;
      }
      case 'z': {
        config.name = optarg;
        break;
      }
      case 'F': {
        conf_file = optarg;
        break;
      }

      // Server or Client
      case 'S': {
        role -= 1;
        break;
      }
      case 'C': {
        role += 1;
        break;
      }
      case 'q': {
        client_configuration.colored_ = server_configuration.colored_ = false;
        break;
      }
      case 'J': {
        client_configuration.aggr_interest_passphrase_ = optarg;
        server_configuration.aggr_interest_passphrase_ = optarg;
        // Consumer signature is only used with aggregated interests,
        // hence enabling it also forces usage of aggregated interests
        client_configuration.aggregated_interests_ = true;
        break;
      }
      // Client specifc
      case 'b': {
        client_configuration.beta_ = std::stod(optarg);
        options = 1;
        break;
      }
      case 'd': {
        client_configuration.drop_factor_ = std::stod(optarg);
        options = 1;
        break;
      }
      case 'W': {
        client_configuration.window_ = std::stod(optarg);
        options = 1;
        break;
      }
      case 'M': {
        client_configuration.receive_buffer_size_ = std::stoull(optarg);
        options = 1;
        break;
      }
      case 'N': {
        client_configuration.aggregated_interests_ = true;
        break;
      }
      case 'P': {
        client_configuration.parallel_flows_ =
            server_configuration.parallel_flows_ = std::stoull(optarg);
        break;
      }
      case 'c': {
        client_configuration.producer_certificate_ = std::string(optarg);
        options = 1;
        break;
      }
      case 'i': {
        client_configuration.report_interval_milliseconds_ = std::stoul(optarg);
        options = 1;
        break;
      }
      case 't': {
        client_configuration.test_mode_ = true;
        options = 1;
        break;
      }
      case 'L': {
        client_configuration.interest_lifetime_ = std::stoul(optarg);
        options = 1;
        break;
      }
      case 'U': {
        client_configuration.manifest_factor_relevant_ = std::stoul(optarg);
        options = 1;
        break;
      }
      case 'u': {
        client_configuration.manifest_factor_alert_ = std::stoul(optarg);
        options = 1;
        break;
      }
      case 'j': {
        client_configuration.relay_ = true;
        client_configuration.relay_name_ = Prefix(optarg);
        options = 1;
        break;
      }
      case 'H': {
        client_configuration.print_headers_ = false;
        options = 1;
        break;
      }
      case 'n': {
        client_configuration.nb_iterations_ = std::stoul(optarg);
        options = 1;
        break;
      }
      // Server specific
      case 'A': {
        server_configuration.download_size_ = std::stoul(optarg);
        options = -1;
        break;
      }
      case 's': {
        server_configuration.payload_size_ = std::stoul(optarg);
        options = -1;
        break;
      }
      case 'r': {
        server_configuration.virtual_producer_ = false;
        options = -1;
        break;
      }
      case 'm': {
        server_configuration.manifest_max_capacity_ = std::stoul(optarg);
        options = -1;
        break;
      }
      case 'l': {
        server_configuration.live_production_ = true;
        options = -1;
        break;
      }
      case 'K': {
        server_configuration.keystore_name_ = std::string(optarg);
        options = -1;
        break;
      }
      case 'y': {
        CryptoHashType hash_algorithm = CryptoHashType::SHA256;
        if (strncasecmp(optarg, "sha256", 6) == 0) {
          hash_algorithm = CryptoHashType::SHA256;
        } else if (strncasecmp(optarg, "sha512", 6) == 0) {
          hash_algorithm = CryptoHashType::SHA512;
        } else if (strncasecmp(optarg, "blake2b512", 10) == 0) {
          hash_algorithm = CryptoHashType::BLAKE2B512;
        } else if (strncasecmp(optarg, "blake2s256", 10) == 0) {
          hash_algorithm = CryptoHashType::BLAKE2S256;
        } else {
          LoggerWarn() << "Unknown hash algorithm. Using SHA 256.";
        }
        server_configuration.hash_algorithm_ = hash_algorithm;
        options = -1;
        break;
      }
      case 'p': {
        server_configuration.keystore_password_ = std::string(optarg);
        options = -1;
        break;
      }
      case 'x': {
        server_configuration.multiphase_produce_ = true;
        options = -1;
        break;
      }
      case 'B': {
        auto str = std::string(optarg);
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        server_configuration.production_rate_ = str;
        options = -1;
        break;
      }
      case 'e': {
        client_configuration.recovery_strategy_ = std::stoul(optarg);
        options = 1;
        break;
      }
      case 'X': {
        server_configuration.fec_type_ = std::string(optarg);
        options = -1;
        break;
      }
      case 'h':
      default:
        usage();
        return EXIT_FAILURE;
    }
  }

  if (options > 0 && role < 0) {
    LoggerErr() << "Client options cannot be used when using the "
                   "software in server mode";
    usage();
    return EXIT_FAILURE;
  } else if (options < 0 && role > 0) {
    LoggerErr() << "Server options cannot be used when using the "
                   "software in client mode";
    usage();
    return EXIT_FAILURE;
  } else if (!role) {
    LoggerErr() << "Please specify if running hiperf as client "
                   "or server.";
    usage();
    return EXIT_FAILURE;
  }

  if (argv[optind] == 0) {
    LoggerErr() << "Please specify the name/prefix to use.";
    usage();
    return EXIT_FAILURE;
  } else {
    if (role > 0) {
      client_configuration.name_ = Prefix(argv[optind]);
    } else {
      server_configuration.name_ = Prefix(argv[optind]);
    }
  }

  if (log_file) {
#ifndef _WIN32
    int fd = open(log_file, O_WRONLY | O_APPEND | O_CREAT, S_IWUSR | S_IRUSR);
    dup2(fd, STDOUT_FILENO);
    dup2(STDOUT_FILENO, STDERR_FILENO);
    close(fd);
#else
    int fd =
        _open(log_file, _O_WRONLY | _O_APPEND | _O_CREAT, _S_IWRITE | _S_IREAD);
    _dup2(fd, _fileno(stdout));
    _dup2(_fileno(stdout), _fileno(stderr));
    _close(fd);
#endif
  }

#ifndef _WIN32
  if (daemon) {
    utils::Daemonizator::daemonize(false);
  }
#endif

  /**
   * IO module configuration
   */
  config.set();

  // Parse config file
  global_conf.parseConfigurationFile(conf_file);

  if (role > 0) {
    HIperfClient c(client_configuration);
    if (c.setup() != ERROR_SETUP) {
      c.run();
    }
  } else if (role < 0) {
    HIperfServer s(server_configuration);
    if (s.setup() != ERROR_SETUP) {
      s.run();
    }
  } else {
    usage();
    return EXIT_FAILURE;
  }

#ifdef _WIN32
  WSACleanup();
#endif

  return 0;
}

}  // namespace hiperf

int main(int argc, char *argv[]) { return hiperf::main(argc, argv); }
