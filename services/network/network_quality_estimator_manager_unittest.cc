// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_quality_estimator_manager.h"

#include <algorithm>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "net/log/test_net_log.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator.h"
#include "services/network/public/mojom/network_quality_estimator_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class TestNetworkQualityEstimatorManagerClient
    : public mojom::NetworkQualityEstimatorManagerClient {
 public:
  explicit TestNetworkQualityEstimatorManagerClient(
      NetworkQualityEstimatorManager* network_quality_estimator_manager)
      : network_quality_estimator_manager_(network_quality_estimator_manager),
        num_network_quality_changed_(0),
        run_loop_(std::make_unique<base::RunLoop>()),
        effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
        http_rtt_(base::TimeDelta()),
        downlink_bandwidth_kbps_(INT32_MAX),
        binding_(this) {
    mojom::NetworkQualityEstimatorManagerPtr manager_ptr;
    mojom::NetworkQualityEstimatorManagerRequest request(
        mojo::MakeRequest(&manager_ptr));
    network_quality_estimator_manager_->AddRequest(std::move(request));

    mojom::NetworkQualityEstimatorManagerClientPtr client_ptr;
    mojom::NetworkQualityEstimatorManagerClientRequest client_request(
        mojo::MakeRequest(&client_ptr));
    binding_.Bind(std::move(client_request));
    manager_ptr->RequestNotifications(std::move(client_ptr));
  }

  ~TestNetworkQualityEstimatorManagerClient() override {}

  void OnNetworkQualityChanged(net::EffectiveConnectionType type,
                               base::TimeDelta http_rtt,
                               int32_t downlink_bandwidth_kbps) override {
    num_network_quality_changed_++;
    effective_connection_type_ = type;
    http_rtt_ = http_rtt;
    downlink_bandwidth_kbps_ = downlink_bandwidth_kbps;
    if (run_loop_wait_effective_connection_type_ == type)
      run_loop_->Quit();
  }

  // Returns the number of OnNetworkQualityChanged() notifications. Note that
  // the number may change based on the order in which underlying network
  // quality estimator provides notifications when effective connection
  // type, RTT and downlink estimates change simultaneously.
  size_t num_network_quality_changed() const {
    return num_network_quality_changed_;
  }

  void WaitForNotification(
      net::EffectiveConnectionType effective_connection_type) {
    run_loop_wait_effective_connection_type_ = effective_connection_type;
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop());
  }

  net::EffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }
  base::TimeDelta http_rtt() const { return http_rtt_; }
  int32_t downlink_bandwidth_kbps() const { return downlink_bandwidth_kbps_; }

 private:
  NetworkQualityEstimatorManager* network_quality_estimator_manager_;
  size_t num_network_quality_changed_;
  std::unique_ptr<base::RunLoop> run_loop_;
  net::EffectiveConnectionType run_loop_wait_effective_connection_type_;
  net::EffectiveConnectionType effective_connection_type_;
  base::TimeDelta http_rtt_;
  int32_t downlink_bandwidth_kbps_;
  mojo::Binding<mojom::NetworkQualityEstimatorManagerClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkQualityEstimatorManagerClient);
};

}  // namespace

class NetworkQualityEstimatorManagerTest : public testing::Test {
 public:
  NetworkQualityEstimatorManagerTest()
      : net_log_(std::make_unique<net::BoundTestNetLog>()),
        network_quality_estimator_manager_(
            std::make_unique<NetworkQualityEstimatorManager>(
                net_log_->bound().net_log())) {
    // Change the network quality to UNKNOWN to prevent any spurious
    // notifications.
    SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
    network_quality_estimator_manager_client_ =
        std::make_unique<TestNetworkQualityEstimatorManagerClient>(
            network_quality_estimator_manager_.get());
  }

  ~NetworkQualityEstimatorManagerTest() override {}

  TestNetworkQualityEstimatorManagerClient*
  network_quality_estimator_manager_client() {
    return network_quality_estimator_manager_client_.get();
  }

  NetworkQualityEstimatorManager* network_quality_estimator_manager() const {
    return network_quality_estimator_manager_.get();
  }

