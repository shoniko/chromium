// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/network_location_provider.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "services/device/geolocation/location_arbitrator.h"
#include "services/device/geolocation/wifi_data_provider.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

// Records the most recent position update and counts the number of times
// OnLocationUpdate is called.
struct LocationUpdateListener {
  LocationUpdateListener()
      : callback(base::BindRepeating(&LocationUpdateListener::OnLocationUpdate,
                                     base::Unretained(this))) {}

  void OnLocationUpdate(const LocationProvider* provider,
                        const mojom::Geoposition& position) {
    last_position = position;
    update_count++;
  }

  const LocationProvider::LocationProviderUpdateCallback callback;
  mojom::Geoposition last_position;
  int update_count = 0;
};

// A mock implementation of WifiDataProvider for testing. Adapted from
// http://gears.googlecode.com/svn/trunk/gears/geolocation/geolocation_test.cc
class MockWifiDataProvider : public WifiDataProvider {
 public:
  // Factory method for use with WifiDataProvider::SetFactoryForTesting.
  static WifiDataProvider* GetInstance() {
    CHECK(instance_);
    return instance_;
  }

  static MockWifiDataProvider* CreateInstance() {
    CHECK(!instance_);
    instance_ = new MockWifiDataProvider;
    return instance_;
  }

  MockWifiDataProvider() : start_calls_(0), stop_calls_(0), got_data_(true) {}

  // WifiDataProvider implementation.
  void StartDataProvider() override { ++start_calls_; }

  void StopDataProvider() override { ++stop_calls_; }

  bool DelayedByPolicy() override { return false; }

  bool GetData(WifiData* data_out) override {
    CHECK(data_out);
    *data_out = data_;
    return got_data_;
  }

  void SetData(const WifiData& new_data) {
    got_data_ = true;
    const bool differs = data_.DiffersSignificantly(new_data);
    data_ = new_data;
    if (differs)
      this->RunCallbacks();
  }

  void set_got_data(bool got_data) { got_data_ = got_data; }
  int start_calls_;
  int stop_calls_;

 private:
  ~MockWifiDataProvider() override {
    CHECK(this == instance_);
    instance_ = nullptr;
  }

  static MockWifiDataProvider* instance_;

  WifiData data_;
  bool got_data_;

  DISALLOW_COPY_AND_ASSIGN(MockWifiDataProvider);
};

// An implementation of LastPositionCache.
class TestLastPositionCache
    : public NetworkLocationProvider::LastPositionCache {
 public:
  void SetLastNetworkPosition(const mojom::Geoposition& position) override {
    last_network_position_ = position;
  }
  const mojom::Geoposition& GetLastNetworkPosition() override {
    return last_network_position_;
  }

 private:
  mojom::Geoposition last_network_position_;
};

MockWifiDataProvider* MockWifiDataProvider::instance_ = nullptr;

// Main test fixture
class GeolocationNetworkProviderTest : public testing::Test {
 public:
  void TearDown() override {
    WifiDataProviderManager::ResetFactoryForTesting();
  }

  LocationProvider* CreateProvider(bool set_permission_granted,
                                   const std::string& api_key = std::string()) {
    // No URLContextGetter needed: The request within the provider is tested
    // directly using TestURLFetcherFactory.
    LocationProvider* provider = new NetworkLocationProvider(
        nullptr, api_key, last_position_cache_.get());
    if (set_permission_granted)
      provider->OnPermissionGranted();
    return provider;
  }

 protected:
  GeolocationNetworkProviderTest()
      : wifi_data_provider_(MockWifiDataProvider::CreateInstance()),
        last_position_cache_(std::make_unique<TestLastPositionCache>()) {
    // TODO(joth): Really these should be in SetUp, not here, but they take no
    // effect on Mac OS Release builds if done there. I kid not. Figure out why.
    WifiDataProviderManager::SetFactoryForTesting(
        MockWifiDataProvider::GetInstance);
  }

  // Returns the current url fetcher (if any) and advances the id ready for the
  // next test step.
  net::TestURLFetcher* get_url_fetcher_and_advance_id() {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(
        NetworkLocationRequest::url_fetcher_id_for_tests);
    if (fetcher)
      ++NetworkLocationRequest::url_fetcher_id_for_tests;
    return fetcher;
  }

