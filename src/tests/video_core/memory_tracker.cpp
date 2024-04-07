// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>
#include <stdexcept>
#include <unordered_map>

#include <catch2/catch_test_macros.hpp>

#include "common/alignment.h"
#include "common/common_types.h"
#include "video_core/buffer_cache/memory_tracker_base.h"

namespace {
using Range = std::pair<u64, u64>;

constexpr u64 PAGE = 4096;
constexpr u64 WORD = 4096 * 64;
constexpr u64 HIGH_PAGE_BITS = 22;
constexpr u64 HIGH_PAGE_SIZE = 1ULL << HIGH_PAGE_BITS;

constexpr VAddr c = 16 * HIGH_PAGE_SIZE;

class RasterizerInterface {
public:
    void UpdatePagesCachedCount(VAddr addr, u64 size, int delta) {
        const u64 page_start{addr >> Core::DEVICE_PAGEBITS};
        const u64 page_end{(addr + size + Core::DEVICE_PAGESIZE - 1) >> Core::DEVICE_PAGEBITS};
        for (u64 page = page_start; page < page_end; ++page) {
            int& value = page_table[page];
            value += delta;
            if (value < 0) {
                throw std::logic_error{"negative page"};
            }
            if (value == 0) {
                page_table.erase(page);
            }
        }
    }

    [[nodiscard]] int Count(VAddr addr) const noexcept {
        const auto it = page_table.find(addr >> Core::DEVICE_PAGEBITS);
        return it == page_table.end() ? 0 : it->second;
    }

    [[nodiscard]] unsigned Count() const noexcept {
        unsigned count = 0;
        for (const auto& [index, value] : page_table) {
            count += value;
        }
        return count;
    }

private:
    std::unordered_map<u64, int> page_table;
};
} // Anonymous namespace

using MemoryTracker = VideoCommon::MemoryTrackerBase<RasterizerInterface>;

TEST_CASE("MemoryTracker: Small region", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    REQUIRE(rasterizer.Count() == 0);
    memory_track->UnmarkRegionAsCpuModified(c, WORD);
    REQUIRE(rasterizer.Count() == WORD / PAGE);
    REQUIRE(memory_track->ModifiedCpuRegion(c, WORD) == Range{0, 0});

    memory_track->MarkRegionAsCpuModified(c + PAGE, 1);
    REQUIRE(memory_track->ModifiedCpuRegion(c, WORD) == Range{c + PAGE * 1, c + PAGE * 2});
}

TEST_CASE("MemoryTracker: Large region", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD * 32);
    memory_track->MarkRegionAsCpuModified(c + 4096, WORD * 4);
    REQUIRE(memory_track->ModifiedCpuRegion(c, WORD + PAGE * 2) ==
            Range{c + PAGE, c + WORD + PAGE * 2});
    REQUIRE(memory_track->ModifiedCpuRegion(c + PAGE * 2, PAGE * 6) ==
            Range{c + PAGE * 2, c + PAGE * 8});
    REQUIRE(memory_track->ModifiedCpuRegion(c, WORD * 32) == Range{c + PAGE, c + WORD * 4 + PAGE});
    REQUIRE(memory_track->ModifiedCpuRegion(c + WORD * 4, PAGE) ==
            Range{c + WORD * 4, c + WORD * 4 + PAGE});
    REQUIRE(memory_track->ModifiedCpuRegion(c + WORD * 3 + PAGE * 63, PAGE) ==
            Range{c + WORD * 3 + PAGE * 63, c + WORD * 4});

    memory_track->MarkRegionAsCpuModified(c + WORD * 5 + PAGE * 6, PAGE);
    memory_track->MarkRegionAsCpuModified(c + WORD * 5 + PAGE * 8, PAGE);
    REQUIRE(memory_track->ModifiedCpuRegion(c + WORD * 5, WORD) ==
            Range{c + WORD * 5 + PAGE * 6, c + WORD * 5 + PAGE * 9});

    memory_track->UnmarkRegionAsCpuModified(c + WORD * 5 + PAGE * 8, PAGE);
    REQUIRE(memory_track->ModifiedCpuRegion(c + WORD * 5, WORD) ==
            Range{c + WORD * 5 + PAGE * 6, c + WORD * 5 + PAGE * 7});

    memory_track->MarkRegionAsCpuModified(c + PAGE, WORD * 31 + PAGE * 63);
    REQUIRE(memory_track->ModifiedCpuRegion(c, WORD * 32) == Range{c + PAGE, c + WORD * 32});

    memory_track->UnmarkRegionAsCpuModified(c + PAGE * 4, PAGE);
    memory_track->UnmarkRegionAsCpuModified(c + PAGE * 6, PAGE);

    memory_track->UnmarkRegionAsCpuModified(c, WORD * 32);
    REQUIRE(memory_track->ModifiedCpuRegion(c, WORD * 32) == Range{0, 0});
}

