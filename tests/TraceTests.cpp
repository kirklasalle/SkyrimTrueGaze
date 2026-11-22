#include "TestSupport.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Engine/TraceLogger.hpp"

using namespace TrueGaze::Engine;

static void CleanTestFile(const std::filesystem::path& p)
{
    std::error_code ec;
    if (std::filesystem::exists(p, ec))
    {
        std::filesystem::remove(p, ec);
    }
}

static std::vector<std::string> ReadLines(const std::filesystem::path& p)
{
    std::vector<std::string> lines;
    std::ifstream file(p);
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty())
        {
            lines.push_back(line);
        }
    }
    return lines;
}

void TestTraceLoggerLifecycle()
{
    const std::filesystem::path testFile = "test_trace_lifecycle.jsonl";
    CleanTestFile(testFile);

    auto& logger = TraceLogger::Get();
    assert(!logger.IsActive());

    logger.StartTracing(testFile);
    assert(logger.IsActive());
    assert(logger.TotalEvents() == 0);

    logger.StopTracing();
    assert(!logger.IsActive());

    assert(std::filesystem::exists(testFile));
    CleanTestFile(testFile);
    std::cout << "[PASS] TestTraceLoggerLifecycle\n";
}

void TestTraceLoggerGazeTickSchema()
{
    const std::filesystem::path testFile = "test_trace_gaze_tick.jsonl";
    CleanTestFile(testFile);

    auto& logger = TraceLogger::Get();
    logger.StartTracing(testFile);

    logger.LogGazeTick(100.5f, 0x000A2C94, 0x00000014, 3.42f, -1.07f, 1.22f, -0.53f,
                       1, 1, "LOGIC", 0, true, 1.43f, 0.62f, 0.0167f);
    logger.Flush();
    logger.StopTracing();

    auto lines = ReadLines(testFile);
    assert(lines.size() == 1);
    const auto& line = lines[0];
    (void)line;

    // Verify key substrings match schema exactly
    assert(line.find("\"t\":\"GAZE_TICK\"") != std::string::npos);
    assert(line.find("\"ts\":100.500") != std::string::npos);
    assert(line.find("\"actor\":\"0x000A2C94\"") != std::string::npos);
    assert(line.find("\"target\":\"0x00000014\"") != std::string::npos);
    assert(line.find("\"yaw\":3.42") != std::string::npos);
    assert(line.find("\"pitch\":-1.07") != std::string::npos);
    assert(line.find("\"eyeYaw\":1.22") != std::string::npos);
    assert(line.find("\"eyePitch\":-0.53") != std::string::npos);
    assert(line.find("\"region\":1") != std::string::npos);
    assert(line.find("\"hitRegion\":1") != std::string::npos);
    assert(line.find("\"mode\":\"LOGIC\"") != std::string::npos);
    assert(line.find("\"lod\":0") != std::string::npos);
    assert(line.find("\"mutual\":true") != std::string::npos);
    assert(line.find("\"mutualSec\":1.43") != std::string::npos);
    assert(line.find("\"headPct\":0.62") != std::string::npos);
    assert(line.find("\"dt\":0.0167") != std::string::npos);

    CleanTestFile(testFile);
    std::cout << "[PASS] TestTraceLoggerGazeTickSchema\n";
}

