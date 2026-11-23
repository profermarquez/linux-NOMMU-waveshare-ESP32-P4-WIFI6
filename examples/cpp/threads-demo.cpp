// threads-demo - std::thread, std::mutex and std::condition_variable on the
// Waveshare ESP32-P4 v2 board (uClibc-ng LinuxThreads; see nommu-cxx.c).
// A bounded queue with 2 producers and 2 consumers; the checksum proves no
// item was lost or duplicated.
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

class BoundedQueue {
public:
	explicit BoundedQueue(size_t cap) : cap_(cap) {}
	void push(int v)
	{
		std::unique_lock<std::mutex> l(mu_);
		not_full_.wait(l, [&] { return q_.size() < cap_; });
		q_.push_back(v);
		not_empty_.notify_one();
	}
	// false when closed and empty.
	bool pop(int &v)
	{
		std::unique_lock<std::mutex> l(mu_);
		not_empty_.wait(l, [&] { return !q_.empty() || closed_; });
		if (q_.empty())
			return false;
		v = q_.front();
		q_.pop_front();
		not_full_.notify_one();
		return true;
	}
	void close()
	{
		std::lock_guard<std::mutex> l(mu_);
		closed_ = true;
		not_empty_.notify_all();
	}

private:
	std::mutex mu_;
	std::condition_variable not_full_, not_empty_;
	std::deque<int> q_;
	size_t cap_;
	bool closed_ = false;
};

int main()
{
	const int producers = 2, consumers = 2, per_producer = 5000;
	BoundedQueue q(16);
	std::mutex sum_mu;
	long long sum = 0;
	long count = 0;
	auto t0 = std::chrono::steady_clock::now();

	std::vector<std::thread> prod, cons;
	for (int p = 0; p < producers; p++)
		prod.emplace_back([&, p] {
			for (int i = 1; i <= per_producer; i++)
				q.push(p * per_producer + i);
		});
	for (int c = 0; c < consumers; c++)
		cons.emplace_back([&] {
			long long local = 0;
			long n = 0;
			int v;
			while (q.pop(v)) {
				local += v;
				n++;
			}
			std::lock_guard<std::mutex> l(sum_mu);
			sum += local;
			count += n;
		});
	for (auto &t : prod)
		t.join();
	q.close();
	for (auto &t : cons)
		t.join();

	long n = static_cast<long>(producers) * per_producer;
	long long expect = n * (n + 1) / 2;
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();
	std::printf("hardware_concurrency=%u\n", std::thread::hardware_concurrency());
	std::printf("%d producers, %d consumers: %ld items, sum=%lld (expect %lld), %lld ms\n",
		producers, consumers, count, sum, expect, static_cast<long long>(ms));
	bool ok = count == n && sum == expect;
	std::printf("%s\n", ok ? "ALL OK" : "FAILED");
	return ok ? 0 : 1;
}