TEST_CASE("MemoryTracker: Rasterizer counting", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    REQUIRE(rasterizer.Count() == 0);
    memory_track->UnmarkRegionAsCpuModified(c, PAGE);
    REQUIRE(rasterizer.Count() == 1);
    memory_track->MarkRegionAsCpuModified(c, PAGE * 2);
    REQUIRE(rasterizer.Count() == 0);
    memory_track->UnmarkRegionAsCpuModified(c, PAGE);
    memory_track->UnmarkRegionAsCpuModified(c + PAGE, PAGE);
    REQUIRE(rasterizer.Count() == 2);
    memory_track->MarkRegionAsCpuModified(c, PAGE * 2);
    REQUIRE(rasterizer.Count() == 0);
}

TEST_CASE("MemoryTracker: Basic range", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD);
    memory_track->MarkRegionAsCpuModified(c, PAGE);
    int num = 0;
    memory_track->ForEachUploadRange(c, WORD, [&](u64 offset, u64 size) {
        REQUIRE(offset == c);
        REQUIRE(size == PAGE);
        ++num;
    });
    REQUIRE(num == 1U);
}

TEST_CASE("MemoryTracker: Border upload", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD * 2);
    memory_track->MarkRegionAsCpuModified(c + WORD - PAGE, PAGE * 2);
    memory_track->ForEachUploadRange(c, WORD * 2, [](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD - PAGE);
        REQUIRE(size == PAGE * 2);
    });
}

TEST_CASE("MemoryTracker: Border upload range", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD * 2);
    memory_track->MarkRegionAsCpuModified(c + WORD - PAGE, PAGE * 2);
    memory_track->ForEachUploadRange(c + WORD - PAGE, PAGE * 2, [](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD - PAGE);
        REQUIRE(size == PAGE * 2);
    });
    memory_track->MarkRegionAsCpuModified(c + WORD - PAGE, PAGE * 2);
    memory_track->ForEachUploadRange(c + WORD - PAGE, PAGE, [](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD - PAGE);
        REQUIRE(size == PAGE);
    });
    memory_track->ForEachUploadRange(c + WORD, PAGE, [](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD);
        REQUIRE(size == PAGE);
    });
}

TEST_CASE("MemoryTracker: Border upload partial range", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD * 2);
    memory_track->MarkRegionAsCpuModified(c + WORD - PAGE, PAGE * 2);
    memory_track->ForEachUploadRange(c + WORD - 1, 2, [](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD - PAGE);
        REQUIRE(size == PAGE * 2);
    });
    memory_track->MarkRegionAsCpuModified(c + WORD - PAGE, PAGE * 2);
    memory_track->ForEachUploadRange(c + WORD - 1, 1, [](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD - PAGE);
        REQUIRE(size == PAGE);
    });
    memory_track->ForEachUploadRange(c + WORD + 50, 1, [](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD);
        REQUIRE(size == PAGE);
    });
}