void TestTraceLoggerSaccadeAndRegionEvents()
{
    const std::filesystem::path testFile = "test_trace_saccades.jsonl";
    CleanTestFile(testFile);

    auto& logger = TraceLogger::Get();
    logger.StartTracing(testFile);

    logger.LogSaccadeOnset(101.0f, 0x000A2C94, 0, 2, 6.12f, 0.032f);
    logger.LogSaccadeComplete(101.035f, 0x000A2C94, 2, 0.035f, 0.3f);
    logger.LogRegionChange(101.040f, 0x000A2C94, 0, 2, 0.87f, "LeftEye", "Mouth");
    logger.Flush();
    logger.StopTracing();

    auto lines = ReadLines(testFile);
    assert(lines.size() == 3);

    // 1. SACCADE_ONSET
    assert(lines[0].find("\"t\":\"SACCADE_ONSET\"") != std::string::npos);
    assert(lines[0].find("\"fromRegion\":0") != std::string::npos);
    assert(lines[0].find("\"toVertex\":2") != std::string::npos);
    assert(lines[0].find("\"angularDistance\":6.12") != std::string::npos);
    assert(lines[0].find("\"expectedDuration\":0.032") != std::string::npos);

    // 2. SACCADE_COMPLETE
    assert(lines[1].find("\"t\":\"SACCADE_COMPLETE\"") != std::string::npos);
    assert(lines[1].find("\"landedRegion\":2") != std::string::npos);
    assert(lines[1].find("\"actualDuration\":0.035") != std::string::npos);
    assert(lines[1].find("\"overshootDeg\":0.30") != std::string::npos);

    // 3. REGION_CHANGE
    assert(lines[2].find("\"t\":\"REGION_CHANGE\"") != std::string::npos);
    assert(lines[2].find("\"from\":0") != std::string::npos);
    assert(lines[2].find("\"to\":2") != std::string::npos);
    assert(lines[2].find("\"dwellSec\":0.87") != std::string::npos);
    assert(lines[2].find("\"fromLabel\":\"LeftEye\"") != std::string::npos);
    assert(lines[2].find("\"toLabel\":\"Mouth\"") != std::string::npos);

    CleanTestFile(testFile);
    std::cout << "[PASS] TestTraceLoggerSaccadeAndRegionEvents\n";
}

