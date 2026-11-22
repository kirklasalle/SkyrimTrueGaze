#include "ExpandedPlay/CoSaveSerializer.hpp"

#include "Engine/ConfigManager.hpp"
#include "PCH.h"

#include <mutex>
#include <utility>

namespace TrueGaze::ExpandedPlay
{
    namespace
    {
        std::mutex g_stateMutex;
        PersistentState g_state{};

#if __has_include(<SKSE/SKSE.h>)
        constexpr uint32_t kMaxRecordBytes = 4u * 1024u * 1024u;

        void OnGameSaved(SKSE::SerializationInterface* a_intfc)
        {
            if (!a_intfc)
            {
                return;
            }

            const auto state = CoSaveSerializer::Get().SnapshotState();
            const bool enabled = Engine::ConfigManager::GetSingleton().expandedPlayEnabled;
            if (!enabled && !state.HasMeaningfulState())
            {
                logger::debug(
                    "[TrueGaze::ExpandedPlay] Co-save write skipped (disabled, clean state).");
                return;
            }

            const auto signBytes = EncodeSignRecord(state.sign);
            if (!a_intfc->WriteRecord(kRecordTagSign, kSerializationVersion, signBytes.data(),
                                      static_cast<std::uint32_t>(signBytes.size())))
            {
                logger::error("[TrueGaze::ExpandedPlay] Failed to write SIGN co-save record.");
            }

            if (!state.ledger.empty())
            {
                const auto ledgerBytes = EncodeLedgerRecord(state.ledger);
                if (!a_intfc->WriteRecord(kRecordTagLedg, kSerializationVersion, ledgerBytes.data(),
                                          static_cast<std::uint32_t>(ledgerBytes.size())))
                {
                    logger::error("[TrueGaze::ExpandedPlay] Failed to write LEDG co-save record.");
                }
            }

            if (!state.books.empty())
            {
                const auto bookBytes = EncodeBookRecord(state.books);
                if (!a_intfc->WriteRecord(kRecordTagBook, kSerializationVersion, bookBytes.data(),
                                          static_cast<std::uint32_t>(bookBytes.size())))
                {
                    logger::error("[TrueGaze::ExpandedPlay] Failed to write BOOK co-save record.");
                }
            }

            if (state.awakening.HasAnyProgress())
            {
                const auto awknBytes = EncodeAwakeningRecord(state.awakening);
                if (!a_intfc->WriteRecord(kRecordTagAwkn, kSerializationVersion, awknBytes.data(),
                                          static_cast<std::uint32_t>(awknBytes.size())))
                {
                    logger::error("[TrueGaze::ExpandedPlay] Failed to write AWKN co-save record.");
                }
            }

            logger::info(
                "[TrueGaze::ExpandedPlay] Saved co-save (presenceMax={:.1f}, ledger={}, books={}, "
                "studied={}, awakening={}).",
                state.sign.presence.maximum, state.ledger.size(), state.books.size(),
                state.sign.booksFullyStudiedCount, FormatTumblerGlyphs(state.awakening));
        }

