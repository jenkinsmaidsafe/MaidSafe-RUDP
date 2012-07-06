/*******************************************************************************
 *  Copyright 2012 MaidSafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of MaidSafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of MaidSafe.net. *
 ******************************************************************************/

#include "maidsafe/rudp/tests/test_utils.h"

#include <thread>
#include <set>

#include "boost/lexical_cast.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"

#include "maidsafe/common/log.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"
#include "maidsafe/rudp/utils.h"

namespace bptime = boost::posix_time;


namespace maidsafe {

namespace rudp {

namespace test {

uint16_t GetRandomPort() {
  static std::set<uint16_t> already_used_ports;
  bool unique(false);
  uint16_t port(0);
  do {
    port = (RandomUint32() % 48126) + 1025;
    unique = (already_used_ports.insert(port)).second;
  } while (!unique);
  return port;
}

testing::AssertionResult SetupNetwork(std::vector<NodePtr> &nodes,
                                      std::vector<Endpoint> &bootstrap_endpoints,
                                      const int &node_count) {
  if (node_count < 2)
    return testing::AssertionFailure() << "Network size must be greater than 1";

  nodes.clear();
  bootstrap_endpoints.clear();
  for (int i(0); i != node_count; ++i)
    nodes.push_back(std::make_shared<Node>(i));

  // Setting up first two nodes
  Endpoint endpoint0(GetLocalIp(), GetRandomPort()),
           endpoint1(GetLocalIp(), GetRandomPort()),
           chosen_endpoint;

  boost::thread thread([&] {
    chosen_endpoint = nodes[0]->Bootstrap(std::vector<Endpoint>(1, endpoint1), endpoint0);
  });
  if (nodes[1]->Bootstrap(std::vector<Endpoint>(1, endpoint0), endpoint1) != endpoint0)
    return testing::AssertionFailure() << "Bootstrapping failed for Node 1";

  thread.join();
  if (chosen_endpoint != endpoint1)
    return testing::AssertionFailure() << "Bootstrapping failed for Node 0";

  auto futures0(nodes[0]->GetFutureForMessages(1));
  auto futures1(nodes[1]->GetFutureForMessages(1));
  LOG(kInfo) << "Calling Add from " << endpoint0 << " to " << endpoint1;
  if (nodes[0]->managed_connections()->Add(endpoint0, endpoint1, nodes[0]->kValidationData()) !=
      kSuccess) {
    return testing::AssertionFailure() << "Node 0 failed to add Node 1";
  }
  nodes[0]->AddConnectedEndPoint(endpoint1);
  LOG(kInfo) << "Calling Add from " << endpoint1 << " to " << endpoint0;
  if (nodes[1]->managed_connections()->Add(endpoint1, endpoint0, nodes[1]->kValidationData()) !=
      kSuccess) {
    return testing::AssertionFailure() << "Node 1 failed to add Node 0";
  }
  nodes[1]->AddConnectedEndPoint(endpoint0);

  if (!futures0.timed_wait(bptime::seconds(3))) {
    return testing::AssertionFailure() << "Failed waiting for " << nodes[0]->kId()
        << " to receive " << nodes[1]->kId() << "'s validation data.";
  }
  if (!futures1.timed_wait(bptime::seconds(3))) {
    return testing::AssertionFailure() << "Failed waiting for " << nodes[1]->kId()
        << " to receive " << nodes[0]->kId() << "'s validation data.";
  }
  auto messages0(futures0.get());
  auto messages1(futures1.get());
  if (messages0.size() != 1U) {
    return testing::AssertionFailure() << nodes[0]->kId() << " has "
        << messages0.size() << " messages [should be 1].";
  }
  if (messages1.size() != 1U) {
    return testing::AssertionFailure() << nodes[1]->kId() << " has "
        << messages1.size() << " messages [should be 1].";
  }
  if (messages0[0] != nodes[1]->kValidationData()) {
    return testing::AssertionFailure() << nodes[0]->kId() << " has received " << nodes[1]->kId()
        << "'s validation data as " << messages0[0] << " [should be \""
        << nodes[1]->kValidationData() << "\"].";
  }
  if (messages1[0] != nodes[0]->kValidationData()) {
    return testing::AssertionFailure() << nodes[1]->kId() << " has received " << nodes[0]->kId()
        << "'s validation data as " << messages1[0] << " [should be \""
        << nodes[0]->kValidationData() << "\"].";
  }

  bootstrap_endpoints.push_back(endpoint0);
  bootstrap_endpoints.push_back(endpoint1);
  nodes[0]->ResetData();
  nodes[1]->ResetData();

  LOG(kInfo) << "Setting up remaining " << (node_count - 2) << " nodes";

  // Adding nodes to each other
  for (int i(2); i != node_count; ++i) {
    Endpoint chosen_endpoint(nodes[i]->Bootstrap(bootstrap_endpoints));
    if (!IsValid(chosen_endpoint))
      return testing::AssertionFailure() << "Bootstrapping failed for " << nodes[i]->kId();

    for (int j(0); j != i; ++j) {
      nodes[i]->ResetData();
      nodes[j]->ResetData();
      Endpoint peer_endpoint;
      if (chosen_endpoint == bootstrap_endpoints[j])
        peer_endpoint = chosen_endpoint;
      EndpointPair this_endpoint_pair, peer_endpoint_pair;
      LOG(kInfo) << "Calling GetAvailableEndpoint on " << nodes[i]->kId() << " to "
                 << nodes[j]->kId() << " with peer_endpoint " << peer_endpoint;
      int result(nodes[i]->managed_connections()->GetAvailableEndpoint(peer_endpoint,
                                                                       this_endpoint_pair));
      if (result != kSuccess) {
        return testing::AssertionFailure() << "GetAvailableEndpoint failed for "
                                           << nodes[i]->kId() << " with result " << result
                                           << ".  Local: " << this_endpoint_pair.local
                                           << "  External: " << this_endpoint_pair.external;
      }
      LOG(kInfo) << "Calling GetAvailableEndpoint on " << nodes[j]->kId() << " to "
                 << nodes[i]->kId() << " with peer_endpoint " << this_endpoint_pair.external;
      result = nodes[j]->managed_connections()->GetAvailableEndpoint(this_endpoint_pair.external,
                                                                     peer_endpoint_pair);
      if (result != kSuccess) {
        return testing::AssertionFailure() << "GetAvailableEndpoint failed for "
                                           << nodes[j]->kId() << " with result " << result
                                           << ".  Local: " << peer_endpoint_pair.local
                                           << "  External: " << peer_endpoint_pair.external
                                           << "  Peer: " << this_endpoint_pair.external;
      }

      futures0 = nodes[i]->GetFutureForMessages(1);
      futures1 = nodes[j]->GetFutureForMessages(1);
      LOG(kInfo) << "Calling Add from " << nodes[i]->kId() << " on "
                  << this_endpoint_pair.external << " to " << nodes[j]->kId()
                  << " on " << peer_endpoint_pair.external;
      result = nodes[i]->managed_connections()->Add(this_endpoint_pair.external,
                                                    peer_endpoint_pair.external,
                                                    nodes[i]->kValidationData());
      nodes[i]->AddConnectedEndPoint(peer_endpoint_pair.external);
      if (result != kSuccess) {
        return testing::AssertionFailure() << "Add failed for " << nodes[i]->kId()
                                           << " with result " << result;
      }

      LOG(kInfo) << "Calling Add from " << nodes[j]->kId() << " on "
                  << peer_endpoint_pair.external << " to " << nodes[i]->kId()
                  << " on " << this_endpoint_pair.external;
      result = nodes[j]->managed_connections()->Add(peer_endpoint_pair.external,
                                                    this_endpoint_pair.external,
                                                    nodes[j]->kValidationData());
      nodes[j]->AddConnectedEndPoint(this_endpoint_pair.external);
      if (result != kSuccess) {
        return testing::AssertionFailure() << "Add failed for " << nodes[j]->kId()
                                           << " with result " << result;
      }
      if (!futures0.timed_wait(bptime::seconds(3))) {
        return testing::AssertionFailure() << "Failed waiting for " << nodes[i]->kId()
            << " to receive " << nodes[j]->kId() << "'s validation data.";
      }
      if (!futures1.timed_wait(bptime::seconds(3))) {
        return testing::AssertionFailure() << "Failed waiting for " << nodes[j]->kId()
            << " to receive " << nodes[i]->kId() << "'s validation data.";
      }
      messages0 = futures0.get();
      messages1 = futures1.get();
      if (messages0.size() != 1U) {
        return testing::AssertionFailure() << nodes[i]->kId() << " has "
            << messages0.size() << " messages [should be 1].";
      }
      if (messages1.size() != 1U) {
        return testing::AssertionFailure() << nodes[j]->kId() << " has "
            << messages1.size() << " messages [should be 1].";
      }
      if (messages0[0] != nodes[j]->kValidationData()) {
        return testing::AssertionFailure() << nodes[i]->kId() << " has received " << nodes[j]->kId()
            << "'s validation data as " << messages0[0] << " [should be \""
            << nodes[j]->kValidationData() << "\"].";
      }
      if (messages1[0] != nodes[i]->kValidationData()) {
        return testing::AssertionFailure() << nodes[j]->kId() << " has received " << nodes[i]->kId()
            << "'s validation data as " << messages1[0] << " [should be \""
            << nodes[i]->kValidationData() << "\"].";
      }
      bootstrap_endpoints.push_back(this_endpoint_pair.external);
    }
  }
  return testing::AssertionSuccess();
}


Node::Node(int id)
      : kId_("Node " + boost::lexical_cast<std::string>(id)),
        kValidationData_(kId_ + std::string("'s validation data")),
        mutex_(),
        connection_lost_endpoints_(),
        connected_endpoints_(),
        messages_(),
        managed_connections_(new ManagedConnections),
        promised_(false),
        total_message_count_expectation_(0),
        message_promise_() {}

std::vector<Endpoint> Node::connection_lost_endpoints() const {
  std::lock_guard<std::mutex> guard(mutex_);
  return connection_lost_endpoints_;
}

std::vector<std::string> Node::messages() const {
  std::lock_guard<std::mutex> guard(mutex_);
  return messages_;
}

Endpoint Node::Bootstrap(const std::vector<Endpoint> &bootstrap_endpoints,
                         Endpoint local_endpoint) {
  return managed_connections_->Bootstrap(
      bootstrap_endpoints,
      [&](const std::string &message) {
        bool is_printable(true);
        for (auto itr(message.begin()); itr != message.end(); ++itr) {
          if (*itr < 32) {
            is_printable = false;
            break;
          }
        }
        LOG(kInfo) << kId_ << " -- Received: " << (is_printable ? message.substr(0, 30) :
                                                   EncodeToHex(message.substr(0, 15)));
        std::lock_guard<std::mutex> guard(mutex_);
        messages_.emplace_back(message);
        SetPromiseIfDone();
      },
      [&](const Endpoint &endpoint) {
        LOG(kInfo) << kId_ << " -- Lost connection to " << endpoint;
        std::lock_guard<std::mutex> guard(mutex_);
        connection_lost_endpoints_.emplace_back(endpoint);
        connected_endpoints_.erase(std::remove(connected_endpoints_.begin(),
                                               connected_endpoints_.end(),
                                               endpoint),
                                   connected_endpoints_.end());
      },
      local_endpoint);
}

int Node::GetReceivedMessageCount(const std::string &message) const {
  std::lock_guard<std::mutex> guard(mutex_);
  return static_cast<int>(std::count(messages_.begin(), messages_.end(), message));
}

void Node::ResetData() {
  std::lock_guard<std::mutex> guard(mutex_);
  connection_lost_endpoints_.clear();
  messages_.clear();
  total_message_count_expectation_ = 0;
}

boost::unique_future<std::vector<std::string>> Node::GetFutureForMessages(
    const uint32_t &message_count) {
  BOOST_ASSERT(message_count > 0);
  total_message_count_expectation_ = message_count;
  promised_ = true;
  boost::promise<std::vector<std::string>> message_promise;
  message_promise_.swap(message_promise);
  return message_promise_.get_future();
}

void Node::SetPromiseIfDone() {
  if (promised_ && messages_.size() >= total_message_count_expectation_) {
    message_promise_.set_value(messages_);
    promised_ = false;
    total_message_count_expectation_ = 0;
  }
}


}  // namespace test

}  // namespace rudp

}  // namespace maidsafe