  void SimulateNetworkQualityChange(net::EffectiveConnectionType type) {
    network_quality_estimator_manager_->GetNetworkQualityEstimator()
        ->SimulateNetworkQualityChangeForTesting(type);
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<net::BoundTestNetLog> net_log_;
  std::unique_ptr<NetworkQualityEstimatorManager>
      network_quality_estimator_manager_;
  std::unique_ptr<TestNetworkQualityEstimatorManagerClient>
      network_quality_estimator_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityEstimatorManagerTest);
};

TEST_F(NetworkQualityEstimatorManagerTest, ClientNotified) {
  // Simulate a new network quality change.
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_3G);
  network_quality_estimator_manager_client()->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  EXPECT_EQ(
      net::EFFECTIVE_CONNECTION_TYPE_3G,
      network_quality_estimator_manager_client()->effective_connection_type());
  base::RunLoop().RunUntilIdle();
  // Verify that not more than 2 notifications were received.
  EXPECT_GE(2u, network_quality_estimator_manager_client()
                    ->num_network_quality_changed());
  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(450),
            network_quality_estimator_manager_client()->http_rtt());
  EXPECT_EQ(
      400,
      network_quality_estimator_manager_client()->downlink_bandwidth_kbps());
}

TEST_F(NetworkQualityEstimatorManagerTest, OneClientPipeBroken) {
  auto network_quality_estimator_manager_client2 =
      std::make_unique<TestNetworkQualityEstimatorManagerClient>(
          network_quality_estimator_manager());

  // Simulate a network quality change.
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_4G);

  network_quality_estimator_manager_client()->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  network_quality_estimator_manager_client2->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  EXPECT_EQ(
      net::EFFECTIVE_CONNECTION_TYPE_4G,
      network_quality_estimator_manager_client()->effective_connection_type());
  EXPECT_EQ(
      net::EFFECTIVE_CONNECTION_TYPE_4G,
      network_quality_estimator_manager_client2->effective_connection_type());
  // Typical RTT and downlink values when effective connection type is 4G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(175),
            network_quality_estimator_manager_client2->http_rtt());
  EXPECT_EQ(
      1600,
      network_quality_estimator_manager_client2->downlink_bandwidth_kbps());
  base::RunLoop().RunUntilIdle();

  EXPECT_GE(2u, network_quality_estimator_manager_client()
                    ->num_network_quality_changed());
  EXPECT_GE(
      2u,
      network_quality_estimator_manager_client2->num_network_quality_changed());
  network_quality_estimator_manager_client2.reset();

  base::RunLoop().RunUntilIdle();

  // Simulate a second network quality change, and the remaining client should
  // be notified.
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_2G);

  network_quality_estimator_manager_client()->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_EQ(
      net::EFFECTIVE_CONNECTION_TYPE_2G,
      network_quality_estimator_manager_client()->effective_connection_type());
  EXPECT_GE(3u, network_quality_estimator_manager_client()
                    ->num_network_quality_changed());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1800),
            network_quality_estimator_manager_client()->http_rtt());
  EXPECT_EQ(
      75,
      network_quality_estimator_manager_client()->downlink_bandwidth_kbps());
}

TEST_F(NetworkQualityEstimatorManagerTest,
       NewClientReceivesCurrentEffectiveType) {
  // Simulate a network quality change.
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_2G);

  network_quality_estimator_manager_client()->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_EQ(
      net::EFFECTIVE_CONNECTION_TYPE_2G,
      network_quality_estimator_manager_client()->effective_connection_type());
  base::RunLoop().RunUntilIdle();
  // Typical RTT and downlink values when effective connection type is 2G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1800),
            network_quality_estimator_manager_client()->http_rtt());
  EXPECT_EQ(
      75,
      network_quality_estimator_manager_client()->downlink_bandwidth_kbps());

  // Register a new client after the network quality change and it should
  // receive the up-to-date effective connection type.
  TestNetworkQualityEstimatorManagerClient
      network_quality_estimator_manager_client2(
          network_quality_estimator_manager());
  network_quality_estimator_manager_client2.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_EQ(
      net::EFFECTIVE_CONNECTION_TYPE_2G,
      network_quality_estimator_manager_client2.effective_connection_type());
  // Typical RTT and downlink values when when effective connection type is 2G.
  // Taken from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1800),
            network_quality_estimator_manager_client2.http_rtt());
  EXPECT_EQ(
      75, network_quality_estimator_manager_client2.downlink_bandwidth_kbps());
}

}  // namespace network