TEST_CASE("MemoryTracker: Partial word uploads", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    int num = 0;
    memory_track->ForEachUploadRange(c, WORD, [&](u64 offset, u64 size) {
        REQUIRE(offset == c);
        REQUIRE(size == WORD);
        ++num;
    });
    REQUIRE(num == 1);
    memory_track->ForEachUploadRange(c + WORD, WORD, [&](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD);
        REQUIRE(size == WORD);
        ++num;
    });
    REQUIRE(num == 2);
    memory_track->ForEachUploadRange(c + 0x79000, 0x24000, [&](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD * 2);
        REQUIRE(size == PAGE * 0x1d);
        ++num;
    });
    REQUIRE(num == 3);
}

TEST_CASE("MemoryTracker: Partial page upload", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD);
    int num = 0;
    memory_track->MarkRegionAsCpuModified(c + PAGE * 2, PAGE);
    memory_track->MarkRegionAsCpuModified(c + PAGE * 9, PAGE);
    memory_track->ForEachUploadRange(c, PAGE * 3, [&](u64 offset, u64 size) {
        REQUIRE(offset == c + PAGE * 2);
        REQUIRE(size == PAGE);
        ++num;
    });
    REQUIRE(num == 1);
    memory_track->ForEachUploadRange(c + PAGE * 7, PAGE * 3, [&](u64 offset, u64 size) {
        REQUIRE(offset == c + PAGE * 9);
        REQUIRE(size == PAGE);
        ++num;
    });
    REQUIRE(num == 2);
}

TEST_CASE("MemoryTracker: Partial page upload with multiple words on the right") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD * 9);
    memory_track->MarkRegionAsCpuModified(c + PAGE * 13, WORD * 7);
    int num = 0;
    memory_track->ForEachUploadRange(c + PAGE * 10, WORD * 7, [&](u64 offset, u64 size) {
        REQUIRE(offset == c + PAGE * 13);
        REQUIRE(size == WORD * 7 - PAGE * 3);
        ++num;
    });
    REQUIRE(num == 1);
    memory_track->ForEachUploadRange(c + PAGE, WORD * 8, [&](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD * 7 + PAGE * 10);
        REQUIRE(size == PAGE * 3);
        ++num;
    });
    REQUIRE(num == 2);
}

TEST_CASE("MemoryTracker: Partial page upload with multiple words on the left", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD * 8);
    memory_track->MarkRegionAsCpuModified(c + PAGE * 13, WORD * 7);
    int num = 0;
    memory_track->ForEachUploadRange(c + PAGE * 16, WORD * 7, [&](u64 offset, u64 size) {
        REQUIRE(offset == c + PAGE * 16);
        REQUIRE(size == WORD * 7 - PAGE * 3);
        ++num;
    });
    REQUIRE(num == 1);
    memory_track->ForEachUploadRange(c + PAGE, WORD, [&](u64 offset, u64 size) {
        REQUIRE(offset == c + PAGE * 13);
        REQUIRE(size == PAGE * 3);
        ++num;
    });
    REQUIRE(num == 2);
}

TEST_CASE("MemoryTracker: Partial page upload with multiple words in the middle", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD * 8);
    memory_track->MarkRegionAsCpuModified(c + PAGE * 13, PAGE * 140);
    int num = 0;
    memory_track->ForEachUploadRange(c + PAGE * 16, WORD, [&](u64 offset, u64 size) {
        REQUIRE(offset == c + PAGE * 16);
        REQUIRE(size == WORD);
        ++num;
    });
    REQUIRE(num == 1);
    memory_track->ForEachUploadRange(c, WORD, [&](u64 offset, u64 size) {
        REQUIRE(offset == c + PAGE * 13);
        REQUIRE(size == PAGE * 3);
        ++num;
    });
    REQUIRE(num == 2);
    memory_track->ForEachUploadRange(c, WORD * 8, [&](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD + PAGE * 16);
        REQUIRE(size == PAGE * 73);
        ++num;
    });
    REQUIRE(num == 3);
}

TEST_CASE("MemoryTracker: Empty right bits", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD * 2048);
    memory_track->MarkRegionAsCpuModified(c + WORD - PAGE, PAGE * 2);
    memory_track->ForEachUploadRange(c, WORD * 2048, [](u64 offset, u64 size) {
        REQUIRE(offset == c + WORD - PAGE);
        REQUIRE(size == PAGE * 2);
    });
}

