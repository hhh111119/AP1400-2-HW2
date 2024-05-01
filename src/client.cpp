#include "client.h"
#include "server.h"

#include "crypto.h"

#include <chrono>
#include <cstddef>
#include <limits>
#include <random>

Client::Client(std::string id, const Server &server) : id{id}, server{&server} {
  std::string public_key{}, private_key{};
  crypto::generate_key(public_key, private_key);
  this->public_key = public_key;
  this->private_key = private_key;
}

std::string Client::get_id() { return id; }

double Client::get_wallet() { return this->server->get_wallet(this->id); }

std::string Client::get_publickey() const { return this->public_key; }

std::string Client::sign(std::string txt) const {
  return crypto::signMessage(this->private_key, txt);
}

bool Client::transfer_money(std::string receiver, double value) {
  if (this->get_wallet() < value) {
    return false;
  }
  if (this->server->get_client(receiver) == nullptr) {
    return false;
  }
  auto transer_trx = this->id + "-" + receiver + "-" + std::to_string(value);
  return this->server->add_pending_trx(transer_trx, this->sign(transer_trx));
}

size_t Client::generate_nonce() {
  // 利用当前时间作为随机数生成器的种子
  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::default_random_engine generator(seed);
  // 我们使用 std::uniform_int_distribution 来保证生成的数字是均匀分布的
  // 注意：下界和上界需要根据你的实际需要来设定
  std::uniform_int_distribution<size_t> distribution(std::numeric_limits<size_t>::min(), std::numeric_limits<size_t>::max());
  size_t random_num = distribution(generator);
  return random_num;
}