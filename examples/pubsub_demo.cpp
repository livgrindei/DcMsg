// Recreates the shape of the FZMQ-based publisher/subscriber demo this
// library used to ship with (a server building one DcMsg per tick and a
// client decoding each one as it arrives), but without pulling in a real
// transport dependency: a "publisher" thread and a "subscriber" thread
// exchange raw byte buffers through a small in-process Channel instead of a
// ZMQ PUB/SUB socket pair.
//
// Swap Channel::Send/Channel::Receive for your transport of choice (ZeroMQ,
// a TCP socket, a message queue, ...) and the DcMsg encode/decode calls on
// either side stay exactly the same.

#include <dcmsg/dcmsg.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    constexpr int kMessageCount = 5;

    // Stand-in for a real transport: a thread-safe queue of raw byte
    // buffers, exactly what you'd get frame-by-frame off a ZMQ_SUB socket.
    class Channel
    {
    public:
        void Send(std::vector<unsigned char> buffer)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                queue_.push_back(std::move(buffer));
            }
            cv_.notify_one();
        }

        std::vector<unsigned char> Receive()
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty(); });
            std::vector<unsigned char> buffer = std::move(queue_.front());
            queue_.pop_front();
            return buffer;
        }

    private:
        std::mutex mutex_;
        std::condition_variable cv_;
        std::deque<std::vector<unsigned char>> queue_;
    };

    void Publisher(Channel& channel)
    {
        for (uint32_t chn = 0; chn < kMessageCount; ++chn)
        {
            double value = 101.15 + chn * 0.1;

            DS::DcMsg msg;
            msg.AddUInt("Channel", chn);
            msg.AddDouble("DoubleValue", value);

            uint64_t size = 0;
            void* data = msg.GetData(size);
            std::vector<unsigned char> buffer(static_cast<unsigned char*>(data),
                                               static_cast<unsigned char*>(data) + size);

            printf("[publisher]  sent channel %u with value %.2f\n", chn, value);
            channel.Send(std::move(buffer));

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    void Subscriber(Channel& channel)
    {
        for (int i = 0; i < kMessageCount; ++i)
        {
            std::vector<unsigned char> buffer = channel.Receive();

            DS::DcMsg received(buffer.data(), buffer.size());
            if (!received.IsValid())
            {
                printf("[subscriber] received an invalid message!\n");
                continue;
            }

            uint32_t channelId = 0;
            double value = 0.0;
            if (received.GetUInt("Channel", channelId) && received.GetDouble("DoubleValue", value))
            {
                printf("[subscriber] channel %u - value: %.6f\n", channelId, value);
            }
            else
            {
                printf("[subscriber] error decoding the message!\n");
            }
        }
    }
} // namespace

int main()
{
    Channel channel;
    std::thread publisher(Publisher, std::ref(channel));
    std::thread subscriber(Subscriber, std::ref(channel));

    publisher.join();
    subscriber.join();

    return 0;
}
