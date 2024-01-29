/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PrometheusReporter.h"
#include <gtest/gtest.h>

namespace facebook::presto::prometheus {
class PrometheusReporterTest : public testing::Test {
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(PrometheusReporterTest, testAllMetrics) {
  ::prometheus::Labels testLabels(
      {{"cluster", "test_cluster"}, {"worker", "test_worker_pod"}});
  std::string labelsSerialized =
      ("cluster=\"test_cluster\",worker=\"test_worker_pod\"");
  auto reporter = std::make_shared<PrometheusReporter>(testLabels);
  reporter->registerMetricExportType(
      "test.key1", facebook::velox::StatType::COUNT);
  reporter->registerMetricExportType(
      "test.key2", facebook::velox::StatType::AVG);
  reporter->registerMetricExportType(
      "test.key3", facebook::velox::StatType::SUM);
  reporter->registerMetricExportType(
      "test.key4", facebook::velox::StatType::RATE);
  EXPECT_EQ(
      facebook::velox::StatType::COUNT,
      reporter->registeredMetrics_.find("test.key1")->second);
  EXPECT_EQ(
      facebook::velox::StatType::AVG,
      reporter->registeredMetrics_.find("test.key2")->second);
  EXPECT_EQ(
      facebook::velox::StatType::SUM,
      reporter->registeredMetrics_.find("test.key3")->second);
  EXPECT_EQ(
      facebook::velox::StatType::RATE,
      reporter->registeredMetrics_.find("test.key4")->second);

  std::vector<size_t> testData = {10, 11, 15};
  for (auto i : testData) {
    reporter->addMetricValue("test.key1", i);
    reporter->addMetricValue("test.key2", i + 1000);
  }
  // Uses default value of 1 for second parameter.
  reporter->addMetricValue("test.key1");
  prometheus::PrometheusSerializer serializer(prometheus::Labels{
      {"cluster", "test_cluster"}, {"worker", "test_worker_pod"}});
  auto fullSerializedResult = reporter->getSerializedMetrics();
  std::vector<std::string> expected = {
      "# TYPE test_key1 counter",
      "test_key1{" + labelsSerialized + "} 37",
      "# TYPE test_key2 gauge",
      "test_key2{" + labelsSerialized + "} 1015",
      "# TYPE test_key3 gauge",
      "test_key3{" + labelsSerialized + "} 0",
      "# TYPE test_key4 gauge",
      "test_key4{" + labelsSerialized + "} 0"};

  auto verifySerializedResult = [&](std::string& fullSerializedResult,
                                    std::vector<std::string>& expected) {
    auto i = 0;
    auto pos = fullSerializedResult.find("\n");
    while (pos != std::string::npos) {
      std::string line = fullSerializedResult.substr(0, pos);
      EXPECT_EQ(line, expected[i++]);
      fullSerializedResult = fullSerializedResult.substr(pos + 1);
      pos = fullSerializedResult.find("\n");
    }
  };
  verifySerializedResult(fullSerializedResult, expected);
  std::string histSummaryKey = "test.histogram.key1";
  std::string histogramKey = "test.histogram.key2";
  // Register Histograms and Summaries.
  reporter->registerHistogramMetricExportType(
      histSummaryKey, 10, 0, 100, {50, 99, 100});
  // Only histogram.
  reporter->registerHistogramMetricExportType(histogramKey, 10, 0, 100, {});
  int recordCount = 100;
  int sum = 0;
  for (int i = 0; i < recordCount; ++i) {
    if (i < 20) {
      reporter->addHistogramMetricValue(histSummaryKey, 20);
      sum += 20;
    } else if (i >= 20 && i < 50) {
      reporter->addHistogramMetricValue(histSummaryKey, 50);
      sum += 50;
    } else {
      reporter->addHistogramMetricValue(histSummaryKey, 85);
      sum += 85;
    }
  }
  reporter->addHistogramMetricValue(histogramKey, 10);
  fullSerializedResult = reporter->getSerializedMetrics();
  std::replace(histSummaryKey.begin(), histSummaryKey.end(), '.', '_');
  std::replace(histogramKey.begin(), histogramKey.end(), '.', '_');
  std::vector<std::string> histogramMetricsFormatted = {
      "# TYPE " + histSummaryKey + " histogram",
      histSummaryKey + "_count{" + labelsSerialized + "} " +
          std::to_string(recordCount),
      histSummaryKey + "_sum{" + labelsSerialized + "} " + std::to_string(sum),
      histSummaryKey + "_bucket{" + labelsSerialized + ",le=\"10\"} 0",
      histSummaryKey + "_bucket{" + labelsSerialized + ",le=\"20\"} 20",
      histSummaryKey + "_bucket{" + labelsSerialized + ",le=\"30\"} 20",
      histSummaryKey + "_bucket{" + labelsSerialized + ",le=\"40\"} 20",
      histSummaryKey + "_bucket{" + labelsSerialized + ",le=\"50\"} 50",
      histSummaryKey + "_bucket{" + labelsSerialized + ",le=\"60\"} 50",
      histSummaryKey + "_bucket{" + labelsSerialized + ",le=\"70\"} 50",
      histSummaryKey + "_bucket{" + labelsSerialized + ",le=\"80\"} 50",
      histSummaryKey + "_bucket{" + labelsSerialized + ",le=\"90\"} 100",
      histSummaryKey + "_bucket{" + labelsSerialized + ",le=\"100\"} 100",
      histSummaryKey + "_bucket{" + labelsSerialized + ",le=\"+Inf\"} 100",
      "# TYPE test_histogram_key2 histogram",
      histogramKey + "_count{" + labelsSerialized + "} 1",
      histogramKey + "_sum{" + labelsSerialized + "} 10",
      histogramKey + "_bucket{" + labelsSerialized + ",le=\"10\"} 1",
      histogramKey + "_bucket{" + labelsSerialized + ",le=\"20\"} 1",
      histogramKey + "_bucket{" + labelsSerialized + ",le=\"30\"} 1",
      histogramKey + "_bucket{" + labelsSerialized + ",le=\"40\"} 1",
      histogramKey + "_bucket{" + labelsSerialized + ",le=\"50\"} 1",
      histogramKey + "_bucket{" + labelsSerialized + ",le=\"60\"} 1",
      histogramKey + "_bucket{" + labelsSerialized + ",le=\"70\"} 1",
      histogramKey + "_bucket{" + labelsSerialized + ",le=\"80\"} 1",
      histogramKey + "_bucket{" + labelsSerialized + ",le=\"90\"} 1",
      histogramKey + "_bucket{" + labelsSerialized + ",le=\"100\"} 1",
      histogramKey + "_bucket{" + labelsSerialized + ",le=\"+Inf\"} 1",
      "# TYPE test_histogram_key1_summary summary",
      histSummaryKey + "_summary_count{" + labelsSerialized + "} " +
          std::to_string(recordCount),
      histSummaryKey + "_summary_sum{" + labelsSerialized + "} " +
          std::to_string(sum),
      histSummaryKey + "_summary{" + labelsSerialized + ",quantile=\"0.5\"} 50",
      histSummaryKey + "_summary{" + labelsSerialized +
          ",quantile=\"0.99\"} 85",
      histSummaryKey + "_summary{" + labelsSerialized + ",quantile=\"1\"} 85"};
  expected.insert(
      expected.end(),
      histogramMetricsFormatted.begin(),
      histogramMetricsFormatted.end());
  verifySerializedResult(fullSerializedResult, expected);
}
}; // namespace facebook::presto::prometheus
