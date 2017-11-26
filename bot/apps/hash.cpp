#include <iostream>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <string.h>

#include <string>
#include <map>
#include <json/json.hpp>

#include <bot/secrets.hpp>
#include <bot/base64.hpp>
#include <ctime>
#include <chrono>

using json = nlohmann::json;

int main () {
  std::cout << API_KEY << std::endl;
  std::cout << API_SECRET << std::endl;
  unsigned char digest[SHA384_DIGEST_LENGTH];

  char string[] = "hello world";
  SHA384((unsigned char*)&string, strlen(string), (unsigned char*)&digest);

  char mdString[SHA384_DIGEST_LENGTH*2+1];

  for(int i = 0; i < SHA384_DIGEST_LENGTH; i++)
       sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);

  printf("SHA384 digest: %s\n", mdString);

  // The key to hash
  char key[] = "012345678";
  // The data that we're going to hash using HMAC
  char data[] = "hello world";
  unsigned char* digest2;

  // Using sha1 hash engine here.
  // You may use other hash engines. e.g EVP_md5(), EVP_sha224, EVP_sha512, etc
  digest2 = HMAC(EVP_sha384(), key, strlen(key), (unsigned char*)data, strlen(data), NULL, NULL);

  // Be careful of the length of string with the choosen hash engine. SHA1 produces a 20-byte hash value which rendered as 40 characters.
  // Change the length accordingly with your choosen hash engine
  char result[SHA384_DIGEST_LENGTH * 2 + 1];
  for(int i = 0; i < SHA384_DIGEST_LENGTH; i++)
       sprintf(&mdString[i*2], "%02x", (unsigned int)digest2[i]);

  printf("HMAC digest: %s\n", mdString);

  std::string str(mdString);
  std::cout << str << std::endl;
  json body;
  body["request"] = "/v1/account_infos";
  body["nouce"] = 12345678;
  std::cout << body << std::endl;

  uint64_t unix_timestamp = std::chrono::seconds(std::time(NULL)).count() * 1000;
  // uint64_t unix_timestamp_x_1000 = std::chrono::milliseconds(unix_timestamp).count();

  std::cout << unix_timestamp << std::endl;
}