        void OnGameLoaded(SKSE::SerializationInterface* a_intfc)
        {
            if (!a_intfc)
            {
                return;
            }

            PersistentState loaded{};
            const FormIdResolver resolver = [a_intfc](uint32_t oldId, uint32_t& newId) -> bool
            {
                RE::FormID resolved = 0;
                if (oldId == 0u || !a_intfc->ResolveFormID(static_cast<RE::FormID>(oldId), resolved))
                {
                    return false;
                }
                newId = static_cast<uint32_t>(resolved);
                return newId != 0u;
            };

            std::uint32_t type = 0;
            std::uint32_t version = 0;
            std::uint32_t length = 0;
            while (a_intfc->GetNextRecordInfo(type, version, length))
            {
                if (length == 0u || length > kMaxRecordBytes)
                {
                    logger::warn(
                        "[TrueGaze::ExpandedPlay] Skipping co-save record {:08X} v{} with invalid "
                        "length {}.",
                        type, version, length);
                    continue;
                }

                std::vector<uint8_t> buffer(length);
                const auto bytesRead = a_intfc->ReadRecordData(buffer.data(), length);
                if (bytesRead != length)
                {
                    logger::warn(
                        "[TrueGaze::ExpandedPlay] Truncated co-save record {:08X} ({}/{} bytes).",
                        type, bytesRead, length);
                    continue;
                }

                if (type == kRecordTagSign)
                {
                    if (!DecodeSignRecord(buffer, version, loaded.sign))
                    {
                        logger::warn(
                            "[TrueGaze::ExpandedPlay] Failed to decode SIGN v{} record.",
                            version);
                    }
                }
                else if (type == kRecordTagLedg)
                {
                    if (!DecodeLedgerRecord(buffer, version, loaded.ledger, resolver))
                    {
                        logger::warn(
                            "[TrueGaze::ExpandedPlay] Failed to decode LEDG v{} record.",
                            version);
                    }
                }
                else if (type == kRecordTagBook)
                {
                    if (!DecodeBookRecord(buffer, version, loaded.books, resolver))
                    {
                        logger::warn(
                            "[TrueGaze::ExpandedPlay] Failed to decode BOOK v{} record.",
                            version);
                    }
                }
                else if (type == kRecordTagAwkn)
                {
                    if (!DecodeAwakeningRecord(buffer, version, loaded.awakening))
                    {
                        logger::warn(
                            "[TrueGaze::ExpandedPlay] Failed to decode AWKN v{} record.",
                            version);
                    }
                }
                else
                {
                    logger::warn(
                        "[TrueGaze::ExpandedPlay] Unknown co-save record tag {:08X} v{}.",
                        type, version);
                }
            }

            // Backward compatibility: pre-WP-9 saves that already awakened the First Canticle
            // retain full Stage 4 progression cleanly.
            if (loaded.sign.presence.canticleAwakened && !loaded.awakening.HasAnyProgress())
            {
                ForceUnlockAllForConsole(loaded.awakening);
            }

            const size_t ledgerCount = loaded.ledger.size();
            const size_t bookCount = loaded.books.size();
            const uint32_t studied = loaded.sign.booksFullyStudiedCount;
            const float maxPresence = loaded.sign.presence.maximum;
            const std::string awknGlyphs = FormatTumblerGlyphs(loaded.awakening);

            CoSaveSerializer::Get().SetState(std::move(loaded));

            logger::info(
                "[TrueGaze::ExpandedPlay] Loaded co-save (presenceMax={:.1f}, ledger={}, books={}, "
                "studied={}, awakening={}).",
                maxPresence, ledgerCount, bookCount, studied, awknGlyphs);
        }

        void OnRevert(SKSE::SerializationInterface*)
        {
            CoSaveSerializer::Get().ResetState();
            logger::debug("[TrueGaze::ExpandedPlay] Co-save state reverted.");
        }
#endif
    } // namespace

    CoSaveSerializer& CoSaveSerializer::Get() noexcept
    {
        static CoSaveSerializer instance;
        return instance;
    }

    void CoSaveSerializer::RegisterWithSkse() noexcept
    {
#if __has_include(<SKSE/SKSE.h>)
        static bool registered = false;
        if (registered)
        {
            return;
        }

        auto* serde = SKSE::GetSerializationInterface();
        if (!serde)
        {
            logger::warn(
                "[TrueGaze::ExpandedPlay] SKSE serialization interface unavailable; co-save "
                "persistence disabled.");
            return;
        }

        serde->SetUniqueID(kSerializationUid);
        serde->SetSaveCallback(OnGameSaved);
        serde->SetLoadCallback(OnGameLoaded);
        serde->SetRevertCallback(OnRevert);
        registered = true;

        logger::info(
            "[TrueGaze::ExpandedPlay] Registered SKSE co-save serializer (UID=0x{:08X} v{}).",
            kSerializationUid, kSerializationVersion);
#endif
    }

    void CoSaveSerializer::ResetState() noexcept
    {
        std::lock_guard lock(g_stateMutex);
        g_state.Clear();
    }

    PersistentState CoSaveSerializer::SnapshotState() const
    {
        std::lock_guard lock(g_stateMutex);
        return g_state;
    }

    void CoSaveSerializer::SetState(PersistentState state)
    {
        std::lock_guard lock(g_stateMutex);
        g_state = std::move(state);
    }

    size_t CoSaveSerializer::LedgerCount() const noexcept
    {
        std::lock_guard lock(g_stateMutex);
        return g_state.ledger.size();
    }

    size_t CoSaveSerializer::BookCount() const noexcept
    {
        std::lock_guard lock(g_stateMutex);
        return g_state.books.size();
    }

    uint32_t CoSaveSerializer::BooksFullyStudiedCount() const noexcept
    {
        std::lock_guard lock(g_stateMutex);
        return g_state.sign.booksFullyStudiedCount;
    }
} // namespace TrueGaze::ExpandedPlay