  static int IndexToChannel(int index) { return index + 4; }

  // Creates wifi data containing the specified number of access points, with
  // some differentiating charactistics in each.
  static WifiData CreateReferenceWifiScanData(int ap_count) {
    WifiData data;
    for (int i = 0; i < ap_count; ++i) {
      AccessPointData ap;
      ap.mac_address =
          base::ASCIIToUTF16(base::StringPrintf("%02d-34-56-78-54-32", i));
      ap.radio_signal_strength = ap_count - i;
      ap.channel = IndexToChannel(i);
      ap.signal_to_noise = i + 42;
      ap.ssid = base::ASCIIToUTF16("Some nice+network|name\\");
      data.access_point_data.insert(ap);
    }
    return data;
  }

  static void CreateReferenceWifiScanDataJson(
      int ap_count,
      int start_index,
      base::ListValue* wifi_access_point_list) {
    std::vector<std::string> wifi_data;
    for (int i = 0; i < ap_count; ++i) {
      std::unique_ptr<base::DictionaryValue> ap(new base::DictionaryValue());
      ap->SetString("macAddress", base::StringPrintf("%02d-34-56-78-54-32", i));
      ap->SetInteger("signalStrength", start_index + ap_count - i);
      ap->SetInteger("age", 0);
      ap->SetInteger("channel", IndexToChannel(i));
      ap->SetInteger("signalToNoiseRatio", i + 42);
      wifi_access_point_list->Append(std::move(ap));
    }
  }

  static mojom::Geoposition CreateReferencePosition(int id) {
    mojom::Geoposition pos;
    pos.latitude = id;
    pos.longitude = -(id + 1);
    pos.altitude = 2 * id;
    pos.accuracy = 3 * id;
    pos.timestamp = base::Time::Now();
    return pos;
  }

  static std::string PrettyJson(const base::Value& value) {
    std::string pretty;
    base::JSONWriter::WriteWithOptions(
        value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &pretty);
    return pretty;
  }

  static testing::AssertionResult JsonGetList(
      const std::string& field,
      const base::DictionaryValue& dict,
      const base::ListValue** output_list) {
    if (!dict.GetList(field, output_list))
      return testing::AssertionFailure() << "Dictionary " << PrettyJson(dict)
                                         << " is missing list field " << field;
    return testing::AssertionSuccess();
  }

  static testing::AssertionResult JsonFieldEquals(
      const std::string& field,
      const base::DictionaryValue& expected,
      const base::DictionaryValue& actual) {
    const base::Value* expected_value;
    const base::Value* actual_value;
    if (!expected.Get(field, &expected_value))
      return testing::AssertionFailure()
             << "Expected dictionary " << PrettyJson(expected)
             << " is missing field " << field;
    if (!expected.Get(field, &actual_value))
      return testing::AssertionFailure()
             << "Actual dictionary " << PrettyJson(actual)
             << " is missing field " << field;
    if (!expected_value->Equals(actual_value))
      return testing::AssertionFailure()
             << "Field " << field
             << " mismatch: " << PrettyJson(*expected_value)
             << " != " << PrettyJson(*actual_value);
    return testing::AssertionSuccess();
  }

