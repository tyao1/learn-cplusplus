#include <iostream>
#include <fstream>
#include <spdlog/spdlog.h>
#include <chrono>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <json/json.hpp>

#include <openssl/sha.h>
#include <bot/secrets.hpp>
#include <bot/base64.hpp>
#include <bot/parsers.hpp>

// for convenience
using json = nlohmann::json;
using namespace std;

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
namespace spd = spdlog;



#define DEBUG
#ifdef DEBUG
#define INTERVAL 30
#define LOG(logger, ...) logger->debug(__VA_ARGS__)
#else
#define INTERVAL 60
#define LOG(logger, ...)
#endif

#define UA "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_13_1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/62.0.3202.94 Safari/537.36"


// Performs an HTTP GET and prints the response
class worker : public std::enable_shared_from_this<worker> {

    tcp::resolver resolver_;
    ssl::stream<tcp::socket> stream_;
    boost::beast::flat_buffer buffer_; // (Must persist between reads)
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
    boost::asio::steady_timer timer_;
    std::unique_ptr<IResultParser> parser_;
    std::shared_ptr<spdlog::logger> my_logger;
    std::shared_ptr<spdlog::logger> console;

    string host_;
    string port_;
    string target_;
    int count_;

    vector<report> datas_;

public:
    worker(worker&&) = default;
    // Resolver and socket require an io_context
    worker(
      boost::asio::io_context& ioc,
      ssl::context& ctx,
      string host,
      string port,
      string target,
      IResultParser* parser
    )
        : resolver_(ioc),stream_(ioc, ctx), parser_{parser},
        host_{host}, port_{port}, target_{target},
        timer_(ioc, (chrono::steady_clock::time_point::max)()), count_{0}

    {
      // parser_ = parser,
      // Set up an HTTP GET request message
      req_.version(11);
      req_.method(http::verb::get);
      req_.target(target_);
      req_.set(http::field::host, host_);
      req_.set(http::field::user_agent, UA);

      console = spd::stdout_color_mt("bot");
      string path = "logs.txt";
      /*
      if (argc < 2) {
        console->info("Log path not defined, use default logs.txt");
        path = "logs.txt";
      } else {
        path = argv[1];
      }
      */

      LOG(console, "START {} {} ", 1, 3.23);
      try {
          my_logger = spdlog::basic_logger_mt("record", path);
      }
      catch (const spdlog::spdlog_ex& ex) {
          cout << "Log initialization failed: " << ex.what() << endl;
          my_logger = console;
      }
      my_logger->info("initialized");
    }

    // err callback
    void fail(boost::system::error_code ec, char const* what) {
      LOG(console, what);
      LOG(console, ec.message());
      // console->error((string)what + ": " + ec.message());
      // my_logger->error((string)what + ": " + ec.message());

    }
    void init() {
      if(! SSL_set_tlsext_host_name(stream_.native_handle(), host_.c_str())) {
        boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
        fail(ec, "SSL set tlsext host name");
        return;
      }
      // start ticking!
      on_timer({});
    }

    void prepare_payload() {
      LOG(console, "payload");
      json body;
      body["request"] = target_;
      body["nounce"] = to_string(std::chrono::seconds(std::time(NULL)).count() * 1000);
      string body_json = body.dump();
      string body_json_base64 = base64_encode(body_json);
      LOG(console, "secret!");

      constexpr char key[] = API_SECRET;
      // The data that we're going to hash using HMAC
      auto data = body_json_base64.c_str();
      unsigned char* digest2;
      digest2 = HMAC(EVP_sha384(), key, strlen(key), (unsigned char*)data, strlen(data), NULL, NULL);
      char result[SHA384_DIGEST_LENGTH * 2 + 1];
      for(int i = 0; i < SHA384_DIGEST_LENGTH; i++) {
        sprintf(&result[i*2], "%02x", (unsigned int)digest2[i]);
      }
      string signature{result};
      // http::header<true> header;64
      req_.clear();
      req_.body() = body_json;
      LOG(console, body_json);
      req_.set("X-BFX-APIKEY", API_KEY);
      req_.set("X-BFX-PAYLOAD", body_json_base64);
      req_.set("X-BFX-SIGNATURE", signature);
      cout << req_.body() << endl;
    }
    // Start the asynchronous operation, connect and fetch
    void run() {
      console->info("Start a new round:");
      console->info(++count_);

      // Look up the domain name
      resolver_.async_resolve(
        host_,
        port_,
        std::bind(
          &worker::on_resolve,
          shared_from_this(),
          std::placeholders::_1,
          std::placeholders::_2));
    }

