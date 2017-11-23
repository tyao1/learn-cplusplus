#include <iostream>
#include <fstream>
#include <spdlog/spdlog.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio.hpp>

using namespace std;
namespace spd = spdlog;


#define DEBUG
#ifdef DEBUG
#define LOG(logger, ...) logger->debug(__VA_ARGS__)
#else
#define LOG(logger, ...)
#endif

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


  spd::drop_all();
}
