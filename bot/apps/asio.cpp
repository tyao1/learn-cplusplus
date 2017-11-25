#include <iostream>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace std;

class printer
{
public:
  printer(boost::asio::io_context& io)
    : strand_(io),
      timer1_(io, boost::posix_time::seconds(1)),
      timer2_(io, boost::posix_time::seconds(1)),
      timer3_(io, boost::posix_time::seconds(1)),
      count_{0},
      count2_{0}
  {
    timer1_.async_wait(boost::asio::bind_executor(strand_, std::bind(&printer::print1, this)));
    timer2_.async_wait(boost::asio::bind_executor(strand_, std::bind(&printer::print2, this)));
    timer3_.async_wait(boost::asio::bind_executor(strand_, std::bind(&printer::print3, this)));
  }
  ~printer()
  {
    std::cout << "Final count is " << count_ << "\n";
  }
  void print1()
  {
    if (count_ < 10)
    {
      std::cout << "Timer 1: " << count_ << "\n";
      ++count_;

      timer1_.expires_at(timer1_.expires_at() + boost::posix_time::seconds(1));
      timer1_.async_wait(boost::asio::bind_executor(strand_, std::bind(&printer::print1, this)));
    }
  }

  void print2()
  {
    if (count_ < 10)
    {
      std::cout << "Timer 2: " << count_ << "\n";
      ++count_;

      timer2_.expires_at(timer2_.expires_at() + boost::posix_time::seconds(1));
      timer2_.async_wait(boost::asio::bind_executor(strand_, std::bind(&printer::print2, this)));
    }
  }

  void print3() {
    if (count2_ < 10) {
      cout << "Timer 3：" << count2_++ << endl;
      timer3_.expires_at(timer2_.expires_at() + boost::posix_time::seconds(1));
      timer3_.async_wait(boost::asio::bind_executor(strand_, std::bind(&printer::print3, this)));
    }
  }

private:
  boost::asio::io_context::strand strand_;
  boost::asio::deadline_timer timer1_;
  boost::asio::deadline_timer timer2_;
  boost::asio::deadline_timer timer3_;
  int count_;
  int count2_;
};

int main()
{
  boost::asio::io_context io;
  printer p(io);
  boost::thread t(boost::bind(&boost::asio::io_context::run, &io));
  boost::thread t2(boost::bind(&boost::asio::io_context::run, &io));

  io.run();
  t.join();
  t2.join();

  return 0;
}