TEST_CASE("MemoryTracker: Out of bound ranges 1", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c - WORD, 3 * WORD);
    memory_track->MarkRegionAsCpuModified(c, PAGE);
    REQUIRE(rasterizer.Count() == (3 * WORD - PAGE) / PAGE);
    int num = 0;
    memory_track->ForEachUploadRange(c - WORD, WORD, [&](u64 offset, u64 size) { ++num; });
    memory_track->ForEachUploadRange(c + WORD, WORD, [&](u64 offset, u64 size) { ++num; });
    memory_track->ForEachUploadRange(c - PAGE, PAGE, [&](u64 offset, u64 size) { ++num; });
    REQUIRE(num == 0);
    memory_track->ForEachUploadRange(c - PAGE, PAGE * 2, [&](u64 offset, u64 size) { ++num; });
    REQUIRE(num == 1);
    memory_track->MarkRegionAsCpuModified(c, WORD);
    REQUIRE(rasterizer.Count() == 2 * WORD / PAGE);
}

TEST_CASE("MemoryTracker: Out of bound ranges 2", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    REQUIRE_NOTHROW(memory_track->UnmarkRegionAsCpuModified(c + 0x22000, PAGE));
    REQUIRE_NOTHROW(memory_track->UnmarkRegionAsCpuModified(c + 0x28000, PAGE));
    REQUIRE(rasterizer.Count() == 2);
    REQUIRE_NOTHROW(memory_track->UnmarkRegionAsCpuModified(c + 0x21100, PAGE - 0x100));
    REQUIRE(rasterizer.Count() == 3);
    REQUIRE_NOTHROW(memory_track->UnmarkRegionAsCpuModified(c - PAGE, PAGE * 2));
    memory_track->UnmarkRegionAsCpuModified(c - PAGE * 3, PAGE * 2);
    memory_track->UnmarkRegionAsCpuModified(c - PAGE * 2, PAGE * 2);
    REQUIRE(rasterizer.Count() == 7);
}

TEST_CASE("MemoryTracker: Out of bound ranges 3", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, 0x310720);
    REQUIRE(rasterizer.Count(c) == 1);
    REQUIRE(rasterizer.Count(c + PAGE) == 1);
    REQUIRE(rasterizer.Count(c + WORD) == 1);
    REQUIRE(rasterizer.Count(c + WORD + PAGE) == 1);
}

TEST_CASE("MemoryTracker: Sparse regions 1", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD);
    memory_track->MarkRegionAsCpuModified(c + PAGE * 1, PAGE);
    memory_track->MarkRegionAsCpuModified(c + PAGE * 3, PAGE * 4);
    memory_track->ForEachUploadRange(c, WORD, [i = 0](u64 offset, u64 size) mutable {
        static constexpr std::array<u64, 2> offsets{c + PAGE, c + PAGE * 3};
        static constexpr std::array<u64, 2> sizes{PAGE, PAGE * 4};
        REQUIRE(offset == offsets.at(i));
        REQUIRE(size == sizes.at(i));
        ++i;
    });
}

TEST_CASE("MemoryTracker: Sparse regions 2", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, PAGE * 0x23);
    REQUIRE(rasterizer.Count() == 0x23);
    memory_track->MarkRegionAsCpuModified(c + PAGE * 0x1B, PAGE);
    memory_track->MarkRegionAsCpuModified(c + PAGE * 0x21, PAGE);
    memory_track->ForEachUploadRange(c, PAGE * 0x23, [i = 0](u64 offset, u64 size) mutable {
        static constexpr std::array<u64, 3> offsets{c + PAGE * 0x1B, c + PAGE * 0x21};
        static constexpr std::array<u64, 3> sizes{PAGE, PAGE};
        REQUIRE(offset == offsets.at(i));
        REQUIRE(size == sizes.at(i));
        ++i;
    });
}