  // Checks that |request| contains valid JSON upload data. The Wifi access
  // points specified in the JSON are validated against the first
  // |expected_wifi_aps| access points, starting from position
  // |wifi_start_index|, that are generated by CreateReferenceWifiScanDataJson.
  void CheckRequestIsValid(const net::TestURLFetcher& request,
                           int expected_wifi_aps,
                           int wifi_start_index) {
    const std::string& upload_data = request.upload_data();
    ASSERT_FALSE(upload_data.empty());
    std::string json_parse_error_msg;
    std::unique_ptr<base::Value> parsed_json =
        base::JSONReader::ReadAndReturnError(upload_data, base::JSON_PARSE_RFC,
                                             nullptr, &json_parse_error_msg);
    EXPECT_TRUE(json_parse_error_msg.empty());
    ASSERT_TRUE(parsed_json);

    const base::DictionaryValue* request_json;
    ASSERT_TRUE(parsed_json->GetAsDictionary(&request_json));

    if (expected_wifi_aps) {
      base::ListValue expected_wifi_aps_json;
      CreateReferenceWifiScanDataJson(expected_wifi_aps, wifi_start_index,
                                      &expected_wifi_aps_json);
      EXPECT_EQ(size_t(expected_wifi_aps), expected_wifi_aps_json.GetSize());

      const base::ListValue* wifi_aps_json;
      ASSERT_TRUE(
          JsonGetList("wifiAccessPoints", *request_json, &wifi_aps_json));
      for (size_t i = 0; i < expected_wifi_aps_json.GetSize(); ++i) {
        const base::DictionaryValue* expected_json;
        ASSERT_TRUE(expected_wifi_aps_json.GetDictionary(i, &expected_json));
        const base::DictionaryValue* actual_json;
        ASSERT_TRUE(wifi_aps_json->GetDictionary(i, &actual_json));
        ASSERT_TRUE(
            JsonFieldEquals("macAddress", *expected_json, *actual_json));
        ASSERT_TRUE(
            JsonFieldEquals("signalStrength", *expected_json, *actual_json));
        ASSERT_TRUE(JsonFieldEquals("channel", *expected_json, *actual_json));
        ASSERT_TRUE(JsonFieldEquals("signalToNoiseRatio", *expected_json,
                                    *actual_json));
      }
    } else {
      ASSERT_FALSE(request_json->HasKey("wifiAccessPoints"));
    }
  }

  const base::MessageLoop main_message_loop_;
  const net::TestURLFetcherFactory url_fetcher_factory_;
  const scoped_refptr<MockWifiDataProvider> wifi_data_provider_;
  std::unique_ptr<NetworkLocationProvider::LastPositionCache>
      last_position_cache_;
};

// Tests that fixture members were SetUp correctly.
TEST_F(GeolocationNetworkProviderTest, CreateDestroy) {
  EXPECT_EQ(&main_message_loop_, base::MessageLoop::current());
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  EXPECT_TRUE(provider);
  provider.reset();
  SUCCEED();
}

// Tests that, with an empty api_key, no query string parameter is included in
// the request.
TEST_F(GeolocationNetworkProviderTest, EmptyApiKey) {
  const std::string api_key = "";
  std::unique_ptr<LocationProvider> provider(CreateProvider(true, api_key));
  provider->StartProvider(false);
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher);
  EXPECT_FALSE(fetcher->GetOriginalURL().has_query());
}

// Tests that, with non-empty api_key, a "key" query string parameter is
// included in the request.
TEST_F(GeolocationNetworkProviderTest, NonEmptyApiKey) {
  const std::string api_key = "something";
  std::unique_ptr<LocationProvider> provider(CreateProvider(true, api_key));
  provider->StartProvider(false);
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher);
  EXPECT_TRUE(fetcher->GetOriginalURL().has_query());
  EXPECT_TRUE(fetcher->GetOriginalURL().query_piece().starts_with("key="));
}

// Tests that, after StartProvider(), a TestURLFetcher can be extracted,
// representing a valid request.
TEST_F(GeolocationNetworkProviderTest, StartProvider) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher);
  CheckRequestIsValid(*fetcher, 0, 0);
}

// Tests that, with a very large number of access points, the set of access
// points represented in the request is truncated to fit within 2048 characters.
TEST_F(GeolocationNetworkProviderTest, StartProviderLongRequest) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);
  // Create Wifi scan data with too many access points.
  const int kFirstScanAps = 20;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  base::RunLoop().RunUntilIdle();
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher);
  // The request url should have been shortened to less than 2048 characters
  // in length by not including access points with the lowest signal strength
  // in the request.
  EXPECT_LT(fetcher->GetOriginalURL().spec().size(), size_t(2048));
  // Expect only 16 out of 20 original access points.
  CheckRequestIsValid(*fetcher, 16, 4);
}

