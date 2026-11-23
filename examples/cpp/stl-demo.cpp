// stl-demo - STL containers/algorithms, smart pointers and exceptions on the
// Waveshare ESP32-P4 v2 board.  Output with <cstdio>: <iostream> alone needs
// ~543 KB at exec, over the 496 KB block (see board.mk).
#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// An exception type of our own.
struct SensorError : std::runtime_error {
	int code;
	SensorError(const std::string &what, int c) : std::runtime_error(what), code(c) {}
};

// RAII: the destructor must run while an exception unwinds the stack.
struct Trace {
	const char *name;
	explicit Trace(const char *n) : name(n) { std::printf("  enter %s\n", name); }
	~Trace() { std::printf("  leave %s (destructor)\n", name); }
};

static double read_sensor(int id)
{
	Trace t("read_sensor");
	if (id < 0)
		throw SensorError("sensor " + std::to_string(id) + " does not exist", 404);
	return 20.0 + id * 0.5;
}

static std::optional<int> find_first_even(const std::vector<int> &v)
{
	auto it = std::find_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
	if (it == v.end())
		return std::nullopt;
	return *it;
}

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char *what) {
		std::printf("[%s] %s\n", ok ? "OK" : "FAIL", what);
		failures += !ok;
	};

	// Containers and algorithms.
	std::vector<int> v{42, 7, 19, 3, 88, 1};
	std::sort(v.begin(), v.end());
	check(std::is_sorted(v.begin(), v.end()) && v.front() == 1 && v.back() == 88, "vector + sort");
	check(std::accumulate(v.begin(), v.end(), 0) == 160, "accumulate");
	check(find_first_even({1, 3, 6, 9}).value_or(-1) == 6 && !find_first_even({1, 3}), "optional + find_if");

	std::map<std::string, int> m{{"tres", 3}, {"uno", 1}, {"dos", 2}};
	std::string keys;
	for (const auto &[k, val] : m)
		keys += k + "=" + std::to_string(val) + " ";
	check(keys == "dos=2 tres=3 uno=1 ", "map (ordered) + structured bindings");

	std::unordered_map<std::string, std::vector<double>> series;
	for (int i = 0; i < 100; i++)
		series["t" + std::to_string(i % 4)].push_back(i * 0.25);
	check(series.size() == 4 && series["t3"].size() == 25, "unordered_map of vectors");

	// Smart pointers.
	auto shared = std::make_shared<std::string>("compartido");
	std::weak_ptr<std::string> weak = shared;
	{
		auto copy = shared;
		check(shared.use_count() == 2, "shared_ptr use_count");
	}
	auto owner = std::make_unique<std::vector<int>>(1000, 7);
	check(owner->size() == 1000 && !weak.expired(), "unique_ptr + weak_ptr");
	shared.reset();
	check(weak.expired(), "weak_ptr expires");

	// Exceptions: our own type, through a destructor.
	try {
		read_sensor(-3);
		check(false, "exception expected");
	} catch (const SensorError &e) {
		std::printf("  caught SensorError: %s (code %d)\n", e.what(), e.code);
		check(e.code == 404, "own exception type + unwinding");
	}
	// Thrown from inside libstdc++.
	try {
		v.at(100);
		check(false, "out_of_range expected");
	} catch (const std::out_of_range &) {
		check(true, "std::out_of_range from vector::at");
	}
	try {
		(void)std::stoi("no es un numero");
		check(false, "invalid_argument expected");
	} catch (const std::invalid_argument &e) {
		check(true, "std::invalid_argument from std::stoi");
	}
	try {
		std::vector<char> huge;
		huge.reserve(static_cast<size_t>(-1) / 2);
		check(false, "length_error/bad_alloc expected");
	} catch (const std::length_error &) {
		check(true, "std::length_error from reserve");
	} catch (const std::bad_alloc &) {
		check(true, "std::bad_alloc from reserve");
	}
	// Rethrow and catch by base class.
	try {
		try {
			throw SensorError("anidada", 7);
		} catch (...) {
			throw;
		}
	} catch (const std::exception &e) {
		check(std::string(e.what()) == "anidada", "rethrow, catch by std::exception");
	}

	std::printf("%s: %d failure(s)\n", failures ? "FAILED" : "ALL OK", failures);
	return failures ? 1 : 0;
}
