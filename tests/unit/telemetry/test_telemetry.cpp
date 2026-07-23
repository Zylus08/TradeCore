#include <gtest/gtest.h>
#include "tradecore/telemetry/histogram.hpp"
#include "tradecore/telemetry/latency_tracker.hpp"
#include "tradecore/telemetry/metrics_registry.hpp"
#include <thread>

using namespace tradecore::telemetry;

TEST(TelemetryTest, HistogramBasicStats) {
    Histogram<10000> hist;
    
    EXPECT_EQ(hist.count(), 0);
    EXPECT_EQ(hist.min(), 0);
    EXPECT_EQ(hist.max(), 0);
    
    hist.record(100);
    hist.record(200);
    hist.record(300);
    
    EXPECT_EQ(hist.count(), 3);
    EXPECT_EQ(hist.min(), 100);
    EXPECT_EQ(hist.max(), 300);
    EXPECT_EQ(hist.mean(), 200);
}

TEST(TelemetryTest, HistogramPercentiles) {
    Histogram<1000> hist;
    
    // Add 100 records from 1 to 100
    for (uint64_t i = 1; i <= 100; ++i) {
        hist.record(i);
    }
    
    EXPECT_EQ(hist.count(), 100);
    EXPECT_EQ(hist.percentile(50.0), 50);
    EXPECT_EQ(hist.percentile(90.0), 90);
    EXPECT_EQ(hist.percentile(99.0), 99);
    EXPECT_EQ(hist.percentile(100.0), 100);
}

TEST(TelemetryTest, HistogramOverflow) {
    Histogram<100> hist; // small max value
    
    hist.record(50);
    hist.record(150); // Overflow
    hist.record(200); // Overflow
    
    EXPECT_EQ(hist.count(), 3);
    EXPECT_EQ(hist.overflow_count(), 2);
    EXPECT_EQ(hist.max(), 200);
    
    // Because max tracks perfectly, p100 will return true max
    EXPECT_EQ(hist.percentile(100.0), 200); 
}

TEST(TelemetryTest, LatencyTrackerRecords) {
    Histogram<10000> hist;
    
    {
        LatencyTracker<10000> tracker(hist);
        // Sleep a bit so time passes (even steady_clock is coarse, but >0ns usually)
        // Actually, just wait for clock to advance
        uint64_t start = get_time_ns();
        while (get_time_ns() == start) {}
    }
    
    EXPECT_EQ(hist.count(), 1);
    EXPECT_GT(hist.max(), 0);
}

TEST(TelemetryTest, MetricsRegistryState) {
    MetricsRegistry registry;
    
    registry.parser_latency.record(150);
    registry.messages_processed++;
    registry.risk_rejects = 5;
    
    EXPECT_EQ(registry.parser_latency.count(), 1);
    EXPECT_EQ(registry.messages_processed.load(), 1);
    EXPECT_EQ(registry.risk_rejects.load(), 5);
    
    registry.reset();
    
    EXPECT_EQ(registry.parser_latency.count(), 0);
    EXPECT_EQ(registry.messages_processed.load(), 0);
    EXPECT_EQ(registry.risk_rejects.load(), 0);
}