void TestTraceLoggerCgaBlinkAndMismatchEvents()
{
    const std::filesystem::path testFile = "test_trace_diagnostics.jsonl";
    CleanTestFile(testFile);

    auto& logger = TraceLogger::Get();
    // Blink tracing is OFF by default (TraceLogger::_traceBlinks{false}) to keep
    // production traces lean; this test exercises the blink schema, so opt in
    // explicitly. (Audit 2026-10-15 F-02: previously masked by NDEBUG.)
    logger.SetConfig(/*gazeTick*/ true, /*saccades*/ true, /*regionChanges*/ true,
                     /*cga*/ true, /*blinks*/ true, /*hitMismatches*/ true,
                     /*bufferSize*/ 256, /*maxFileSizeMb*/ 100);
    logger.StartTracing(testFile);

    logger.LogCgaEnter(102.5f, 0x000A2C94, "MUTUAL_THRESHOLD", 9, "ULPeripheral");
    logger.LogCgaExit(105.1f, 0x000A2C94, 0, 2.60f, "DIALOGUE_SYNC");
    logger.LogBlink(106.0f, 0x000A2C94, 180.0f, 3.2f);
    logger.LogHitMismatch(107.0f, 0x000A2C94, 3, 0, 0.5f, 4.2f, 0.43f, 3.60f, "BOUNDARY");
    logger.LogCalibration(108.0f, "sweep", 9, -15.0f, 18.0f, 9, 9, true);
    logger.LogExpandedPlayEvent(109.25f, "TX_COMMIT", 0x0001347Bu, "Camilla", 0u, 2.20f,
                                "type=RapportBarter vec=Admire aff=Love speech=+5.0");
    logger.LogVisualEyeSync(109.50f, 0x0002C90Fu, 0x00000014u, 4u, -12.0f, 1.5f,
                            -28.0f, 0.5f, 0.95f, 0.0f, 0.05f, 0.0f,
                            -30.4f, 1.2f, -15.0f, -11.2f, 92.5f, true);

    logger.Flush();
    logger.StopTracing();

    auto lines = ReadLines(testFile);
    assert(lines.size() == 7);

    // 1. CGA_ENTER
    assert(lines[0].find("\"t\":\"CGA_ENTER\"") != std::string::npos);
    assert(lines[0].find("\"trigger\":\"MUTUAL_THRESHOLD\"") != std::string::npos);
    assert(lines[0].find("\"quadrant\":9") != std::string::npos);
    assert(lines[0].find("\"quadrantLabel\":\"ULPeripheral\"") != std::string::npos);

    // 2. CGA_EXIT
    assert(lines[1].find("\"t\":\"CGA_EXIT\"") != std::string::npos);
    assert(lines[1].find("\"returnRegion\":0") != std::string::npos);
    assert(lines[1].find("\"aversionDurationSec\":2.60") != std::string::npos);
    assert(lines[1].find("\"trigger\":\"DIALOGUE_SYNC\"") != std::string::npos);

    // 3. BLINK
    assert(lines[2].find("\"t\":\"BLINK\"") != std::string::npos);
    assert(lines[2].find("\"durationMs\":180") != std::string::npos);
    assert(lines[2].find("\"intervalSec\":3.20") != std::string::npos);

    // 4. HIT_MISMATCH
    assert(lines[3].find("\"t\":\"HIT_MISMATCH\"") != std::string::npos);
    assert(lines[3].find("\"classified\":3") != std::string::npos);
    assert(lines[3].find("\"hit\":0") != std::string::npos);
    assert(lines[3].find("\"hitX\":0.43") != std::string::npos);
    assert(lines[3].find("\"hitZ\":3.60") != std::string::npos);
    assert(lines[3].find("\"reason\":\"BOUNDARY\"") != std::string::npos);

    // 5. CALIBRATION
    assert(lines[4].find("\"t\":\"CALIBRATION\"") != std::string::npos);
    assert(lines[4].find("\"command\":\"sweep\"") != std::string::npos);
    assert(lines[4].find("\"pass\":true") != std::string::npos);

    // 6. EXPANDED_PLAY
    assert(lines[5].find("\"t\":\"EXPANDED_PLAY\"") != std::string::npos);
    assert(lines[5].find("\"action\":\"TX_COMMIT\"") != std::string::npos);
    assert(lines[5].find("\"actor\":\"0x0001347B\"") != std::string::npos);
    assert(lines[5].find("\"name\":\"Camilla\"") != std::string::npos);
    assert(lines[5].find("RapportBarter") != std::string::npos);

    // 7. VISUAL_EYE_SYNC
    assert(lines[6].find("\"t\":\"VISUAL_EYE_SYNC\"") != std::string::npos);
    assert(lines[6].find("\"actor\":\"0x0002C90F\"") != std::string::npos);
    assert(lines[6].find("\"arrowYaw\":-30.40") != std::string::npos);
    assert(lines[6].find("\"seated\":true") != std::string::npos);

    CleanTestFile(testFile);
    std::cout << "[PASS] TestTraceLoggerCgaBlinkAndMismatchEvents\n";
}

void TestTraceLoggerActorFiltering()
{
    const std::filesystem::path testFile = "test_trace_filter.jsonl";
    CleanTestFile(testFile);

    auto& logger = TraceLogger::Get();
    // Trace ONLY actor 0x000A2C94
    logger.StartTracing(testFile, {0x000A2C94});

    // Event for allowed actor
    logger.LogGazeTick(100.0f, 0x000A2C94, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, "LOGIC", 0, false, 0.0f, 0.0f, 0.016f);
    // Event for excluded actor
    logger.LogGazeTick(100.0f, 0x00012345, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, "LOGIC", 0, false, 0.0f, 0.0f, 0.016f);

    logger.Flush();
    logger.StopTracing();

    auto lines = ReadLines(testFile);
    assert(lines.size() == 1);
    assert(lines[0].find("0x000A2C94") != std::string::npos);

    CleanTestFile(testFile);
    std::cout << "[PASS] TestTraceLoggerActorFiltering\n";
}

