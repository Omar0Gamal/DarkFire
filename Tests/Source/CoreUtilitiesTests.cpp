#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"
#include "Engine/Core/Result.h"
#include "Engine/Core/StringId.h"
#include "Engine/Core/Timer.h"
#include "Engine/Core/Random.h"
#include "Engine/Core/Memory/FrameArena.h"
#include "Engine/Core/Jobs/ThreadPool.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/VFS/VirtualFileSystem.h"
#include <thread>
#include <chrono>

TEST_CASE("Core::Result - Basic Operations")
{
	SUBCASE("Ok Result with Value")
	{
		auto res = Core::Result<int>::Ok(42);
		CHECK(res.isOk());
		CHECK_FALSE(res.isErr());
		CHECK(res.value() == 42);
		CHECK(res.unwrap() == 42);
	}

	SUBCASE("Err Result with Message")
	{
		auto res = Core::Result<int>::Err("Sample Error");
		CHECK_FALSE(res.isOk());
		CHECK(res.isErr());
		CHECK(res.error() == "Sample Error");
		CHECK(res.valueOr(100) == 100);
	}

	SUBCASE("Void Result")
	{
		auto okRes = Core::Result<void>::Ok();
		CHECK(okRes.isOk());

		auto errRes = Core::Result<void>::Err("Void Error");
		CHECK(errRes.isErr());
		CHECK(errRes.error() == "Void Error");
	}
}

TEST_CASE("Core::StringId - FNV-1a Constexpr Hashing")
{
	constexpr Core::StringId id1("TransformComponent");
	constexpr Core::StringId id2("TransformComponent");
	constexpr Core::StringId id3("MeshComponent");

	CHECK(id1 == id2);
	CHECK(id1 != id3);
	CHECK(id1.value() != 0);

	constexpr auto literalId = "TransformComponent"_sid;
	CHECK(id1 == literalId);
}

TEST_CASE("Core::Timer - High-Precision Measurement")
{
	Core::Timer timer;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	double ms = timer.getElapsedMilliseconds();
	CHECK(ms > 0.0);
	CHECK(ms < 1000.0);
}

TEST_CASE("Core::Random - Seeded Determinism")
{
	Core::Random rng1(1337);
	Core::Random rng2(1337);

	for (int i = 0; i < 10; ++i)
	{
		CHECK(rng1.nextU64() == rng2.nextU64());
	}

	float f = rng1.nextFloat();
	CHECK(f >= 0.0f);
	CHECK(f < 1.0f);

	int32_t val = rng1.nextIntRange(10, 20);
	CHECK(val >= 10);
	CHECK(val <= 20);
}

TEST_CASE("Core::FrameArena - Monotonic Allocation & Alignment")
{
	Core::FrameArena arena(1024 * 1024); // 1 MB
	CHECK(arena.getUsedBytes() == 0);

	void* p1 = arena.allocate(128, 16);
	CHECK(p1 != nullptr);
	CHECK(reinterpret_cast<uintptr_t>(p1) % 16 == 0);
	CHECK(arena.getUsedBytes() >= 128);

	void* p2 = arena.allocate(256, 64);
	CHECK(p2 != nullptr);
	CHECK(reinterpret_cast<uintptr_t>(p2) % 64 == 0);

	CHECK(arena.getHighWaterMark() >= arena.getUsedBytes());

	arena.reset();
	CHECK(arena.getUsedBytes() == 0);
	CHECK(arena.getHighWaterMark() > 0);
}

TEST_CASE("Core::ThreadPool - Task Scheduling & ParallelFor")
{
	Core::ThreadPool pool(4);
	CHECK(pool.getThreadCount() == 4);

	auto future = pool.enqueue([]() { return 12345; });
	CHECK(future.get() == 12345);

	std::vector<int> numbers(100, 0);
	pool.parallelFor(numbers.size(), [&numbers](size_t i)
	{
		numbers[i] = static_cast<int>(i * 2);
	});

	for (size_t i = 0; i < numbers.size(); ++i)
	{
		CHECK(numbers[i] == static_cast<int>(i * 2));
	}
}

