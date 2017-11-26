#include <iostream>
#include <string>
#include <json/json.hpp>
#include <vector>

using namespace std;
using json = nlohmann::json;

class IResultParser {
public:
  void parse_result(const string &str) {
    cout << "called interface" << endl;
    parse(str);
  }
  virtual ~IResultParser() = default;
private:
  // virtual implementation non-pure
  virtual void parse(const string &str) {};
};

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
      return "Mid:" + to_string(mid) + "/" + "Low:" + to_string(low) + "/" + "High:" + to_string(high) + '\n'
         + "Ask:" + to_string(ask) + '/'  + "Bid:" + to_string(bid) + "/Volume:" + to_string(volume) + '\n';
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

class PriceParser : public IResultParser {
private:
  vector<report> datas_;
  virtual void parse(const string &str) {
    std::cout << str << std::endl;
    try {
      json j = json::parse(str);
      report p = j;
      cout << p << endl;
      datas_.push_back(p);
    } catch (exception ec) {
      // console->warn(ec.what());
      cout << ec.what() << endl;
    }
  }
};
