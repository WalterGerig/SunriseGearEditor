#include "family0_source_seed.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../../../core/logging/log.h"

namespace sunrise::client::hooks::queuez::family0 {
namespace {

/** Fields of the queuez manager this hook reads. */
struct ManagerLayout {
    /** The account key the sweep itself passes to every family subscribe. */
    static constexpr std::size_t accountKey = 16;
};

/** Fields of the family-zero source list. The sweep diffs the whole fixed-size record. */
struct SourceListLayout {
    /**
     * Members the producer counted with flags bit 0 or bit 4. Bit 4 is set by every upsert, so
     * this moves even when nothing is emitted.
     */
    static constexpr std::size_t countA = 0;
    /**
     * Entries the producer wrote with a non-zero soid. It moves once per emitted entry, so it is
     * the exact length.
     */
    static constexpr std::size_t countB = 8;
    /** First entry, {u64 key, u16 mask}. */
    static constexpr std::size_t firstKey = 16;
    static constexpr std::size_t firstMask = 24;
    /** Second entry. Two entries naming one soid is what gives the family two records. */
    static constexpr std::size_t secondKey = 32;
    static constexpr std::size_t secondMask = 40;
};

/**
 * The smallest entry mask that passes the sweep's own filter without claiming the family-zero
 * subscription is already synced. That synced state is what the subscribe must reach.
 */
constexpr std::uint16_t kEntryMask = 1;
/** One seeded entry: one record per source key, and one is all the account needs. */
constexpr std::uint64_t kSeededCount = 1;
/** Bytes of the source list the producer owns, rebuilt whole so no stale entry survives. */
constexpr std::size_t kSourceListBytes = 0x210;

/** Shortest gap between two seed reports. The sweep runs per frame, so this bounds the volume. */
constexpr std::uint64_t kReportIntervalMs = 1'000;
/** One line carries the replaced key and the rewrite count. */
constexpr std::size_t kReportLimit = 128;
/** One line carries both counters and the first two entries. */
constexpr std::size_t kListReportLimit = 192;

/** The producer's own output, as the seed found it before writing anything. */
struct ListHead {
    std::uint64_t countA{};
    std::uint64_t countB{};
    std::uint64_t firstKey{};
    std::uint64_t secondKey{};
    std::uint16_t firstMask{};
    std::uint16_t secondMask{};
};

/** @return True when both heads hold the same values. */
[[nodiscard]] bool equal(const ListHead& first, const ListHead& second) noexcept {
    return first.countA == second.countA && first.countB == second.countB
           && first.firstKey == second.firstKey && first.secondKey == second.secondKey
           && first.firstMask == second.firstMask && first.secondMask == second.secondMask;
}

std::atomic<SourceList> g_sourceList{nullptr};
/** The account key, read from the manager rather than written by us, so it cannot disagree. */
std::atomic<std::uint64_t> g_accountKey{0};
/** Total rewrites. A rewrite means something else put its own list back. */
std::atomic<std::uint64_t> g_rewrites{0};
std::atomic<std::uint64_t> g_reportDueTick{0};
/** The last head reported, so the producer's output is logged on change rather than per frame. */
ListHead g_lastHead{};
bool g_hasLastHead{false};

/**
 * Reports the producer's own output whenever it changes.
 * @param head Source-list head as the seed found it.
 */
void report_list(const ListHead& head) noexcept {
    if (g_hasLastHead && equal(head, g_lastHead)) {
        return;
    }
    g_lastHead = head;
    g_hasLastHead = true;
    std::array<char, kListReportLimit> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=queuez stage=family0_list countA=%llu countB=%llu "
                                      "first=0x%016llX/%u second=0x%016llX/%u",
                                      static_cast<unsigned long long>(head.countA),
                                      static_cast<unsigned long long>(head.countB),
                                      static_cast<unsigned long long>(head.firstKey),
                                      static_cast<unsigned>(head.firstMask),
                                      static_cast<unsigned long long>(head.secondKey),
                                      static_cast<unsigned>(head.secondMask));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/**
 * Copies the source-list head the producer left behind.
 * @param list Borrowed source list.
 * @return Its counters and first two entries.
 */
[[nodiscard]] ListHead read_head(const std::byte* list) noexcept {
    ListHead head{};
    std::memcpy(&head.countA, list + SourceListLayout::countA, sizeof head.countA);
    std::memcpy(&head.countB, list + SourceListLayout::countB, sizeof head.countB);
    std::memcpy(&head.firstKey, list + SourceListLayout::firstKey, sizeof head.firstKey);
    std::memcpy(&head.firstMask, list + SourceListLayout::firstMask, sizeof head.firstMask);
    std::memcpy(&head.secondKey, list + SourceListLayout::secondKey, sizeof head.secondKey);
    std::memcpy(&head.secondMask, list + SourceListLayout::secondMask, sizeof head.secondMask);
    return head;
}

/**
 * Reports how often the seed is having to put the account key back.
 * A steady rewrite rate means the game's own producer is rebuilding the list underneath.
 * @param replaced Key that was in the list before this rewrite.
 */
void report_seed(std::uint64_t replaced) noexcept {
    const std::uint64_t count = g_rewrites.fetch_add(1, std::memory_order_relaxed) + 1;
    const std::uint64_t now = GetTickCount64();
    if (now < g_reportDueTick.load(std::memory_order_relaxed)) {
        return;
    }
    g_reportDueTick.store(now + kReportIntervalMs, std::memory_order_relaxed);
    std::array<char, kReportLimit> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=queuez stage=family0 result=seeded replaced=0x%016llX "
                                      "rewrites=%llu",
                                      static_cast<unsigned long long>(replaced),
                                      static_cast<unsigned long long>(count));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

} // namespace

/** @param getter The source-list getter we found, or null to withdraw it. */
void publish_source_list(SourceList getter) noexcept {
    g_sourceList.store(getter, std::memory_order_release);
}

/** Captures the account key the source-list seed needs. */
void learn_account_key(std::byte* manager) noexcept {
    if (manager == nullptr || g_accountKey.load(std::memory_order_acquire) != 0) {
        return;
    }
    std::uint64_t key = 0;
    std::memcpy(&key, manager + ManagerLayout::accountKey, sizeof key);
    if (key != 0) {
        g_accountKey.store(key, std::memory_order_release);
    }
}

/** Seeds the source list with the account key. Runs every tick, writes only when it differs. */
void seed_source_list() noexcept {
    const SourceList source = g_sourceList.load(std::memory_order_acquire);
    const std::uint64_t key = g_accountKey.load(std::memory_order_acquire);
    if (source == nullptr || key == 0) {
        return;
    }
    std::byte* const list = source();
    if (list == nullptr) {
        return;
    }
    // Read before the early return, so the producer's own output is visible once it starts working.
    const ListHead head = read_head(list);
    report_list(head);
    // Both counters have to agree, not just the first key. A counter mismatch arms a boot
    // deadline in the sweep, and rewriting the whole buffer clears it.
    if (head.firstKey == key && head.countA == kSeededCount && head.countB == kSeededCount
        && head.secondKey == 0) {
        return; // Already exactly ours. Rewriting the same bytes would only churn the subscribe.
    }
    const std::uint64_t existing = head.firstKey;
    // The whole buffer is rebuilt, so an entry the producer left past the first cannot survive
    // as a key the sweep would go on to subscribe.
    std::array<std::byte, kSourceListBytes> seeded{};
    std::memcpy(seeded.data() + SourceListLayout::countA, &kSeededCount, sizeof kSeededCount);
    std::memcpy(seeded.data() + SourceListLayout::countB, &kSeededCount, sizeof kSeededCount);
    std::memcpy(seeded.data() + SourceListLayout::firstKey, &key, sizeof key);
    std::memcpy(seeded.data() + SourceListLayout::firstMask, &kEntryMask, sizeof kEntryMask);
    std::memcpy(list, seeded.data(), seeded.size());
    report_seed(existing);
}

/** Clears the captured key, the published getter, and the report state. */
void reset() noexcept {
    g_sourceList.store(nullptr, std::memory_order_release);
    g_accountKey.store(0, std::memory_order_release);
    g_rewrites.store(0, std::memory_order_release);
    g_reportDueTick.store(0, std::memory_order_release);
    g_lastHead = {};
    g_hasLastHead = false;
}

} // namespace sunrise::client::hooks::queuez::family0