TEST_CASE("MemoryTracker: Single page modified range", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    REQUIRE(memory_track->IsRegionCpuModified(c, PAGE));
    memory_track->UnmarkRegionAsCpuModified(c, PAGE);
    REQUIRE(!memory_track->IsRegionCpuModified(c, PAGE));
}

TEST_CASE("MemoryTracker: Two page modified range", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    REQUIRE(memory_track->IsRegionCpuModified(c, PAGE));
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    REQUIRE(memory_track->IsRegionCpuModified(c, PAGE * 2));
    memory_track->UnmarkRegionAsCpuModified(c, PAGE);
    REQUIRE(!memory_track->IsRegionCpuModified(c, PAGE));
}

TEST_CASE("MemoryTracker: Multi word modified ranges", "[video_core]") {
    for (int offset = 0; offset < 4; ++offset) {
        const VAddr address = c + WORD * offset;
        RasterizerInterface rasterizer;
        std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
        REQUIRE(memory_track->IsRegionCpuModified(address, PAGE));
        REQUIRE(memory_track->IsRegionCpuModified(address + PAGE * 48, PAGE));
        REQUIRE(memory_track->IsRegionCpuModified(address + PAGE * 56, PAGE));

        memory_track->UnmarkRegionAsCpuModified(address + PAGE * 32, PAGE);
        REQUIRE(memory_track->IsRegionCpuModified(address + PAGE, WORD));
        REQUIRE(memory_track->IsRegionCpuModified(address + PAGE * 31, PAGE));
        REQUIRE(!memory_track->IsRegionCpuModified(address + PAGE * 32, PAGE));
        REQUIRE(memory_track->IsRegionCpuModified(address + PAGE * 33, PAGE));
        REQUIRE(memory_track->IsRegionCpuModified(address + PAGE * 31, PAGE * 2));
        REQUIRE(memory_track->IsRegionCpuModified(address + PAGE * 32, PAGE * 2));

        memory_track->UnmarkRegionAsCpuModified(address + PAGE * 33, PAGE);
        REQUIRE(!memory_track->IsRegionCpuModified(address + PAGE * 32, PAGE * 2));
    }
}

TEST_CASE("MemoryTracker: Single page in large region", "[video_core]") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD * 16);
    REQUIRE(!memory_track->IsRegionCpuModified(c, WORD * 16));

    memory_track->MarkRegionAsCpuModified(c + WORD * 12 + PAGE * 8, PAGE);
    REQUIRE(memory_track->IsRegionCpuModified(c, WORD * 16));
    REQUIRE(!memory_track->IsRegionCpuModified(c + WORD * 10, WORD * 2));
    REQUIRE(memory_track->IsRegionCpuModified(c + WORD * 11, WORD * 2));
    REQUIRE(memory_track->IsRegionCpuModified(c + WORD * 12, WORD * 2));
    REQUIRE(memory_track->IsRegionCpuModified(c + WORD * 12 + PAGE * 4, PAGE * 8));
    REQUIRE(memory_track->IsRegionCpuModified(c + WORD * 12 + PAGE * 6, PAGE * 8));
    REQUIRE(!memory_track->IsRegionCpuModified(c + WORD * 12 + PAGE * 6, PAGE));
    REQUIRE(memory_track->IsRegionCpuModified(c + WORD * 12 + PAGE * 7, PAGE * 2));
    REQUIRE(memory_track->IsRegionCpuModified(c + WORD * 12 + PAGE * 8, PAGE * 2));
}

TEST_CASE("MemoryTracker: Wrap word regions") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD * 32);
    memory_track->MarkRegionAsCpuModified(c + PAGE * 63, PAGE * 2);
    REQUIRE(memory_track->IsRegionCpuModified(c, WORD * 2));
    REQUIRE(!memory_track->IsRegionCpuModified(c + PAGE * 62, PAGE));
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE * 63, PAGE));
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE * 64, PAGE));
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE * 63, PAGE * 2));
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE * 63, PAGE * 8));
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE * 60, PAGE * 8));

    REQUIRE(!memory_track->IsRegionCpuModified(c + PAGE * 127, WORD * 16));
    memory_track->MarkRegionAsCpuModified(c + PAGE * 127, PAGE);
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE * 127, WORD * 16));
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE * 127, PAGE));
    REQUIRE(!memory_track->IsRegionCpuModified(c + PAGE * 126, PAGE));
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE * 126, PAGE * 2));
    REQUIRE(!memory_track->IsRegionCpuModified(c + PAGE * 128, WORD * 16));
}

