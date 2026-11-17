#include "PCH.h"

#include <windows.h>
#include <sddl.h> // ConvertStringSecurityDescriptorToSecurityDescriptorA

#include "NamedPipeServer.hpp"
#include <cstring>
#include <chrono>
#include <format>
#include <string>
#include <vector>

namespace TrueGaze::Bridge
{

    namespace
    {

        uint32_t ComputeCrc32(const uint8_t *data, size_t length) noexcept
        {
            uint32_t crc = 0xFFFFFFFF;
            for (size_t i = 0; i < length; ++i)
            {
                crc ^= data[i];
                for (int j = 0; j < 8; ++j)
                {
                    crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
                }
            }
            return ~crc;
        }

        uint64_t NowMicroseconds() noexcept
        {
            using namespace std::chrono;
            return static_cast<uint64_t>(
                duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
        }

        /// Builds a security descriptor granting access to the current user only.
        ///
        /// The pipe carries biometric data, so the default DACL — which typically permits
        /// any process in the same session — is not appropriate. See GOVERNANCE.md
        /// "Law 6 — Biometric data protection".
        ///
        /// On failure the caller falls back to the default DACL and logs, rather than
        /// refusing to start: a permissive pipe is preferable to a mod that cannot load,
        /// provided the fallback is reported.
        class UserOnlySecurityDescriptor
        {
        public:
            UserOnlySecurityDescriptor() noexcept
            {
                // Grant GENERIC_ALL solely to the current process logon user's TokenUser
                // SID (not Owner Rights "OW", which binds to TokenOwner and breaks or
                // widens access across UAC split-token elevation). Fail closed if any
                // step fails so the biometric pipe never falls back to a default DACL.
                HANDLE hToken = nullptr;
                if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
                {
                    return;
                }

                DWORD needed = 0;
                GetTokenInformation(hToken, TokenUser, nullptr, 0, &needed);
                if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || needed == 0)
                {
                    CloseHandle(hToken);
                    return;
                }

                std::vector<uint8_t> buf(needed);
                if (!GetTokenInformation(hToken, TokenUser, buf.data(), needed, &needed))
                {
                    CloseHandle(hToken);
                    return;
                }
                CloseHandle(hToken);

                const auto *tokenUser = reinterpret_cast<const TOKEN_USER *>(buf.data());
                LPSTR sidStr = nullptr;
                if (!ConvertSidToStringSidA(tokenUser->User.Sid, &sidStr) || !sidStr)
                {
                    return;
                }

                const std::string sddl = std::format("D:(A;;GA;;;{})", sidStr);
                LocalFree(sidStr);

                if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
                        sddl.c_str(), SDDL_REVISION_1, &_descriptor, nullptr))
                {
                    _descriptor = nullptr;
                    return;
                }

                _attributes.nLength = sizeof(_attributes);
                _attributes.lpSecurityDescriptor = _descriptor;
                _attributes.bInheritHandle = FALSE;
                _valid = true;
            }

            ~UserOnlySecurityDescriptor() noexcept
            {
                if (_descriptor)
                {
                    LocalFree(_descriptor);
                }
            }

            UserOnlySecurityDescriptor(const UserOnlySecurityDescriptor &) = delete;
            UserOnlySecurityDescriptor &operator=(const UserOnlySecurityDescriptor &) = delete;

            [[nodiscard]] bool IsValid() const noexcept { return _valid; }

            [[nodiscard]] SECURITY_ATTRIBUTES *Attributes() noexcept
            {
                return _valid ? &_attributes : nullptr;
            }

