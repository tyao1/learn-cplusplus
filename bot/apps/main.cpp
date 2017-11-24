#include <iostream>
#include <fstream>
#include <spdlog/spdlog.h>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

using namespace std;
using boost::asio::ip::tcp;
namespace http = boost::beast::http;
namespace spd = spdlog;



#define DEBUG
#ifdef DEBUG
#define LOG(logger, ...) logger->debug(__VA_ARGS__)
#else
#define LOG(logger, ...)
#endif


// err callback
void fail(boost::system::error_code ec, char const* what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

// Performs an HTTP GET and prints the response
class session : public std::enable_shared_from_this<session> {
    tcp::resolver resolver_;
    tcp::socket socket_;
    boost::beast::flat_buffer buffer_; // (Must persist between reads)
    http::request<http::empty_body> req_;
    http::response<http::string_body> res_;

public:
    // Resolver and socket require an io_context
    explicit session(boost::asio::io_context& ioc)
        : resolver_(ioc), socket_(ioc) {
    }

    // Start the asynchronous operation
    void run(
      char const* host,
      char const* port,
      char const* target) {

      // Set up an HTTP GET request message
      req_.version(11);
      req_.method(http::verb::get);
      req_.target(target);
      req_.set(http::field::host, host);
      req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

      // Look up the domain name
      resolver_.async_resolve(
        host,
        port,
        std::bind(
          &session::on_resolve,
          shared_from_this(),
          std::placeholders::_1,
          std::placeholders::_2));
    }

    void on_resolve(
      boost::system::error_code ec,
      tcp::resolver::results_type results) {

      if(ec) return fail(ec, "resolve");

      // Make the connection on the IP address we get from a lookup
      boost::asio::async_connect(
        socket_,
        results.begin(),
        results.end(),
        std::bind(
          &session::on_connect,
          shared_from_this(),
          std::placeholders::_1));
    }

    void on_connect(boost::system::error_code ec) {
      if(ec) return fail(ec, "connect");

      // Send the HTTP request to the remote host
      http::async_write(socket_, req_,
        std::bind(
          &session::on_write,
          shared_from_this(),
          std::placeholders::_1,
          std::placeholders::_2));
    }

    void on_write(
        boost::system::error_code ec,
        std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if(ec) return fail(ec, "write");

        // Receive the HTTP response
        http::async_read(socket_, buffer_, res_,
            std::bind(
                &session::on_read,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2));
    }

    void on_read(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec) return fail(ec, "read");

        // Write the message to standard out
        std::cout << res_ << std::endl;

        // Gracefully close the socket
        socket_.shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes so don't bother reporting it.
        if(ec && ec != boost::system::errc::not_connected)
            return fail(ec, "shutdown");

        // If we get here then the connection is closed gracefully
    }
};

int main(int argc, char* argv[]) {
  /* Initialize logging*/
  spdlog::set_level(spdlog::level::debug);
  //Multithreaded console logger(with color support)
  auto console = spd::stdout_color_mt("bot");
  string path;
  if (argc < 2) {
    console->info("Log path not defined, use default logs.txt");
    path = "logs.txt";
  } else {
    path = argv[1];
  }

  std::shared_ptr<spdlog::logger> my_logger;
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


  auto const host = "www.google.com";
  auto const port = "80";
  auto const target = "/";

  // The io_context is required for all I/O
  boost::asio::io_context ioc;

  // Launch the asynchronous operation
  std::make_shared<session>(ioc)->run(host, port, target);
  std::make_shared<session>(ioc)->run("www.baidu.com", "80", "/");

  // Run the I/O service. The call will return when
  // the get operation is complete.
  ioc.run();

  spd::drop_all();
}