TEST_CASE("MemoryTracker: Unaligned page region query") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD);
    memory_track->MarkRegionAsCpuModified(c + 4000, 1000);
    REQUIRE(memory_track->IsRegionCpuModified(c, PAGE));
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    REQUIRE(memory_track->IsRegionCpuModified(c + 4000, 1000));
    REQUIRE(memory_track->IsRegionCpuModified(c + 4000, 1));
}

TEST_CASE("MemoryTracker: Cached write") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD);
    memory_track->CachedCpuWrite(c + PAGE, c + PAGE);
    REQUIRE(!memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    memory_track->FlushCachedWrites();
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    memory_track->MarkRegionAsCpuModified(c, WORD);
    REQUIRE(rasterizer.Count() == 0);
}

TEST_CASE("MemoryTracker: Multiple cached write") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD);
    memory_track->CachedCpuWrite(c + PAGE, PAGE);
    memory_track->CachedCpuWrite(c + PAGE * 3, PAGE);
    REQUIRE(!memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    REQUIRE(!memory_track->IsRegionCpuModified(c + PAGE * 3, PAGE));
    memory_track->FlushCachedWrites();
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE * 3, PAGE));
    memory_track->MarkRegionAsCpuModified(c, WORD);
    REQUIRE(rasterizer.Count() == 0);
}

TEST_CASE("MemoryTracker: Cached write unmarked") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD);
    memory_track->CachedCpuWrite(c + PAGE, PAGE);
    memory_track->UnmarkRegionAsCpuModified(c + PAGE, PAGE);
    REQUIRE(!memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    memory_track->FlushCachedWrites();
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    memory_track->MarkRegionAsCpuModified(c, WORD);
    REQUIRE(rasterizer.Count() == 0);
}

TEST_CASE("MemoryTracker: Cached write iterated") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD);
    memory_track->CachedCpuWrite(c + PAGE, PAGE);
    int num = 0;
    memory_track->ForEachUploadRange(c, WORD, [&](u64 offset, u64 size) { ++num; });
    REQUIRE(num == 0);
    REQUIRE(!memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    memory_track->FlushCachedWrites();
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    memory_track->MarkRegionAsCpuModified(c, WORD);
    REQUIRE(rasterizer.Count() == 0);
}

TEST_CASE("MemoryTracker: Cached write downloads") {
    RasterizerInterface rasterizer;
    std::unique_ptr<MemoryTracker> memory_track(std::make_unique<MemoryTracker>(rasterizer));
    memory_track->UnmarkRegionAsCpuModified(c, WORD);
    REQUIRE(rasterizer.Count() == 64);
    memory_track->CachedCpuWrite(c + PAGE, PAGE);
    REQUIRE(rasterizer.Count() == 63);
    memory_track->MarkRegionAsGpuModified(c + PAGE, PAGE);
    int num = 0;
    memory_track->ForEachDownloadRangeAndClear(c, WORD, [&](u64 offset, u64 size) { ++num; });
    REQUIRE(num == 0);
    num = 0;
    memory_track->ForEachUploadRange(c, WORD, [&](u64 offset, u64 size) { ++num; });
    REQUIRE(num == 0);
    REQUIRE(!memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    REQUIRE(memory_track->IsRegionGpuModified(c + PAGE, PAGE));
    memory_track->FlushCachedWrites();
    REQUIRE(memory_track->IsRegionCpuModified(c + PAGE, PAGE));
    REQUIRE(!memory_track->IsRegionGpuModified(c + PAGE, PAGE));
    memory_track->MarkRegionAsCpuModified(c, WORD);
    REQUIRE(rasterizer.Count() == 0);
}