        private:
            PSECURITY_DESCRIPTOR _descriptor{nullptr};
            SECURITY_ATTRIBUTES _attributes{};
            bool _valid{false};
        };

        /// Logs a runtime-formatted message.
        ///
        /// This TU builds into both the plugin (SKSE `logger`, which only accepts
        /// compile-time format strings) and the standalone bridge mock (where `logger`
        /// is spdlog or a one-argument stub). Pre-formatting with std::format and
        /// passing the result through "{}" or as a single argument works in all three.
        void LogInfo(const std::string &message)
        {
#if __has_include(<SKSE/SKSE.h>)
            logger::info("{}", message);
#else
            logger::info(message);
#endif
        }

        /// Who is on the other end of the pipe. Used only for the connection audit log.
        struct PipeClientIdentity
        {
            ULONG processId{0};
            ULONG sessionId{0};
            bool processIdKnown{false};
            bool sessionIdKnown{false};
            std::string imageName{"unknown"};
        };

        /// Identifies the connected client for the audit log (issue #7).
        ///
        /// Only the executable's file name is recorded, not its full path: a full
        /// path usually contains the Windows user name, and plugin logs are often
        /// shared in support requests. If any query fails, the log says "unknown"
        /// for that field rather than leaving it out.
        PipeClientIdentity QueryPipeClient(HANDLE hPipe)
        {
            PipeClientIdentity id;
            id.processIdKnown = GetNamedPipeClientProcessId(hPipe, &id.processId) != FALSE;
            id.sessionIdKnown = GetNamedPipeClientSessionId(hPipe, &id.sessionId) != FALSE;

            if (id.processIdKnown)
            {
                if (HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, id.processId))
                {
                    char path[MAX_PATH]{};
                    DWORD length = MAX_PATH;
                    if (QueryFullProcessImageNameA(process, 0, path, &length) && length > 0)
                    {
                        const std::string_view full(path, length);
                        const auto slash = full.find_last_of("\\/");
                        id.imageName = std::string(
                            slash == std::string_view::npos ? full : full.substr(slash + 1));
                    }
                    CloseHandle(process);
                }
            }
            return id;
        }

    } // namespace

    void NamedPipeServer::Start(const char *pipeName, float reconnectIntervalSec) noexcept
    {
        if (_isRunning.exchange(true))
        {
            return; // Already running
        }

        // Snapshot the configuration once. The worker reads these plain fields for
        // its lifetime; Start is the only writer and runs before the thread exists.
        if (pipeName && *pipeName)
        {
            _pipeName = pipeName;
        }
        _reconnectIntervalSec = (reconnectIntervalSec > 0.05f) ? reconnectIntervalSec : 3.0f;

        if (!_shutdownEvent)
        {
            _shutdownEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        }
        ResetEvent(_shutdownEvent);

        if (!_connectOverlapped.hEvent)
        {
            _connectOverlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        }
        ResetEvent(_connectOverlapped.hEvent);

        if (!_readOverlapped.hEvent)
        {
            _readOverlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        }
        ResetEvent(_readOverlapped.hEvent);

        if (!_drainEvent)
        {
            _drainEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        }
        ResetEvent(_drainEvent);

        if (!_outboundEvent)
        {
            _outboundEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        }
        ResetEvent(_outboundEvent);

        _workerThread = std::make_unique<std::thread>(&NamedPipeServer::WorkerLoop, this);
    }

    void NamedPipeServer::Stop(JoinPolicy policy) noexcept
    {
        if (policy == JoinPolicy::Abandon)
        {
            _abandoned.store(true, std::memory_order_release);
        }

        if (!_isRunning.exchange(false))
        {
            return;
        }

        // Signal the shutdown event first so any pending asynchronous
        // ConnectNamedPipe, ReadFile, or reconnect timer immediately awakens.
        if (_shutdownEvent)
        {
            SetEvent(_shutdownEvent);
        }

        // Cancel any pending overlapped I/O so the worker's wait points return
        // promptly even if a client is mid-transfer. Do NOT close the pipe handle
        // until after _workerThread.join() so the worker cannot race a recycled handle.
        HANDLE h = nullptr;
        if (void *raw = _pipeHandle.exchange(nullptr))
        {
            h = static_cast<HANDLE>(raw);
            if (h != INVALID_HANDLE_VALUE)
            {
                CancelIoEx(h, nullptr);
            }
            else
            {
                h = nullptr;
            }
        }

        if (policy == JoinPolicy::Join)
        {
            // NORMAL shutdown: join unconditionally. Every wait point in the
            // worker is bounded by the shutdown event + CancelIoEx, so this
            // returns in milliseconds.
            if (_workerThread && _workerThread->joinable())
            {
                _workerThread->join();
            }
            _workerThread.reset();

            if (h)
            {
                DisconnectNamedPipe(h);
                CloseHandle(h);
            }

            // Safe to release the handles: the worker is provably stopped.
            if (_shutdownEvent)
            {
                CloseHandle(_shutdownEvent);
                _shutdownEvent = nullptr;
            }
            if (_connectOverlapped.hEvent)
            {
                CloseHandle(_connectOverlapped.hEvent);
                _connectOverlapped.hEvent = nullptr;
            }
            if (_readOverlapped.hEvent)
            {
                CloseHandle(_readOverlapped.hEvent);
                _readOverlapped.hEvent = nullptr;
            }
            if (_drainEvent)
            {
                CloseHandle(_drainEvent);
                _drainEvent = nullptr;
            }
            if (_outboundEvent)
            {
                CloseHandle(_outboundEvent);
                _outboundEvent = nullptr;
            }
        }
        else
        {
            // ABANDON (process exit / loader-lock window): _abandoned was latched
            // above so ~NamedPipeServer() becomes a no-op, and _workerThread is
            // released rather than joined/destroyed so std::terminate() cannot fire.
            (void)_workerThread.release();
            logger::debug("[TrueGaze] Bridge server abandoned at process exit "
                          "(worker left to terminate with the process).");
        }

        _isConnected.store(false, std::memory_order_relaxed);
    }

    void NamedPipeServer::WorkerLoop() noexcept
    {
        // Rate-limit: a failed security descriptor used to warn on EVERY
        // reconnect cycle (every reconnectIntervalSec while idle) — log spam
        // with no new information. One report per worker lifetime is enough
        // (R14 E1.3 / audit B-10).
        static std::atomic<bool> s_securityFailed{false};
        while (_isRunning.load(std::memory_order_relaxed))
        {
            UserOnlySecurityDescriptor security;
            if (!security.IsValid())
            {
                if (!s_securityFailed.exchange(true, std::memory_order_relaxed))
                {
                    logger::error("[TrueGaze] Failed to build user-only pipe DACL; "
                                  "refusing to create unprotected named pipe (fail closed).");
                }
                if (_shutdownEvent)
                {
                    WaitForSingleObject(_shutdownEvent,
                                        static_cast<DWORD>(_reconnectIntervalSec * 1000.0f));
                }
                continue;
            }

            HANDLE hPipe = CreateNamedPipeA(
                _pipeName.c_str(),
                PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                // Audit 2026-10-15 F-08: PIPE_REJECT_REMOTE_CLIENTS (a pipe-MODE flag,
                // Vista+) makes the kernel refuse network (SMB) clients outright.
                // The user-only DACL above limits who; this limits where from.
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                1,    // max instances
                4096, // out buffer
                4096, // in buffer
                5000, // default timeout (ms)
                security.Attributes());

            if (hPipe == INVALID_HANDLE_VALUE)
            {
                if (_shutdownEvent)
                {
                    WaitForSingleObject(_shutdownEvent,
                                        static_cast<DWORD>(_reconnectIntervalSec * 1000.0f));
                }
                continue;
            }

            _pipeHandle.store(hPipe, std::memory_order_release);

            ResetEvent(_connectOverlapped.hEvent);
            _connectOverlapped.Offset = 0;
            _connectOverlapped.OffsetHigh = 0;

            const BOOL connResult = ConnectNamedPipe(hPipe, &_connectOverlapped);
            const DWORD connErr = GetLastError();
            bool clientConnected = false;

            if (connResult)
            {
                clientConnected = true;
            }
            else if (connErr == ERROR_PIPE_CONNECTED)
            {
                clientConnected = true;
            }
            else if (connErr == ERROR_IO_PENDING)
            {
                HANDLE waitEvents[2] = {_shutdownEvent, _connectOverlapped.hEvent};
                DWORD waitRes = WaitForMultipleObjects(2, waitEvents, FALSE, INFINITE);
                if (waitRes == WAIT_OBJECT_0)
                {
                    // Shutdown requested while waiting for client connection
                    CancelIoEx(hPipe, &_connectOverlapped);
                    DWORD dummy = 0;
                    GetOverlappedResult(hPipe, &_connectOverlapped, &dummy, TRUE);
                    break;
                }
                else if (waitRes == WAIT_OBJECT_0 + 1)
                {
                    DWORD bytesTransferred = 0;
                    if (GetOverlappedResult(hPipe, &_connectOverlapped, &bytesTransferred, FALSE) ||
                        GetLastError() == ERROR_PIPE_CONNECTED)
                    {
                        clientConnected = true;
                    }
                }
            }

            if (clientConnected && _isRunning.load(std::memory_order_relaxed))
            {
                _isConnected.store(true, std::memory_order_release);

                // Connection audit (issue #7): record who connected and under which
                // access policy. Packet contents are never logged.
                const PipeClientIdentity client = QueryPipeClient(hPipe);
                LogInfo(std::format(
                    "[TrueGaze] HCEP bridge client connected on {}: pid={} session={} image='{}' "
                    "access={} trackedPersonId={}.",
                    _pipeName,
                    client.processIdKnown ? std::to_string(client.processId) : "unknown",
                    client.sessionIdKnown ? std::to_string(client.sessionId) : "unknown",
                    client.imageName,
                    security.IsValid() ? "user-only ACL" : "default DACL (fallback)",
                    _retainTrackedPersonId.load(std::memory_order_relaxed) ? "retained (opt-in)"
                                                                           : "discarded"));

                uint64_t framesAccepted = 0;
                uint64_t framesRejectedIntegrity = 0; // size, magic or CRC
                uint64_t framesRejectedSemantic = 0;  // ValidateTelemetryPacket

                auto drainOversizeMessage = [&]() noexcept
                {
                    uint8_t discard[256]{};
                    for (;;)
                    {
                        ResetEvent(_readOverlapped.hEvent);
                        _readOverlapped.Offset = 0;
                        _readOverlapped.OffsetHigh = 0;
                        DWORD junkRead = 0;
                        BOOL ok = ReadFile(hPipe, discard, sizeof(discard), &junkRead, &_readOverlapped);
                        if (ok)
                        {
                            return; // Reached end of message
                        }
                        DWORD err = GetLastError();
                        if (err == ERROR_MORE_DATA)
                        {
                            continue;
                        }
                        if (err == ERROR_IO_PENDING)
                        {
                            HANDLE waitArr[2] = {_shutdownEvent, _readOverlapped.hEvent};
                            DWORD w = WaitForMultipleObjects(2, waitArr, FALSE, INFINITE);
                            if (w == WAIT_OBJECT_0)
                            {
                                CancelIoEx(hPipe, &_readOverlapped);
                                DWORD dummy = 0;
                                GetOverlappedResult(hPipe, &_readOverlapped, &dummy, TRUE);
                                return;
                            }
                            if (!GetOverlappedResult(hPipe, &_readOverlapped, &junkRead, TRUE))
                            {
                                if (GetLastError() == ERROR_MORE_DATA)
                                {
                                    continue;
                                }
                            }
                            return;
                        }
                        return;
                    }
                };

                auto processIncoming = [&](DWORD bytesRead) noexcept
                {
                    if (bytesRead != sizeof(TrueGazeTelemetryPacket))
                    {
                        ++framesRejectedIntegrity;
                        return;
                    }

                    if (_incomingPacket.magic != HCEP_MAGIC)
                    {
                        ++framesRejectedIntegrity;
                        return;
                    }

                    const uint32_t expectedCrc = ComputeCrc32(
                        reinterpret_cast<const uint8_t *>(&_incomingPacket),
                        sizeof(_incomingPacket) - sizeof(uint32_t));

                    if (_incomingPacket.crc32 != expectedCrc)
                    {
                        ++framesRejectedIntegrity;
                        return;
                    }

                    if (!ValidateTelemetryPacket(_incomingPacket))
                    {
                        ++framesRejectedSemantic;
                        logger::warn("[TrueGaze] Rejected semantically invalid HCEP telemetry frame.");
                        return;
                    }

                    // --- Data minimisation (issue #7) -----------------------------
                    //
                    // trackedPersonId links a frame to an identified person and is not
                    // consumed by any gameplay code, so it is discarded here, at the
                    // trust boundary, before the packet is visible to the game thread.
                    if (!_retainTrackedPersonId.load(std::memory_order_relaxed))
                    {
                        _incomingPacket.trackedPersonId = 0;
                    }

                    ++framesAccepted;

                    const uint32_t writeIdx =
                        (_writeIndex.load(std::memory_order_relaxed) + 1) % SLOT_COUNT;

                    _slots[writeIdx].packet = _incomingPacket;
                    _slots[writeIdx].receivedAtUs = NowMicroseconds();
                    _slots[writeIdx].sequence = _incomingPacket.sequenceId;

                    _writeIndex.store(writeIdx, std::memory_order_relaxed);
                    _publishEpoch.fetch_add(1, std::memory_order_release);
                    _readyIndex.store(writeIdx, std::memory_order_release);
                };

                bool readPending = false;
                while (_isRunning.load(std::memory_order_relaxed) && _isConnected.load(std::memory_order_relaxed))
                {
                    // Drain anything the game thread queued for the client.
                    DrainOutboundQueue(hPipe);

                    if (!readPending)
                    {
                        ResetEvent(_readOverlapped.hEvent);
                        _readOverlapped.Offset = 0;
                        _readOverlapped.OffsetHigh = 0;
                        DWORD bytesRead = 0;
                        BOOL okRead = ReadFile(hPipe, &_incomingPacket, sizeof(_incomingPacket),
                                               &bytesRead, &_readOverlapped);
                        if (okRead)
                        {
                            processIncoming(bytesRead);
                            continue;
                        }

                        const DWORD err = GetLastError();
                        if (err == ERROR_IO_PENDING)
                        {
                            readPending = true;
                        }
                        else if (err == ERROR_MORE_DATA)
                        {
                            drainOversizeMessage();
                            ++framesRejectedIntegrity;
                            continue;
                        }
                        else if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED)
                        {
                            break;
                        }
                        else
                        {
                            break;
                        }
                    }

                    if (readPending)
                    {
                        HANDLE waitArr[3] = {_shutdownEvent, _readOverlapped.hEvent, _outboundEvent};
                        const DWORD waitCount = _outboundEvent ? 3 : 2;
                        DWORD waitRead = WaitForMultipleObjects(waitCount, waitArr, FALSE, INFINITE);
                        if (waitRead == WAIT_OBJECT_0)
                        {
                            CancelIoEx(hPipe, &_readOverlapped);
                            DWORD dummy = 0;
                            GetOverlappedResult(hPipe, &_readOverlapped, &dummy, TRUE);
                            readPending = false;
                            break;
                        }
                        else if (waitRead == WAIT_OBJECT_0 + 1)
                        {
                            readPending = false;
                            DWORD bytesRead = 0;
                            if (!GetOverlappedResult(hPipe, &_readOverlapped, &bytesRead, FALSE))
                            {
                                const DWORD err = GetLastError();
                                if (err == ERROR_MORE_DATA)
                                {
                                    drainOversizeMessage();
                                    ++framesRejectedIntegrity;
                                    continue;
                                }
                                break; // ERROR_BROKEN_PIPE or I/O error
                            }
                            processIncoming(bytesRead);
                        }
                        else if (waitRead == WAIT_OBJECT_0 + 2)
                        {
                            // Outbound feedback queued; loop around to DrainOutboundQueue
                            // while keeping the overlapped ReadFile active.
                            continue;
                        }
                        else
                        {
                            CancelIoEx(hPipe, &_readOverlapped);
                            DWORD dummy = 0;
                            GetOverlappedResult(hPipe, &_readOverlapped, &dummy, TRUE);
                            readPending = false;
                            break;
                        }
                    }
                }

                if (readPending)
                {
                    CancelIoEx(hPipe, &_readOverlapped);
                    DWORD dummy = 0;
                    GetOverlappedResult(hPipe, &_readOverlapped, &dummy, TRUE);
                }

                _isConnected.store(false, std::memory_order_release);
                LogInfo(std::format(
                    "[TrueGaze] HCEP bridge client disconnected: pid={} image='{}' "
                    "framesAccepted={} rejectedIntegrity={} rejectedSemantic={}.",
                    client.processIdKnown ? std::to_string(client.processId) : "unknown",
                    client.imageName, framesAccepted, framesRejectedIntegrity,
                    framesRejectedSemantic));
            }

            if (void *raw = _pipeHandle.exchange(nullptr))
            {
                HANDLE h = static_cast<HANDLE>(raw);
                if (h != INVALID_HANDLE_VALUE)
                {
                    DisconnectNamedPipe(h);
                    CloseHandle(h);
                }
            }

            if (_isRunning.load(std::memory_order_relaxed) && _shutdownEvent)
            {
                WaitForSingleObject(_shutdownEvent,
                                    static_cast<DWORD>(_reconnectIntervalSec * 1000.0f));
            }
        }
    }

    void NamedPipeServer::DrainOutboundQueue(void *pipeHandle) noexcept
    {
        if (!pipeHandle)
        {
            return;
        }

        HANDLE hPipe = static_cast<HANDLE>(pipeHandle);

        uint32_t head = _feedbackHead.load(std::memory_order_relaxed);
        const uint32_t tail = _feedbackTail.load(std::memory_order_acquire);

        // Reused drain event (R14 E1.3 / audit C-3): creating an event per call
        // churned a kernel object every ~2 ms while a client was connected. The
        // event lives for the server's lifetime and is single-consumer (the
        // worker), so no synchronisation beyond ResetEvent is needed.
        if (!_drainEvent)
        {
            _drainEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        }
        _writeOverlapped = OVERLAPPED{};
        _writeOverlapped.hEvent = _drainEvent;

        while (head != tail)
        {
            SkyrimFeedbackPacket packet = _feedbackRing[head];
            head = (head + 1) % FEEDBACK_RING_SIZE;

            packet.magic = SKYR_MAGIC;
            packet.crc32 = ComputeCrc32(
                reinterpret_cast<const uint8_t *>(&packet),
                sizeof(packet) - sizeof(uint32_t));

            ResetEvent(_writeOverlapped.hEvent);
            _writeOverlapped.Offset = 0;
            _writeOverlapped.OffsetHigh = 0;
            DWORD written = 0;
            BOOL ok = WriteFile(hPipe, &packet, sizeof(packet), &written, &_writeOverlapped);
            if (!ok)
            {
                if (GetLastError() == ERROR_IO_PENDING)
                {
                    HANDLE waitHandles[2] = {_shutdownEvent, _writeOverlapped.hEvent};
                    DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, 50);
                    if (waitRes == WAIT_OBJECT_0 + 1)
                    {
                        if (!GetOverlappedResult(hPipe, &_writeOverlapped, &written, FALSE) || written != sizeof(packet))
                        {
                            break;
                        }
                    }
                    else
                    {
                        CancelIoEx(hPipe, &_writeOverlapped);
                        DWORD dummy = 0;
                        GetOverlappedResult(hPipe, &_writeOverlapped, &dummy, TRUE);
                        break;
                    }
                }
                else
                {
                    break; // Client went away
                }
            }
            else if (written != sizeof(packet))
            {
                break;
            }
        }

        // NOTE: the drain event is intentionally NOT closed here — it is a
        // member created on Start and closed on Stop (R14 E1.3).

        _feedbackHead.store(head, std::memory_order_release);
    }

    bool NamedPipeServer::TryGetLatestTelemetry(TrueGazeTelemetryPacket &outPacket) noexcept
    {
        if (!_isConnected.load(std::memory_order_relaxed))
        {
            return false;
        }

        // Bounded retry: if the writer reuses the slot mid-copy we simply read again.
        // With three slots this should never iterate more than a handful of times.
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            const uint32_t epochBefore = _publishEpoch.load(std::memory_order_acquire);
            const uint32_t index = _readyIndex.load(std::memory_order_acquire);

            if (index >= SLOT_COUNT)
            {
                return false; // Nothing published yet
            }

            const TelemetrySlot snapshot = _slots[index];

            const uint32_t epochAfter = _publishEpoch.load(std::memory_order_acquire);
            if (epochBefore != epochAfter)
            {
                continue; // A publish happened during the copy; retry
            }

            if (snapshot.packet.magic != HCEP_MAGIC)
            {
                return false;
            }

            // Reject stale telemetry. If HCEP Desktop stops sending (crashed, paused,
            // sensor unplugged) we must stop driving NPCs from frozen data.
            const uint64_t now = NowMicroseconds();
            if (now > snapshot.receivedAtUs && (now - snapshot.receivedAtUs) > TELEMETRY_TIMEOUT_US)
            {
                return false;
            }

            outPacket = snapshot.packet;
            return true;
        }

        return false;
    }

    void NamedPipeServer::SendFeedback(const SkyrimFeedbackPacket &feedback) noexcept
    {
        // Called on the game thread. Must never block and must never touch the pipe
        // handle — that belongs exclusively to the worker. Feedback is queued here and
        // transmitted by DrainOutboundQueue().
        const uint32_t tail = _feedbackTail.load(std::memory_order_relaxed);
        const uint32_t nextTail = (tail + 1) % FEEDBACK_RING_SIZE;

        // Full ring: drop the NEW packet rather than advancing the consumer's head.
        // Advancing _feedbackHead here would race with DrainOutboundQueue reading
        // _feedbackRing[head] — the worker could observe head == tail with a slot
        // mid-write, or read a torn packet. Dropping the newest sample is the safe
        // SPSC policy: the consumer owns its head, the producer owns its tail, and
        // a full ring means the consumer is behind, so the freshest data is the
        // expendable one.
        if (nextTail == _feedbackHead.load(std::memory_order_acquire))
        {
            return;
        }

        _feedbackRing[tail] = feedback;
        _feedbackTail.store(nextTail, std::memory_order_release);
        if (_outboundEvent)
        {
            SetEvent(_outboundEvent);
        }
    }

} // namespace TrueGaze::Bridge
