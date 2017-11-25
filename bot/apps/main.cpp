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

// for convenience
using json = nlohmann::json;
using namespace std;

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
namespace spd = spdlog;



// #define DEBUG
#ifdef DEBUG
#define INTERVAL 5
#define LOG(logger, ...) logger->debug(__VA_ARGS__)
#else
#define INTERVAL 30
#define LOG(logger, ...)
#endif

#define UA "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_13_1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/62.0.3202.94 Safari/537.36"

std::shared_ptr<spdlog::logger> my_logger;
std::shared_ptr<spdlog::logger> console;

// err callback
void fail(boost::system::error_code ec, char const* what) {
  console->error((string)what + ": " + ec.message());
  my_logger->error((string)what + ": " + ec.message());
}

class report {
public:
    double mid;
    double bid;
    double ask;
    double last_price;
    double low;
    double high;
    double volume;
    double timestamp;
    report()
      :mid{0}, bid{0}, ask{0}, last_price{0}, low{0},
      high{0}, volume{0}, timestamp{0} {
    }



     string get_result() const {
      return "Mid:" + to_string(mid) + "/" + "Low:" + to_string(low) + '\n';
    }

};
void to_json(json& j, const report& p) {
    j = json{{"mid", to_string(p.mid)}, {"bid", to_string(p.bid)},
     {"ask", to_string(p.ask)}, {"last_price", to_string(p.last_price)},
     {"low", to_string(p.low)}, {"high", to_string(p.high)},
     {"volume", to_string(p.volume)}, {"timestamp", to_string(p.timestamp)}};
}

void from_json(const json& j, report& p) {
    p.mid = stod(j.at("mid").get<std::string>());
    p.bid = stod(j.at("bid").get<std::string>());
    p.ask = stod(j.at("ask").get<std::string>());
    p.low = stod(j.at("low").get<std::string>());
    p.high = stod(j.at("high").get<std::string>());
    p.last_price = stod(j.at("last_price").get<std::string>());
    p.volume = stod(j.at("volume").get<std::string>());
    p.timestamp = stod(j.at("timestamp").get<std::string>());
}
ostream& operator<<(ostream& os, const report& rp) {
  os << rp.get_result();
  return os;
}
// Performs an HTTP GET and prints the response
class worker : public std::enable_shared_from_this<worker> {

    tcp::resolver resolver_;
    ssl::stream<tcp::socket> stream_;
    boost::beast::flat_buffer buffer_; // (Must persist between reads)
    http::request<http::empty_body> req_;
    http::response<http::string_body> res_;
    boost::asio::steady_timer timer_;

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
      string target
    )
        : resolver_(ioc),stream_(ioc, ctx),
        host_{host}, port_{port}, target_{target},
        timer_(ioc, (chrono::steady_clock::time_point::max)()), count_{0}

    {

      // Set up an HTTP GET request message
      req_.version(11);
      req_.method(http::verb::get);
      req_.target(target_);
      req_.set(http::field::host, host_);
      req_.set(http::field::user_agent, UA);
    }


    void init() {
      if(! SSL_set_tlsext_host_name(stream_.native_handle(), host_.c_str())) {
        boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
        fail(ec, "SSL set tlsext host name");
        return;
      }
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

    // Start the asynchronous operation, connect and fetch
    void run() {
      console->info("Start a new round:");
      console->info(++count_);
      // Send the HTTP request to the remote host
      http::async_write(stream_, req_,
        std::bind(
          &worker::on_write,
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
      console->info("Start!");
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

      on_timer({});
    }

    void on_write(
      boost::system::error_code ec,
      std::size_t bytes_transferred) {
      boost::ignore_unused(bytes_transferred);
      if(ec) return fail(ec, "write");

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

      if(ec) return fail(ec, "read");

      // Write the message to standard out
      // cout << res_.body() << endl;
      json j = json::parse(res_.body());
      report p = j;
      cout << p << endl;
      datas_.push_back(p);
      res_.body().clear();
      // buffer_.consume(buffer_.size());
      // Gracefully close the socket
      /*
      stream_.async_shutdown(
        std::bind(
          &worker::on_shutdown,
          shared_from_this(),
          std::placeholders::_1));
      */
      /*
      socket_.shutdown(tcp::socket::shutdown_both, ec);
      */
    }

    void on_shutdown(boost::system::error_code ec) {
      if (ec == boost::asio::error::eof || (ec.category() == boost::asio::error::get_ssl_category() &&
			   ec.value() == ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ))) {
        // ec.assign(0, ec.category());
      } else {
        return fail(ec, "shutdown");
      }
      // If we get here then the connection is closed gracefully
    }
};

int main(int argc, char* argv[]) {
  /* Initialize logging*/
  spdlog::set_level(spdlog::level::debug);
  //Multithreaded console logger(with color support)
  console = spd::stdout_color_mt("bot");
  string path;
  if (argc < 2) {
    console->info("Log path not defined, use default logs.txt");
    path = "logs.txt";
  } else {
    path = argv[1];
  }


  LOG(console, "START {} {} ", 1, 3.23);
  console->info(path);
  try {
      my_logger = spdlog::basic_logger_mt("record", path);
  }
  catch (const spdlog::spdlog_ex& ex) {
      cout << "Log initialization failed: " << ex.what() << endl;
      my_logger = console;
  }
  my_logger->info("initialized");


  auto const host = "api.bitfinex.com";
  auto const port = "443";
  auto const target = "/v1/pubticker/BTCUSD";

  // The io_context is required for all I/O
  boost::asio::io_context ioc;
  ssl::context ctx{ssl::context::sslv23_client};
  // load_root_certificates(ctx);

  auto worker1 = std::make_shared<worker>(ioc, ctx, host, port, target);
  worker1->init();
  // Launch the asynchronous operation
  // std::make_shared<worker>(ioc)->run("www.baidu.com", "80", "/");

  // Run the I/O service. The call will return when
  // the get operation is complete.
  ioc.run();

  spd::drop_all();
}
