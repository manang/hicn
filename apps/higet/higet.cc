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
 * WITHout_ WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hicn/apps/utils/logger.h>
#include <hicn/transport/http/client_connection.h>
#include <hicn/transport/utils/chrono_typedefs.h>

#include <algorithm>
#include <asio.hpp>
#include <fstream>
#include <functional>
#include <map>

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#include <asio.hpp>
#include <thread>

#define DEFAULT_BETA 0.99
#define DEFAULT_GAMMA 0.07

namespace http {

typedef struct {
  std::string file_name;
  bool print_headers;
  std::string producer_certificate;
  std::string ipv6_first_word;
} Configuration;

class ReadBytesCallbackImplementation
    : public transport::http::HTTPClientConnection::ReadBytesCallback {
  static std::string chunk_separator;

 public:
  ReadBytesCallbackImplementation(std::string file_name, long yet_downloaded)
      : file_name_(file_name),
        temp_file_name_(file_name_ + ".temp"),
        yet_downloaded_(yet_downloaded),
        byte_downloaded_(yet_downloaded),
        chunked_(false),
        chunk_size_(0),
        work_(std::make_unique<asio::io_service::work>(io_service_)),
        thread_(
            std::make_unique<std::thread>([this]() { io_service_.run(); })) {
    std::streambuf *buf;
    if (file_name_ != "-") {
      of_.open(temp_file_name_, std::ofstream::binary | std::ofstream::app);
      buf = of_.rdbuf();
    } else {
      buf = std::cout.rdbuf();
    }

    out_ = new std::ostream(buf);
  }

  ~ReadBytesCallbackImplementation() {
    if (thread_->joinable()) {
      thread_->join();
    }
  }

  void onBytesReceived(std::unique_ptr<utils::MemBuf> &&buffer) {
    auto buffer_ptr = buffer.release();
    io_service_.post([this, buffer_ptr]() {
      auto buffer = std::unique_ptr<utils::MemBuf>(buffer_ptr);
      std::unique_ptr<utils::MemBuf> payload;
      if (!first_chunk_read_) {
        transport::http::HTTPResponse http_response(std::move(buffer));
        payload = http_response.getPayload();
        auto header = http_response.getHeaders();
        content_size_ = yet_downloaded_;
        std::map<std::string, std::string>::iterator it =
            header.find("Content-Length");
        if (it != header.end()) {
          content_size_ += std::stol(it->second);
        } else {
          it = header.find("Transfer-Encoding");
          if (it != header.end() && it->second.compare("chunked") == 0) {
            chunked_ = true;
          }
        }
        first_chunk_read_ = true;
      } else {
        payload = std::move(buffer);
      }

      if (chunked_) {
        if (chunk_size_ > 0) {
          out_->write((char *)payload->data(), chunk_size_);
          payload->trimStart(chunk_size_);

          if (payload->length() >= chunk_separator.size()) {
            payload->trimStart(chunk_separator.size());
          }
        }

        while (payload->length() > 0) {
          // read next chunk size
          const char *begin = (const char *)payload->data();
          const char *end = (const char *)payload->tail();
          const char *begincrlf2 = (const char *)chunk_separator.c_str();
          const char *endcrlf2 = begincrlf2 + chunk_separator.size();
          auto it = std::search(begin, end, begincrlf2, endcrlf2);
          if (it != end) {
            chunk_size_ = std::stoul(begin, 0, 16);
            content_size_ += (long)chunk_size_;
            payload->trimStart(it + chunk_separator.size() - begin);

            std::size_t to_write;
            if (payload->length() >= chunk_size_) {
              to_write = chunk_size_;
            } else {
              to_write = payload->length();
              chunk_size_ -= payload->length();
            }

            out_->write((char *)payload->data(), to_write);
            byte_downloaded_ += (long)to_write;
            payload->trimStart(to_write);

            if (payload->length() >= chunk_separator.size()) {
              payload->trimStart(chunk_separator.size());
            }
          }
        }
      } else {
        out_->write((char *)payload->data(), payload->length());
        byte_downloaded_ += (long)payload->length();
      }

      if (file_name_ != "-") {
        print_bar(byte_downloaded_, content_size_, false);
      }
    });
  }

  void onSuccess(std::size_t bytes) {
    io_service_.post([this, bytes]() {
      if (file_name_ != "-") {
        of_.close();
        delete out_;
        std::size_t found = file_name_.find_last_of(".");
        std::string name = file_name_.substr(0, found);
        std::string extension = file_name_.substr(found + 1);
        if (!exists_file(file_name_)) {
          std::rename(temp_file_name_.c_str(), file_name_.c_str());
        } else {
          int i = 1;
          std::ostringstream sstream;
          sstream << name << "(" << i << ")." << extension;
          std::string final_name = sstream.str();
          while (exists_file(final_name)) {
            i++;
            sstream.str("");
            sstream << name << "(" << i << ")." << extension;
            final_name = sstream.str();
          }
          std::rename(temp_file_name_.c_str(), final_name.c_str());
        }

        print_bar(100, 100, true);
        LoggerInfo() << "\nDownloaded " << bytes << " bytes";
      }
      work_.reset();
    });
  }

  void onError(const std::error_code &ec) {
    io_service_.post([this]() {
      of_.close();
      delete out_;
      work_.reset();
    });
  }

 private:
  bool exists_file(const std::string &name) {
    std::ifstream f(name.c_str());
    return f.good();
  }

  void print_bar(long value, long max_value, bool last) {
    float progress = (float)value / max_value;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    int barWidth = csbi.srWindow.Right - csbi.srWindow.Left + 7;
#else
    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    int barWidth = size.ws_col - 8;
#endif

    std::cout << "[";
    int pos = barWidth * (int)progress;
    for (int i = 0; i < barWidth; ++i) {
      if (i < pos) {
        std::cout << "=";
      } else if (i == pos) {
        std::cout << ">";
      } else {
        std::cout << " ";
      }
    }
    if (last) {
      std::cout << "] " << int(progress * 100.0) << " %";
    } else {
      std::cout << "] " << int(progress * 100.0) << " %\r";
      std::cout.flush();
    }
  }

 private:
  std::string file_name_;
  std::string temp_file_name_;
  std::ostream *out_;
  std::ofstream of_;
  long yet_downloaded_;
  long content_size_;
  bool first_chunk_read_ = false;
  long byte_downloaded_ = 0;
  bool chunked_;
  std::size_t chunk_size_;
  asio::io_service io_service_;
  std::unique_ptr<asio::io_service::work> work_;
  std::unique_ptr<std::thread> thread_;
};

std::string ReadBytesCallbackImplementation::chunk_separator = "\r\n";

long checkFileStatus(std::string file_name) {
  struct stat stat_buf;
  std::string temp_file_name_ = file_name + ".temp";
  int rc = stat(temp_file_name_.c_str(), &stat_buf);
  return rc == 0 ? stat_buf.st_size : -1;
}

void usage(char *program_name) {
  LoggerInfo() << "usage:";
  LoggerInfo() << program_name << " [option]... [url]...";
  LoggerInfo() << program_name << " options:";
  LoggerInfo()
      << "-O <out_put_path>            = write documents to <out_put_file>";
  LoggerInfo() << "-S                          = print server response";
  LoggerInfo()
      << "-P                          = first word of the ipv6 name of "
         "the response";
  LoggerInfo() << "example:";
  LoggerInfo() << "\t" << program_name << " -O - http://origin/index.html";
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
#ifdef _WIN32
  WSADATA wsaData = {0};
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

  Configuration conf;
  conf.file_name = "";
  conf.print_headers = false;
  conf.producer_certificate = "";
  conf.ipv6_first_word = "b001";

  std::string name("http://webserver/sintel/mpd");

  int opt;
  while ((opt = getopt(argc, argv, "O:Sc:P:")) != -1) {
    switch (opt) {
      case 'O':
        conf.file_name = optarg;
        break;
      case 'S':
        conf.print_headers = true;
        break;
      case 'c':
        conf.producer_certificate = optarg;
        break;
      case 'P':
        conf.ipv6_first_word = optarg;
        break;
      case 'h':
      default:
        usage(argv[0]);
        break;
    }
  }

  if (!argv[optind]) {
    usage(argv[0]);
  }

  name = argv[optind];
  LoggerInfo() << "Using name " << name << " and name first word "
               << conf.ipv6_first_word;

  if (conf.file_name.empty()) {
    conf.file_name = name.substr(1 + name.find_last_of("/"));
  }

  long yetDownloaded = checkFileStatus(conf.file_name);

  std::map<std::string, std::string> headers;
  if (yetDownloaded == -1) {
    headers = {{"Host", "localhost"},
               {"User-Agent", "higet/1.0"},
               {"Connection", "Keep-Alive"}};
  } else {
    std::string range;
    range.append("bytes=");
    range.append(std::to_string(yetDownloaded));
    range.append("-");
    headers = {{"Host", "localhost"},
               {"User-Agent", "higet/1.0"},
               {"Connection", "Keep-Alive"},
               {"Range", range}};
  }

  transport::http::HTTPClientConnection connection;

  if (!conf.producer_certificate.empty()) {
    std::shared_ptr<transport::auth::Verifier> verifier =
        std::make_shared<transport::auth::AsymmetricVerifier>(
            conf.producer_certificate);
    connection.setVerifier(verifier);
  }

  http::ReadBytesCallbackImplementation readBytesCallback(conf.file_name,
                                                          yetDownloaded);

  connection.get(name, headers, {}, nullptr, &readBytesCallback,
                 conf.ipv6_first_word);

#ifdef _WIN32
  WSACleanup();
#endif

  return EXIT_SUCCESS;
}

}  // end namespace http

int main(int argc, char **argv) { return http::main(argc, argv); }
