#include <rpp/sockets.h>
#include <rpp/tests.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#if _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h> // GetCurrentProcessId
#else
#  include <fcntl.h>
#  include <unistd.h>
#endif

using namespace rpp;

// AF_Unix message sockets are only implemented on Linux + Windows; elsewhere
// the factories fail cleanly, so the round-trip tests below can't run.
#if _WIN32 || defined(__linux__)
#  define SKIP_IF_UNSUPPORTED() ((void)0)
#else
// On unsupported platforms every op is a failure stub; skip the body so the
// suite reports the test as passing rather than asserting on guaranteed failure.
#  define SKIP_IF_UNSUPPORTED() return
#endif

TestImpl(test_unix_socket)
{
    TestInit(test_unix_socket) {}

    // Unique abstract/temp name per test, so parallel runs don't collide.
    static std::string unique_name(const char* tag)
    {
        static std::atomic<int> counter{0};
        std::string s = "rpp-unixsock-";
        s += tag;
        s += '-';
        s += std::to_string(
#if _WIN32
            (long long)GetCurrentProcessId()
#else
            (long long)::getpid()
#endif
        );
        s += '-';
        s += std::to_string(counter.fetch_add(1));
        return s;
    }

    // Accept the next connection, retrying briefly (connect can race the listen).
    static unix_socket accept_with_timeout(unix_socket& listener, int timeout_ms = 2000)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline)
        {
            unix_socket s = listener.accept(0); // non-blocking poll
            if (s.good())
                return s;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return {};
    }

    // The base rpp::socket pays no per-instance cost for AF_Unix: on POSIX it
    // carries no framing member and no vtable, and the local name is not stored
    // in the socket address -- the unix state (a name member) lives only on
    // unix_socket.
    TestCase(no_base_socket_overhead)
    {
        AssertTrue((std::is_base_of_v<rpp::socket, rpp::unix_socket>));

        // The 64-byte name member is gone from the address union: raw_address /
        // ipaddress are not bloated past what IPv6 needs (raw_address used to
        // balloon to 72 bytes when it carried char UnixName[64]).
        AssertTrue(sizeof(rpp::raw_address) < 64);
        AssertTrue(sizeof(rpp::ipaddress) < 64);

#if !_WIN32
        AssertFalse(std::is_polymorphic_v<rpp::socket>);
        AssertFalse(std::is_polymorphic_v<rpp::unix_socket>);
        AssertEqual(sizeof(rpp::unix_socket), sizeof(rpp::socket) + sizeof(std::string));
#endif
    }

    TestCase(create_af_unix_basics)
    {
        SKIP_IF_UNSUPPORTED();
        socket s;
        AssertTrue(s.create(AF_Unix));
        AssertTrue(s.good());
        AssertEqual(s.family(), AF_Unix);
#if _WIN32
        AssertEqual(s.type(), ST_Stream);    // Windows: framed SOCK_STREAM
#else
        AssertEqual(s.type(), ST_SeqPacket); // Linux: SOCK_SEQPACKET
#endif
        // nodelay queries must not touch IPPROTO_TCP on AF_Unix
        AssertTrue(s.is_nodelay());
        s.close();
        AssertFalse(s.good());
    }

    // listen/connect/send/recv round trip with message boundary preservation.
    TestCase(round_trip_message_boundaries)
    {
        SKIP_IF_UNSUPPORTED();
        const std::string name = unique_name("rt");
        unix_socket listener = unix_socket::listen_unix(name);
        AssertTrue(listener.good());
        AssertEqual(listener.unix_name(), name); // factory records the name

        unix_socket writer = unix_socket::connect_unix(name);
        AssertTrue(writer.good());

        unix_socket reader = accept_with_timeout(listener);
        AssertTrue(reader.good());

        const int sizes[] = { 1, 7, 64, 1000, 2048, 13 };
        for (int sz : sizes)
        {
            std::vector<uint8_t> msg(static_cast<size_t>(sz));
            for (int i = 0; i < sz; ++i)
                msg[static_cast<size_t>(i)] = static_cast<uint8_t>((i * 31 + sz) & 0xFF);

            AssertGreater(writer.send(msg.data(), sz), 0);

            uint8_t buf[4096];
            std::memset(buf, 0, sizeof(buf));
            const int got = reader.recv(buf, sizeof(buf));
            AssertEqual(got, sz); // boundary preserved: exactly one message
            AssertEqual(std::memcmp(buf, msg.data(), static_cast<size_t>(sz)), 0);
        }
    }

    // Reusing a unix_socket object after close() must start from a clean slate
    // (on Windows this resets the software framing buffers).
    TestCase(reuse_after_close)
    {
        SKIP_IF_UNSUPPORTED();
        const std::string name = unique_name("reuse");
        unix_socket listener = unix_socket::listen_unix(name);
        AssertTrue(listener.good());

        unix_socket writer = unix_socket::connect_unix(name);
        AssertTrue(writer.good());
        unix_socket reader = accept_with_timeout(listener);
        AssertTrue(reader.good());
        AssertGreater(writer.send("one", 3), 0);
        uint8_t buf[16];
        AssertEqual(reader.recv(buf, sizeof(buf)), 3);

        writer.close();
        AssertFalse(writer.good());

        // Same object, fresh connection: framing/name state must not leak.
        writer = unix_socket::connect_unix(name);
        AssertTrue(writer.good());
        unix_socket reader2 = accept_with_timeout(listener);
        AssertTrue(reader2.good());
        AssertGreater(writer.send("two", 3), 0);
        AssertEqual(reader2.recv(buf, sizeof(buf)), 3);
    }

    // Backpressure: small buffers, flood non-blocking sends until would-block,
    // then drain and verify recovery + no corruption.
    TestCase(backpressure_and_recovery)
    {
        SKIP_IF_UNSUPPORTED();
        const std::string name = unique_name("bp");
        unix_socket listener = unix_socket::listen_unix(name);
        AssertTrue(listener.good());

        unix_socket writer = unix_socket::connect_unix(name);
        AssertTrue(writer.good());
        unix_socket reader = accept_with_timeout(listener);
        AssertTrue(reader.good());

        writer.set_rcv_buf_size(8 * 1024);
        writer.set_snd_buf_size(8 * 1024);
        reader.set_rcv_buf_size(8 * 1024);
        reader.set_snd_buf_size(8 * 1024);
        writer.set_blocking(false); // non-blocking sends to observe would-block

        const int payload = 1024;
        std::vector<uint8_t> msg(static_cast<size_t>(payload));
        for (int i = 0; i < payload; ++i)
            msg[static_cast<size_t>(i)] = static_cast<uint8_t>(i & 0xFF);

        // Flood until the sender reports would-block (send() returns 0).
        int sent = 0;
        bool blocked = false;
        for (int i = 0; i < 100000; ++i)
        {
            if (writer.send(msg.data(), payload) > 0)
            {
                ++sent;
                AssertTrue(writer.good()); // would-block must not close the socket
            }
            else
            {
                blocked = true;
                AssertTrue(writer.good()); // would-block != peer gone
                break;
            }
        }
        AssertTrue(blocked);
        AssertGreater(sent, 0);

        // Recovery: drain on the reader and verify every message is intact.
        reader.set_blocking(false);
        const int target = sent + 50; // push 50 more after recovery
        int received = 0;
        bool recovered = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (received < sent || sent < target)
        {
            AssertTrue(std::chrono::steady_clock::now() < deadline); // guard against hang

            uint8_t buf[2048];
            const int got = reader.recv(buf, sizeof(buf));
            if (got > 0)
            {
                AssertEqual(got, payload);
                AssertEqual(std::memcmp(buf, msg.data(), static_cast<size_t>(payload)), 0);
                ++received;
            }

            // After draining, keep topping up the pipe until we hit the target.
            if (sent < target && writer.send(msg.data(), payload) > 0)
            {
                ++sent;
                recovered = true; // a send succeeded again post-block
            }
            else if (got == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        AssertTrue(recovered);          // sender unblocked after the reader drained
        AssertEqual(received, sent);    // no messages lost or corrupted
    }

    // EOF -> re-accept: writer disconnects, reader's recv returns -1 and
    // self-closes; listener accepts a new writer.
    TestCase(eof_then_reaccept)
    {
        SKIP_IF_UNSUPPORTED();
        const std::string name = unique_name("eof");
        unix_socket listener = unix_socket::listen_unix(name);
        AssertTrue(listener.good());

        {
            unix_socket writer = unix_socket::connect_unix(name);
            AssertTrue(writer.good());
            unix_socket reader = accept_with_timeout(listener);
            AssertTrue(reader.good());

            AssertGreater(writer.send("hi", 2), 0);
            uint8_t buf[16];
            AssertEqual(reader.recv(buf, sizeof(buf)), 2);

            writer.close(); // peer goes away

            // Reader sees EOF: recv returns -1 and the socket self-closes.
            const int got = reader.recv(buf, sizeof(buf));
            AssertEqual(got, -1);
            AssertFalse(reader.good());
        }

        // A fresh writer can connect and the listener accepts it.
        unix_socket writer2 = unix_socket::connect_unix(name);
        AssertTrue(writer2.good());
        unix_socket reader2 = accept_with_timeout(listener);
        AssertTrue(reader2.good());

        AssertGreater(writer2.send("yo", 2), 0);
        uint8_t buf[16];
        AssertEqual(reader2.recv(buf, sizeof(buf)), 2);
    }

    // Closing the listener from another thread unblocks a blocking accept().
    TestCase(close_unblocks_accept)
    {
        SKIP_IF_UNSUPPORTED();
        const std::string name = unique_name("intr");
        unix_socket listener = unix_socket::listen_unix(name);
        AssertTrue(listener.good());

        std::atomic<bool> accept_returned{false};
        std::thread acceptor([&]
        {
            unix_socket s = listener.accept(-1); // blocks (no connection coming)
            (void)s;
            accept_returned.store(true);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
#if _WIN32
        // shutdown()/close() on Windows does not reliably unblock accept();
        // just unblock by connecting so the thread joins.
        unix_socket w = unix_socket::connect_unix(name);
        (void)w;
#else
        AssertFalse(accept_returned.load()); // still blocked before close
        listener.close();                    // close() does shutdown(SHUT_RDWR)
#endif
        acceptor.join();
        AssertTrue(accept_returned.load());
    }

#if !_WIN32
    // pair() + send_fd/recv_fd round trip through SCM_RIGHTS, plus the
    // skip path (a plain message arriving where recv_fd looks for an fd).
    TestCase(pair_send_recv_fd)
    {
        unix_socket a;
        unix_socket b;
        AssertTrue(unix_socket::pair(a, b));
        AssertTrue(a.good());
        AssertTrue(b.good());

        // Make a real fd to pass: write end of a pipe.
        int pipefd[2];
        AssertEqual(::pipe(pipefd), 0);

        const uint32_t tag = 0xDEADBEEF;
        AssertTrue(a.send_fd(tag, pipefd[1]));

        uint32_t got_tag = 0;
        int got_fd = -1;
        const int r = b.recv_fd(got_tag, got_fd);
        AssertEqual(r, 1);
        AssertEqual(got_tag, tag);
        AssertGreaterOrEqual(got_fd, 0);
        AssertNotEqual(got_fd, pipefd[1]); // a distinct fd in this process

        // The received fd is the same underlying pipe: write via it, read original.
        const char* word = "scm!";
        AssertEqual(::write(got_fd, word, 4), (ssize_t)4);
        char rb[8] = {0};
        AssertEqual(::read(pipefd[0], rb, 4), (ssize_t)4);
        AssertEqual(std::memcmp(rb, word, 4), 0);

        ::close(got_fd);
        ::close(pipefd[0]);
        ::close(pipefd[1]);

        // Skip path: a plain message (no fd) -> recv_fd returns 0.
        const uint32_t plain = 0x12345678;
        AssertGreater(a.send(&plain, sizeof(plain)), 0);
        uint32_t t2 = 0;
        int f2 = -1;
        AssertEqual(b.recv_fd(t2, f2), 0);
    }
#endif

    // Move semantics: moved-from socket invalid, moved-to works.
    TestCase(move_semantics)
    {
        SKIP_IF_UNSUPPORTED();
        const std::string name = unique_name("mv");
        unix_socket listener = unix_socket::listen_unix(name);
        AssertTrue(listener.good());

        unix_socket writer = unix_socket::connect_unix(name);
        AssertTrue(writer.good());
        unix_socket reader = accept_with_timeout(listener);
        AssertTrue(reader.good());

        const int orig_handle = writer.os_handle();

        // Move construct. We deliberately inspect the moved-from object to prove
        // it is left invalid, so suppress the use-after-move analyzers here.
        unix_socket moved{std::move(writer)};
        AssertFalse(writer.good());          // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        AssertEqual(writer.os_handle(), -1); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        AssertTrue(moved.good());
        AssertEqual(moved.os_handle(), orig_handle);

        AssertGreater(moved.send("ab", 2), 0);
        uint8_t buf[8];
        AssertEqual(reader.recv(buf, sizeof(buf)), 2);

        // Move assign over a live socket.
        unix_socket sink;
        sink = std::move(moved);
        AssertFalse(moved.good()); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        AssertTrue(sink.good());
        AssertGreater(sink.send("cd", 2), 0);
        AssertEqual(reader.recv(buf, sizeof(buf)), 2);
    }

    // wait_readable returns once data is pending; ignores invalid fds.
    TestCase(wait_readable_basic)
    {
        SKIP_IF_UNSUPPORTED();
        const std::string name = unique_name("wr");
        unix_socket listener = unix_socket::listen_unix(name);
        AssertTrue(listener.good());
        unix_socket writer = unix_socket::connect_unix(name);
        AssertTrue(writer.good());
        unix_socket reader = accept_with_timeout(listener);
        AssertTrue(reader.good());

        // Nothing pending yet: short timeout returns without crashing on bad fds.
        wait_readable({ -1, reader.os_handle(), -5 }, 50);

        AssertGreater(writer.send("ping", 4), 0);
        wait_readable({ reader.os_handle(), -1 }, 1000); // should return promptly
        uint8_t buf[16];
        AssertEqual(reader.recv(buf, sizeof(buf)), 4);
    }

    // send() rejects messages larger than MAX_UNIX_MESSAGE_SIZE (would wrap the
    // Windows 16-bit length prefix and desync the peer); a max-size message
    // still round-trips intact.
    TestCase(send_size_limit)
    {
        SKIP_IF_UNSUPPORTED();
        const std::string name = unique_name("sz");
        unix_socket listener = unix_socket::listen_unix(name);
        AssertTrue(listener.good());
        unix_socket writer = unix_socket::connect_unix(name);
        AssertTrue(writer.good());
        unix_socket reader = accept_with_timeout(listener);
        AssertTrue(reader.good());

        // Oversize is rejected and the socket stays valid.
        std::vector<uint8_t> big(static_cast<size_t>(socket::MAX_UNIX_MESSAGE_SIZE) + 1, 0xAB);
        AssertEqual(writer.send(big.data(), static_cast<int>(big.size())), -1);
        AssertTrue(writer.good());

        // A legal max-size message survives the round trip. Bump the OS buffers
        // so the single 64KB datagram fits without backpressure.
        const int max = socket::MAX_UNIX_MESSAGE_SIZE;
        writer.set_rcv_buf_size(2 * max);
        writer.set_snd_buf_size(2 * max);
        reader.set_rcv_buf_size(2 * max);
        reader.set_snd_buf_size(2 * max);

        std::vector<uint8_t> msg(static_cast<size_t>(max));
        for (int i = 0; i < max; ++i)
            msg[static_cast<size_t>(i)] = static_cast<uint8_t>((i * 7 + 1) & 0xFF);
        AssertGreater(writer.send(msg.data(), max), 0);

        std::vector<uint8_t> rbuf(static_cast<size_t>(max));
        int got = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        reader.set_blocking(false);
        while (got <= 0 && std::chrono::steady_clock::now() < deadline)
        {
            got = reader.recv(rbuf.data(), max);
            if (got == 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        AssertEqual(got, max);
        AssertEqual(std::memcmp(rbuf.data(), msg.data(), static_cast<size_t>(max)), 0);
    }

    // listen_unix()/connect_unix() with a name too long to fit the address fail
    // cleanly (no truncation that could collide with another service).
    TestCase(name_too_long_fails)
    {
        SKIP_IF_UNSUPPORTED();
        const std::string long_name(200, 'x'); // far past sizeof(sun_path)
        unix_socket listener = unix_socket::listen_unix(long_name);
        AssertFalse(listener.good());

        unix_socket client = unix_socket::connect_unix(long_name);
        AssertFalse(client.good());
    }

#if !_WIN32
    // recv_fd() on an empty non-blocking queue reports "nothing ready" (0), not
    // EOF (-1), and leaves the socket valid.
    TestCase(recv_fd_wouldblock_not_eof)
    {
        unix_socket a;
        unix_socket b;
        AssertTrue(unix_socket::pair(a, b));
        b.set_blocking(false);

        uint32_t tag = 0;
        int fd = -1;
        AssertEqual(b.recv_fd(tag, fd), 0); // nothing queued yet
        AssertTrue(b.good());               // not treated as EOF
    }

    // send_fd() after the peer is gone eventually reports failure and the socket
    // self-closes (good() == false), distinct from a transient would-block.
    TestCase(send_fd_peer_gone_self_closes)
    {
        unix_socket a;
        unix_socket b;
        AssertTrue(unix_socket::pair(a, b));
        b.close(); // peer goes away

        int pipefd[2];
        AssertEqual(::pipe(pipefd), 0);

        // The first send may succeed (buffered), but once the peer-gone error
        // surfaces send_fd() returns false and closes the socket itself.
        bool closed = false;
        for (int i = 0; i < 1000 && a.good(); ++i)
        {
            if (!a.send_fd(0xABCD, pipefd[1]) && !a.good())
            {
                closed = true;
                break;
            }
        }
        AssertTrue(closed);
        AssertFalse(a.good());

        ::close(pipefd[0]);
        ::close(pipefd[1]);
    }
#endif
};