// Tests that the provider issues the right requests, and provides the right
// GetPosition() results based on the responses, for the following complex
// sequence of Wifi data situations:
// 1. Initial "no fix" response -> provide 'invalid' position.
// 2. Wifi data arrives -> make new request.
// 3. Response has good fix -> provide corresponding position.
// 4. Wifi data changes slightly -> no new request.
// 5. Wifi data changes a lot -> new request.
// 6. Response is error -> provide 'invalid' position.
// 7. Wifi data changes back to (2.) -> no new request, provide cached position.
TEST_F(GeolocationNetworkProviderTest, MultipleWifiScansComplete) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);

  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher);

  // 1. Complete the network request with bad position fix.
  const char* kNoFixNetworkResponse =
      "{"
      "  \"status\": \"ZERO_RESULTS\""
      "}";
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);  // OK
  fetcher->SetResponseString(kNoFixNetworkResponse);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  mojom::Geoposition position = provider->GetPosition();
  EXPECT_FALSE(ValidateGeoposition(position));

  // 2. Now wifi data arrives -- SetData will notify listeners.
  const int kFirstScanAps = 6;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  base::RunLoop().RunUntilIdle();
  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher);
  // The request should have the wifi data.
  CheckRequestIsValid(*fetcher, kFirstScanAps, 0);

  // 3. Send a reply with good position fix.
  const char* kReferenceNetworkResponse =
      "{"
      "  \"accuracy\": 1200.4,"
      "  \"location\": {"
      "    \"lat\": 51.0,"
      "    \"lng\": -0.1"
      "  }"
      "}";
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);  // OK
  fetcher->SetResponseString(kReferenceNetworkResponse);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  position = provider->GetPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_FALSE(position.timestamp.is_null());
  EXPECT_TRUE(ValidateGeoposition(position));

  // 4. Wifi updated again, with one less AP. This is 'close enough' to the
  // previous scan, so no new request made.
  const int kSecondScanAps = kFirstScanAps - 1;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kSecondScanAps));
  base::RunLoop().RunUntilIdle();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);

  position = provider->GetPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_TRUE(ValidateGeoposition(position));

  // 5. Now a third scan with more than twice the original APs -> new request.
  const int kThirdScanAps = kFirstScanAps * 2 + 1;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kThirdScanAps));
  base::RunLoop().RunUntilIdle();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);
  CheckRequestIsValid(*fetcher, kThirdScanAps, 0);

  // 6. ...reply with a network error.
  fetcher->set_status(net::URLRequestStatus::FromError(net::ERR_FAILED));
  fetcher->set_response_code(200);  // should be ignored
  fetcher->SetResponseString(std::string());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Error means we now no longer have a fix.
  position = provider->GetPosition();
  EXPECT_FALSE(ValidateGeoposition(position));

  // 7. Wifi scan returns to original set: should be serviced from cache.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(get_url_fetcher_and_advance_id());  // No new request created.

  position = provider->GetPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_TRUE(ValidateGeoposition(position));
}

// Tests that, if no Wifi scan data is available at startup, the provider
// doesn't initiate a request, until Wifi data later becomes available.
TEST_F(GeolocationNetworkProviderTest, NoRequestOnStartupUntilWifiData) {
  LocationUpdateListener listener;
  wifi_data_provider_->set_got_data(false);  // No initial Wifi data.
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);

  provider->SetUpdateCallback(listener.callback);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(get_url_fetcher_and_advance_id())
      << "Network request should not be created right away on startup when "
         "wifi data has not yet arrived";

  // Now Wifi data becomes available.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(get_url_fetcher_and_advance_id());
}

// Tests that, even if a request is already in flight, new wifi data results in
// a new request being sent.
TEST_F(GeolocationNetworkProviderTest, NewDataReplacesExistingNetworkRequest) {
  // Send initial request with empty data
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);

  // Now wifi data arrives; new request should be sent.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(4));
  base::RunLoop().RunUntilIdle();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);
}

// Tests that, if user geolocation permission hasn't been granted during
// startup, the provider doesn't initiate a request until it is notified of the
// user granting permission.
TEST_F(GeolocationNetworkProviderTest, NetworkRequestDeferredForPermission) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));
  provider->StartProvider(false);
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);
  provider->OnPermissionGranted();

  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher);
}

// Tests that, even if new Wifi data arrives, the provider doesn't initiate its
// first request unless & until the user grants permission.
TEST_F(GeolocationNetworkProviderTest,
       NetworkRequestWithWifiDataDeferredForPermission) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));
  provider->StartProvider(false);
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);

  static const int kScanCount = 4;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kScanCount));
  base::RunLoop().RunUntilIdle();

  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);

  provider->OnPermissionGranted();

  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher);

  CheckRequestIsValid(*fetcher, kScanCount, 0);
}