TEST_CASE("Core::Log - Formatting and RingBuffer")
{
	auto& log = Core::Log::Get();
	log.setLogLevel(Core::LogLevel::Trace);

	DF_LOG_INFO("UnitTest", "Test log message with value: {}", 42);
	DF_LOG_WARN("UnitTest", "Test warning message");

	auto recent = log.getRecentMessages(10);
	CHECK_FALSE(recent.empty());
}

TEST_CASE("Core::VirtualFileSystem - Mounting & Resolution")
{
	Core::VirtualFileSystem vfs;
	vfs.mount("/test/", "Scripts");

	auto res = vfs.resolve("/test/Setup.bat");
	CHECK(res.isOk());
	CHECK(vfs.exists("/test/Setup.bat"));

	auto textRes = vfs.readTextFile("/test/Setup.bat");
	CHECK(textRes.isOk());
	CHECK_FALSE(textRes.value().empty());
}

TEST_CASE("Core::VirtualFileSystem - Pak Archive Creation, FNV-1a Hashing & Reading")
{
	std::filesystem::path tempPak = "bin/TestArchive.pak";

	std::string file1Content = "DarkFire Engine Pak File 1 Content - Fast 64-bit FNV-1a lookup";
	std::vector<uint8_t> file2Content = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04 };

	std::vector<std::pair<std::string, std::vector<uint8_t>>> testFiles = {
		{ "/Shaders/TestVS.cso", std::vector<uint8_t>(file1Content.begin(), file1Content.end()) },
		{ "/Assets/BinaryData.bin", file2Content }
	};

	bool created = Core::VirtualFileSystem::CreatePakArchive(tempPak, testFiles);
	CHECK(created);
	CHECK(std::filesystem::exists(tempPak));

	Core::VirtualFileSystem vfs;
	bool mounted = vfs.mountPak("/pak", tempPak);
	CHECK(mounted);

	SUBCASE("Pak Existence & Content Verification")
	{
		CHECK(vfs.exists("/pak/Shaders/TestVS.cso"));
		CHECK(vfs.exists("/pak/Assets/BinaryData.bin"));
		CHECK_FALSE(vfs.exists("/pak/NonExistent.cso"));

		auto textRes = vfs.readTextFile("/pak/Shaders/TestVS.cso");
		CHECK(textRes.isOk());
		CHECK(textRes.value() == file1Content);

		auto binRes = vfs.readBinaryFile("/pak/Assets/BinaryData.bin");
		CHECK(binRes.isOk());
		CHECK(binRes.value() == file2Content);
	}

	SUBCASE("Case-Insensitive FNV-1a Path Hash Lookup")
	{
		CHECK(vfs.exists("/PAK/shaders/testvs.cso"));
		auto textRes = vfs.readTextFile("/PAK/shaders/testvs.cso");
		CHECK(textRes.isOk());
		CHECK(textRes.value() == file1Content);
	}

	SUBCASE("Unmount Pak")
	{
		vfs.unmountPak(tempPak);
		CHECK_FALSE(vfs.exists("/pak/Shaders/TestVS.cso"));
	}

	std::filesystem::remove(tempPak);
}

TEST_CASE("Telemetry & Profiler Stats Verification")
{
	Core::FrameArena arena(1024 * 1024);
	CHECK(arena.getCapacity() == 1024 * 1024);
	CHECK(arena.getUsedBytes() == 0);

	void* ptr = arena.allocate(512);
	CHECK(ptr != nullptr);
	CHECK(arena.getUsedBytes() >= 512);
	CHECK(arena.getHighWaterMark() >= 512);

	arena.reset();
	CHECK(arena.getUsedBytes() == 0);
	CHECK(arena.getHighWaterMark() >= 512);
}