void TestIncomingHitSchemaAndReceiverFilter()
{
    const std::filesystem::path path = "test_trace_incoming.jsonl";
    CleanTestFile(path);
    auto& logger = TraceLogger::Get();
    logger.StartTracing(path, {2});
    TrueGaze::Visuals::Incoming::Hit hit{};
    hit.hit = true;
    hit.observer = 1;
    hit.target = 2;
    hit.frame = 42;
    hit.source = TrueGaze::Visuals::Incoming::Source::Camera;
    hit.region = TrueGaze::Visuals::Incoming::Region::LeftEye;
    hit.quality = TrueGaze::Visuals::Incoming::Quality::Anchored;
    hit.point = {1, 2, 3};
    logger.LogIncomingHit(hit);
    hit.target = 3;
    logger.LogIncomingHit(hit);
    logger.StopTracing();
    const auto lines = ReadLines(path);
    assert(lines.size() == 1);
    assert(lines[0].find("\"schema\":2") != std::string::npos);
    assert(lines[0].find("\"t\":\"INCOMING_HIT\"") != std::string::npos);
    assert(lines[0].find("\"observer\":\"0x00000001\"") != std::string::npos);
    assert(lines[0].find("\"target\":\"0x00000002\"") != std::string::npos);
    assert(lines[0].find("\"source\":\"camera\"") != std::string::npos);
    assert(lines[0].find("\"measurement\":\"anatomy\"") != std::string::npos);
    assert(lines[0].find("\"region\":\"LeftEye\"") != std::string::npos);
    assert(lines[0].find("\"quality\":\"anchoredApproximation\"") != std::string::npos);
    assert(lines[0].find("\"visibilityVerified\":false") != std::string::npos);
    CleanTestFile(path);
}

void TestTraceLoggerSizeCapAutoStopAndTeardown()
{
    const std::filesystem::path capFile = "test_trace_cap_stop.jsonl";
    const std::filesystem::path nextFile = "test_trace_cap_next.jsonl";
    CleanTestFile(capFile);
    CleanTestFile(nextFile);

    auto& logger = TraceLogger::Get();
    // 1 MB cap, small 16-event flush batch so cap triggers quickly
    logger.SetConfig(true, false, false, false, false, false, 16, 1);
    logger.StartTracing(capFile);

    for (int i = 0; i < 6500 && logger.IsActive(); ++i)
    {
        logger.LogGazeTick(static_cast<float>(i) * 0.016f, 0x000A2C94, 0x00000014,
                           1.2f, -0.5f, 0.8f, -0.2f, 0, 255, "LOGIC", 0, false, 0.0f, 0.25f, 0.016f);
        if ((i % 64) == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Wait for WorkerLoop to flush and auto-stop on the 1 MB boundary
    for (int w = 0; w < 200 && logger.IsActive(); ++w)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!logger.IsActive());

    // Previously StopTracing() returned early when _active was already false,
    // leaving _workerThread joinable and calling std::terminate() on StartTracing()
    // or ~TraceLogger().
    logger.StopTracing();

    logger.StartTracing(nextFile);
    assert(logger.IsActive());
    logger.LogGazeTick(0.1f, 0x000A2C94, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, "LOGIC", 0, false, 0.0f, 0.0f, 0.016f);
    logger.StopTracing();

    const auto nextLines = ReadLines(nextFile);
    assert(nextLines.size() == 1);

    CleanTestFile(capFile);
    CleanTestFile(nextFile);
    std::cout << "[PASS] TestTraceLoggerSizeCapAutoStopAndTeardown\n";
}

int main()
{
    std::cout << "=== Running TraceLogger Unit Tests ===\n";
    TestTraceLoggerLifecycle();
    TestTraceLoggerGazeTickSchema();
    TestTraceLoggerSaccadeAndRegionEvents();
    TestTraceLoggerCgaBlinkAndMismatchEvents();
    TestTraceLoggerActorFiltering();
    TestIncomingHitSchemaAndReceiverFilter();
    TestTraceLoggerSizeCapAutoStopAndTeardown();
    std::cout << "=== All TraceLogger Unit Tests Passed (7/7) ===\n";
    return 0;
}