// Tests that the provider's position cache correctly caches each item, and
// begins evicting the oldest entries in order once it reaches its maximum size.
TEST_F(GeolocationNetworkProviderTest, NetworkPositionCache) {
  NetworkLocationProvider::PositionCache cache;

  const int kCacheSize = NetworkLocationProvider::PositionCache::kMaximumSize;
  for (int i = 1; i < kCacheSize * 2 + 1; ++i) {
    mojom::Geoposition pos = CreateReferencePosition(i);
    bool ret = cache.CachePosition(CreateReferenceWifiScanData(i), pos);
    EXPECT_TRUE(ret) << i;
    const mojom::Geoposition* item =
        cache.FindPosition(CreateReferenceWifiScanData(i));
    ASSERT_TRUE(item) << i;
    EXPECT_EQ(pos.latitude, item->latitude) << i;
    EXPECT_EQ(pos.longitude, item->longitude) << i;
    if (i <= kCacheSize) {
      // Nothing should have spilled yet; check oldest item is still there.
      EXPECT_TRUE(cache.FindPosition(CreateReferenceWifiScanData(1)));
    } else {
      const int evicted = i - kCacheSize;
      EXPECT_FALSE(cache.FindPosition(CreateReferenceWifiScanData(evicted)));
      EXPECT_TRUE(cache.FindPosition(CreateReferenceWifiScanData(evicted + 1)));
    }
  }
}

// Tests that the provider's last position cache delegate is correctly used to
// cache the most recent network position estimate, and that this estimate is
// not lost when the provider is torn down and recreated.
TEST_F(GeolocationNetworkProviderTest, LastPositionCache) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);

  // Check that the provider is initialized with an invalid position.
  mojom::Geoposition position = provider->GetPosition();
  EXPECT_FALSE(ValidateGeoposition(position));

  // Check that the cached value is also invalid.
  position = last_position_cache_->GetLastNetworkPosition();
  EXPECT_FALSE(ValidateGeoposition(position));

  // Now wifi data arrives -- SetData will notify listeners.
  const int kFirstScanAps = 6;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  base::RunLoop().RunUntilIdle();
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher);
  // The request should have the wifi data.
  CheckRequestIsValid(*fetcher, kFirstScanAps, 0);

  // Send a reply with good position fix.
  const char* kReferenceNetworkResponse =
      "{"
      "  \"accuracy\": 1200.4,"
      "  \"location\": {"
      "    \"lat\": 51.0,"
      "    \"lng\": -0.1"
      "  }"
      "}";
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);  // OK
  fetcher->SetResponseString(kReferenceNetworkResponse);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // The provider should return the position as the current best estimate.
  position = provider->GetPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_FALSE(position.timestamp.is_null());
  EXPECT_TRUE(ValidateGeoposition(position));

  // Shut down the provider. This typically happens whenever there are no active
  // Geolocation API calls.
  provider->StopProvider();
  provider = nullptr;

  // The cache preserves the last estimate while the provider is inactive.
  position = last_position_cache_->GetLastNetworkPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_FALSE(position.timestamp.is_null());
  EXPECT_TRUE(ValidateGeoposition(position));

  // Restart the provider.
  provider.reset(CreateProvider(true));
  provider->StartProvider(false);

  // Check that the most recent position estimate is retained.
  position = provider->GetPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_FALSE(position.timestamp.is_null());
  EXPECT_TRUE(ValidateGeoposition(position));
}

