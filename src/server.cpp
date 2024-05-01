#include "server.h"
#include "client.h"
#include "crypto.h"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<std::string> pending_trxs;
Server::Server() {}

std::shared_ptr<Client> Server::add_client(std::string id) {
  std::string end;
  const auto retry_time = 5;
  std::mt19937 rng(std::time(0)); // 使用当前时间作为随机数生成器的种子
  for (size_t i = 0; i < retry_time; i++) {
    auto real_id = id + end;
    if (std::find_if(clients.cbegin(), clients.cend(),
                     [&real_id](const auto &ele) {
                       return ele.first->get_id() == real_id;
                     }) == clients.cend()) {
      auto client = std::make_shared<Client>(real_id, *this);
      clients[client] = 5.0;
      return client;
    }
    end = "";
    for (auto ii = 0; ii < 4; ii++) {
      end += std::to_string('a' + rng() % 26);
    }
  }
  throw std::logic_error("id is conflict");
}

double Server::get_wallet(std::string id) const {
  auto it =
      std::find_if(clients.cbegin(), clients.cend(), [&id](const auto &ele) {
        return ele.first->get_id() == id;
      });
  if (it == clients.cend()) {
    throw std::logic_error("id is not exist");
  }
  return it->second;
}

std::shared_ptr<Client> Server::get_client(std::string id) const {
  auto it =
      std::find_if(clients.cbegin(), clients.cend(), [&id](const auto &ele) {
        return ele.first->get_id() == id;
      });
  if (it == clients.cend()) {
    return nullptr;
  }
  return it->first;
}

void show_wallets(const Server &server) {
  std::cout << std::string(20, '*') << std::endl;
  for (const auto &client : server.clients)
    std::cout << client.first->get_id() << " : " << client.second << std::endl;
  std::cout << std::string(20, '*') << std::endl;
}

bool Server::parse_trx(std::string trx, std::string &sender,
                       std::string &receiver, double &value) {
  std::istringstream trx_str_stream(trx);
  std::vector<std::string> splited_trx;
  std::string token;
  while (std::getline(trx_str_stream, token, '-')) {
    splited_trx.emplace_back(token);
  }
  if (splited_trx.size() != 3) {
    throw std::runtime_error(
        "illegal trx format, should be sender-receiver-value");
  }
  std::istringstream money_iss(splited_trx[2]);
  if (!(money_iss >> value)) { // 如果解析失败
    throw std::runtime_error(
        "value is not double, should be sender-receiver-value");
  }
  sender = splited_trx[0];
  receiver = splited_trx[1];
  return true;
}

bool Server::add_pending_trx(std::string trx, std::string signature) const {
  std::string sender, receiver;
  double value;
  if (!Server::parse_trx(trx, sender, receiver, value)) {
    return false;
  }
  if (value < 0) {
    return false;
  }
  auto sender_client = get_client(sender);
  auto receiver_client = get_client(receiver);
  if (sender_client == nullptr || receiver_client == nullptr) {
    return false;
  }

  if (!crypto::verifySignature(sender_client->get_publickey(), trx,
                               signature)) {
    return false;
  }
  if (sender_client->get_wallet() < value) {
    return false;
  }
  pending_trxs.emplace_back(trx);
  return true;
}

size_t Server::mine() {
  std::string memo_pool;
  for (auto &trx : pending_trxs) {
    memo_pool += trx;
  }
  while (true) {
    for (auto &[client, left_money] : clients) {
      auto nonce = client->generate_nonce();
      auto will_hash = memo_pool + std::to_string(nonce);
      std::string hash{crypto::sha256(will_hash)};
      auto count = 0;
      bool found = false;
      auto index = 0;
      for (const auto &ch : hash) {
        if (index == 10) {
          break;
        }
        index++;
        if (ch != '0') {
          count = 0;
          continue;
        }
        count++;
        if (count == 3) {
          found = true;
          break;
        }
      }
      if (found) {
        this->clients[client] += 6.25;
        std::cout << "client: " << client->get_id()
                  << " mine success, nonce:" << nonce << std::endl;
        std::cout << "found hash: " << hash << std::endl;
        std::cout << "memopool: " << memo_pool << std::endl;
        for (auto &trx : pending_trxs) {
          std::string sender, receiver;
          double value;
          auto ok = Server::parse_trx(trx, sender, receiver, value);
          if (!ok) {
            throw std::runtime_error("illegal trx format");
          }
          auto sender_kv =
              std::find_if(this->clients.begin(), this->clients.end(),
                           [&sender](const auto &ele) {
                             return ele.first->get_id() == sender;
                           });
          auto recevier_kv =
              std::find_if(this->clients.begin(), this->clients.end(),
                           [&receiver](const auto &ele) {
                             return ele.first->get_id() == receiver;
                           });
          if (sender_kv == this->clients.end() ||
              recevier_kv == this->clients.end()) {
            throw std::runtime_error("not exist clients");
          }
          sender_kv->second -= value;
          recevier_kv->second += value;
        }
        pending_trxs.clear();
        return nonce;
      }
    }
  }
  return 0;
}