    void on_timer(const boost::system::error_code& e){
      timer_.expires_after(chrono::seconds(INTERVAL));
      timer_.async_wait(bind(&worker::on_timer, shared_from_this(), placeholders::_1));
      run();
    }

    void on_resolve(
      boost::system::error_code ec,
      tcp::resolver::results_type results) {
      if(ec) return fail(ec, "resolve");
      // ips_ = results;
      // on_timer({});

      // Make the connection on the IP address we get from a lookup
      boost::asio::async_connect(
        stream_.next_layer(),
        results.begin(),
        results.end(),
        std::bind(
          &worker::on_connect,
          shared_from_this(),
          std::placeholders::_1));
    }


    void on_connect(boost::system::error_code ec) {
        if(ec) return fail(ec, "connect");
        // Perform the SSL handshake
        stream_.async_handshake(
            ssl::stream_base::client,
            std::bind(
                &worker::on_handshake,
                shared_from_this(),
                std::placeholders::_1));
    }

    void on_handshake(boost::system::error_code ec) {
      if(ec) return fail(ec, "handshake");
      // calculate request header
      prepare_payload();
      LOG(console, "will write");
      // Send the HTTP request to the remote host
      http::async_write(stream_, req_,
        std::bind(
          &worker::on_write,
          shared_from_this(),
          std::placeholders::_1,
          std::placeholders::_2));
    }

    void on_write(
      boost::system::error_code ec,
      std::size_t bytes_transferred) {
      boost::ignore_unused(bytes_transferred);
      if(ec) return fail(ec, "write");
      LOG(console, "write!");

      // Receive the HTTP response
      http::async_read(stream_, buffer_, res_,
        std::bind(
          &worker::on_read,
          shared_from_this(),
          std::placeholders::_1,
          std::placeholders::_2));
    }

    void on_read(
      boost::system::error_code ec,
      std::size_t bytes_transferred) {
      boost::ignore_unused(bytes_transferred);
      LOG(console, "read!");

      if(ec) return fail(ec, "read");

      // Write the message to standard out
      // cout << res_.body() << endl;
      LOG(console, res_.body());
      LOG(console, res_.result_int());
      // LOG(console, res_.base());
      string body = res_.body();
      parser_->parse_result(body);
      LOG(console, "here we go");
      my_logger->info("boom!");
      res_.body().clear();
      LOG(console, "here we don't go ?");

      // buffer_.consume(buffer_.size());
      // Gracefully close the socket

      stream_.async_shutdown(
        std::bind(
          &worker::on_shutdown,
          shared_from_this(),
          std::placeholders::_1));
          LOG(console, "here we go go?");

    }

    void on_shutdown(boost::system::error_code ec) {
      LOG(console, "wat??");
      if (ec == boost::asio::error::eof || (ec.category() == boost::asio::error::get_ssl_category() &&
			   ec.value() == ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ))) {
        // ec.assign(0, ec.category());
      } else {
        LOG(console, "shutdown?!?!");
        return fail(ec, "shutdown");
      }
      // If we get here then the connection is closed gracefully
    }


};

int main(int argc, char* argv[]) {
  spdlog::set_level(spdlog::level::debug);


  // The io_context is required for all I/O
  boost::asio::io_context ioc;
  ssl::context ctx{ssl::context::sslv23_client};
  // load_root_certificates(ctx);

  auto worker1 = std::make_shared<worker>(ioc, ctx, "localhost", "12345", "/v1/pubticker/BTCUSD", new PriceParser());

  worker1->init();
  // Launch the asynchronous operation
  // std::make_shared<worker>(ioc)->run("www.baidu.com", "80", "/");

  // Run the I/O service. The call will return when
  // the get operation is complete.
  ioc.run();

  spd::drop_all();
}