// Tests that when the last network position estimate is sufficiently recent and
// we do not expect to receive a fresh estimate soon (no new wifi data available
// and no pending geolocation service request) then the provider may return the
// last position instead of waiting to acquire a fresh estimate.
TEST_F(GeolocationNetworkProviderTest, LastPositionCacheUsed) {
  LocationUpdateListener listener;

  // Seed the last position cache with a valid geoposition value and the
  // timestamp set to the current time.
  mojom::Geoposition last_position = CreateReferencePosition(0);
  EXPECT_TRUE(ValidateGeoposition(last_position));
  last_position_cache_->SetLastNetworkPosition(last_position);

  // Simulate no initial wifi data.
  wifi_data_provider_->set_got_data(false);

  // Start the provider without geolocation permission.
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));
  provider->StartProvider(false);

  // Register a location update callback. The listener will count how many times
  // OnLocationUpdate is called.
  provider->SetUpdateCallback(listener.callback);

  // Under normal circumstances, when there is no initial wifi data
  // RequestPosition is not called until a few seconds after the provider is
  // started to allow time for the wifi scan to complete. To avoid waiting,
  // grant permissions once the provider is running to cause RequestPosition to
  // be called immediately.
  provider->OnPermissionGranted();

  base::RunLoop().RunUntilIdle();

  // Check that the listener received the position update and that the position
  // is the same as the seeded value except for the timestamp, which should be
  // newer.
  EXPECT_EQ(1, listener.update_count);
  EXPECT_TRUE(ValidateGeoposition(listener.last_position));
  EXPECT_EQ(last_position.latitude, listener.last_position.latitude);
  EXPECT_EQ(last_position.longitude, listener.last_position.longitude);
  EXPECT_EQ(last_position.accuracy, listener.last_position.accuracy);
  EXPECT_LT(last_position.timestamp, listener.last_position.timestamp);
}

// Tests that the last network position estimate is not returned if the
// estimate is too old.
TEST_F(GeolocationNetworkProviderTest, LastPositionNotUsedTooOld) {
  LocationUpdateListener listener;

  // Seed the last position cache with a geoposition value with the timestamp
  // set to 20 minutes ago.
  mojom::Geoposition last_position = CreateReferencePosition(0);
  last_position.timestamp =
      base::Time::Now() - base::TimeDelta::FromMinutes(20);
  EXPECT_TRUE(ValidateGeoposition(last_position));
  last_position_cache_->SetLastNetworkPosition(last_position);

  // Simulate no initial wifi data.
  wifi_data_provider_->set_got_data(false);

  // Start the provider without geolocation permission.
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));
  provider->StartProvider(false);

  // Register a location update callback. The listener will count how many times
  // OnLocationUpdate is called.
  provider->SetUpdateCallback(listener.callback);

  // Under normal circumstances, when there is no initial wifi data
  // RequestPosition is not called until a few seconds after the provider is
  // started to allow time for the wifi scan to complete. To avoid waiting,
  // grant permissions once the provider is running to cause RequestPosition to
  // be called immediately.
  provider->OnPermissionGranted();

  base::RunLoop().RunUntilIdle();

  // Check that the listener received no updates.
  EXPECT_EQ(0, listener.update_count);
  EXPECT_FALSE(ValidateGeoposition(listener.last_position));
}

// Tests that the last network position estimate is not returned if there is
// new wifi data or a pending geolocation service request.
TEST_F(GeolocationNetworkProviderTest, LastPositionNotUsedNewData) {
  LocationUpdateListener listener;

  // Seed the last position cache with a valid geoposition value. The timestamp
  // of the cached position is set to the current time.
  mojom::Geoposition last_position = CreateReferencePosition(0);
  EXPECT_TRUE(ValidateGeoposition(last_position));
  last_position_cache_->SetLastNetworkPosition(last_position);

  // Simulate a completed wifi scan.
  const int kFirstScanAps = 6;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));

  // Create the provider without permissions enabled.
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));

  // Register a location update callback. The callback will count how many times
  // OnLocationUpdate is called.
  provider->SetUpdateCallback(listener.callback);

  // Start the provider.
  provider->StartProvider(false);
  base::RunLoop().RunUntilIdle();

  // The listener should not receive any updates. There is a valid cached value
  // but it should not be sent while we have pending wifi data.
  EXPECT_EQ(0, listener.update_count);
  EXPECT_FALSE(ValidateGeoposition(listener.last_position));

  // Check that there is no pending network request.
  EXPECT_FALSE(get_url_fetcher_and_advance_id());

  // Simulate no new wifi data.
  wifi_data_provider_->set_got_data(false);

  // Grant permission to allow the network request to proceed.
  provider->OnPermissionGranted();
  base::RunLoop().RunUntilIdle();

  // The listener should still not receive any updates. There is a valid cached
  // value and no new wifi data, but the cached value should not be sent while
  // we have a pending request to the geolocation service.
  EXPECT_EQ(0, listener.update_count);
  EXPECT_FALSE(ValidateGeoposition(listener.last_position));

  // Check that a network request is pending.
  EXPECT_TRUE(get_url_fetcher_and_advance_id());
}

}  // namespace device
