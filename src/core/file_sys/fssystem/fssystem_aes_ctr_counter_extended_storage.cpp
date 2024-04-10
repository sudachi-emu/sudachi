// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/file_sys/fssystem/fssystem_aes_ctr_counter_extended_storage.h"
#include "core/file_sys/fssystem/fssystem_aes_ctr_storage.h"
#include "core/file_sys/fssystem/fssystem_nca_header.h"
#include "core/file_sys/vfs/vfs_offset.h"

namespace FileSys {

namespace {

class SoftwareDecryptor final : public AesCtrCounterExtendedStorage::IDecryptor {
public:
    virtual void Decrypt(
        u8* buf, size_t buf_size, const std::array<u8, AesCtrCounterExtendedStorage::KeySize>& key,
        const std::array<u8, AesCtrCounterExtendedStorage::IvSize>& iv) override final;
};

} // namespace

Result AesCtrCounterExtendedStorage::CreateSoftwareDecryptor(std::unique_ptr<IDecryptor>* out) {
    std::unique_ptr<IDecryptor> decryptor = std::make_unique<SoftwareDecryptor>();
    R_UNLESS(decryptor != nullptr, ResultAllocationMemoryFailedInAesCtrCounterExtendedStorageA);
    *out = std::move(decryptor);
    R_SUCCEED();
}

Result AesCtrCounterExtendedStorage::Initialize(const void* key, size_t key_size, u32 secure_value,
                                                VirtualFile data_storage,
                                                VirtualFile table_storage) {
    // Read and verify the bucket tree header.
    BucketTree::Header header;
    table_storage->ReadObject(std::addressof(header), 0);
    R_TRY(header.Verify());

    // Determine extents.
    const auto node_storage_size = QueryNodeStorageSize(header.entry_count);
    const auto entry_storage_size = QueryEntryStorageSize(header.entry_count);
    const auto node_storage_offset = QueryHeaderStorageSize();
    const auto entry_storage_offset = node_storage_offset + node_storage_size;

    // Create a software decryptor.
    std::unique_ptr<IDecryptor> sw_decryptor;
    R_TRY(CreateSoftwareDecryptor(std::addressof(sw_decryptor)));

    // Initialize.
    R_RETURN(this->Initialize(
        key, key_size, secure_value, 0, data_storage,
        std::make_shared<OffsetVfsFile>(table_storage, node_storage_size, node_storage_offset),
        std::make_shared<OffsetVfsFile>(table_storage, entry_storage_size, entry_storage_offset),
        header.entry_count, std::move(sw_decryptor)));
}

Result AesCtrCounterExtendedStorage::Initialize(const void* key, size_t key_size, u32 secure_value,
                                                s64 counter_offset, VirtualFile data_storage,
                                                VirtualFile node_storage, VirtualFile entry_storage,
                                                s32 entry_count,
                                                std::unique_ptr<IDecryptor>&& decryptor) {
    // Validate preconditions.
    ASSERT(key != nullptr);
    ASSERT(key_size == KeySize);
    ASSERT(counter_offset >= 0);
    ASSERT(decryptor != nullptr);

    // Initialize the bucket tree table.
    if (entry_count > 0) {
        R_TRY(
            m_table.Initialize(node_storage, entry_storage, NodeSize, sizeof(Entry), entry_count));
    } else {
        m_table.Initialize(NodeSize, 0);
    }

    // Set members.
    m_data_storage = data_storage;
    std::memcpy(m_key.data(), key, key_size);
    m_secure_value = secure_value;
    m_counter_offset = counter_offset;
    m_decryptor = std::move(decryptor);

    R_SUCCEED();
}

void AesCtrCounterExtendedStorage::Finalize() {
    if (this->IsInitialized()) {
        m_table.Finalize();
        m_data_storage = VirtualFile();
    }
}

Result AesCtrCounterExtendedStorage::GetEntryList(Entry* out_entries, s32* out_entry_count,
                                                  s32 entry_count, s64 offset, s64 size) {
    // Validate pre-conditions.
    ASSERT(offset >= 0);
    ASSERT(size >= 0);
    ASSERT(this->IsInitialized());

    // Clear the out count.
    R_UNLESS(out_entry_count != nullptr, ResultNullptrArgument);
    *out_entry_count = 0;

    // Succeed if there's no range.
    R_SUCCEED_IF(size == 0);

    // If we have an output array, we need it to be non-null.
    R_UNLESS(out_entries != nullptr || entry_count == 0, ResultNullptrArgument);

    // Check that our range is valid.
    BucketTree::Offsets table_offsets;
    R_TRY(m_table.GetOffsets(std::addressof(table_offsets)));

    R_UNLESS(table_offsets.IsInclude(offset, size), ResultOutOfRange);

    // Find the offset in our tree.
    BucketTree::Visitor visitor;
    R_TRY(m_table.Find(std::addressof(visitor), offset));
    {
        const auto entry_offset = visitor.Get<Entry>()->GetOffset();
        R_UNLESS(0 <= entry_offset && table_offsets.IsInclude(entry_offset),
                 ResultInvalidAesCtrCounterExtendedEntryOffset);
    }

    // Prepare to loop over entries.
    const auto end_offset = offset + static_cast<s64>(size);
    s32 count = 0;

    auto cur_entry = *visitor.Get<Entry>();
    while (cur_entry.GetOffset() < end_offset) {
        // Try to write the entry to the out list.
        if (entry_count != 0) {
            if (count >= entry_count) {
                break;
            }
            std::memcpy(out_entries + count, std::addressof(cur_entry), sizeof(Entry));
        }

        count++;

        // Advance.
        if (visitor.CanMoveNext()) {
            R_TRY(visitor.MoveNext());
            cur_entry = *visitor.Get<Entry>();
        } else {
            break;
        }
    }

    // Write the output count.
    *out_entry_count = count;
    R_SUCCEED();
}

size_t AesCtrCounterExtendedStorage::Read(u8* buffer, size_t size, size_t offset) const {
    // Validate preconditions.
    ASSERT(this->IsInitialized());

    // Allow zero size.
    if (size == 0) {
        return size;
    }

    // Validate arguments.
    ASSERT(buffer != nullptr);
    ASSERT(Common::IsAligned(offset, BlockSize));
    ASSERT(Common::IsAligned(size, BlockSize));

    BucketTree::Offsets table_offsets;
    ASSERT(R_SUCCEEDED(m_table.GetOffsets(std::addressof(table_offsets))));

    ASSERT(table_offsets.IsInclude(offset, size));

    // Read the data.
    m_data_storage->Read(buffer, size, offset);

    // Find the offset in our tree.
    BucketTree::Visitor visitor;
    ASSERT(R_SUCCEEDED(m_table.Find(std::addressof(visitor), offset)));
    {
        const auto entry_offset = visitor.Get<Entry>()->GetOffset();
        ASSERT(Common::IsAligned(entry_offset, BlockSize));
        ASSERT(0 <= entry_offset && table_offsets.IsInclude(entry_offset));
    }

    // Prepare to read in chunks.
    u8* cur_data = static_cast<u8*>(buffer);
    auto cur_offset = offset;
    const auto end_offset = offset + static_cast<s64>(size);

    while (cur_offset < end_offset) {
        // Get the current entry.
        const auto cur_entry = *visitor.Get<Entry>();

        // Get and validate the entry's offset.
        const auto cur_entry_offset = cur_entry.GetOffset();
        ASSERT(static_cast<size_t>(cur_entry_offset) <= cur_offset);

        // Get and validate the next entry offset.
        s64 next_entry_offset;
        if (visitor.CanMoveNext()) {
            ASSERT(R_SUCCEEDED(visitor.MoveNext()));
            next_entry_offset = visitor.Get<Entry>()->GetOffset();
            ASSERT(table_offsets.IsInclude(next_entry_offset));
        } else {
            next_entry_offset = table_offsets.end_offset;
        }
        ASSERT(Common::IsAligned(next_entry_offset, BlockSize));
        ASSERT(cur_offset < static_cast<size_t>(next_entry_offset));

        // Get the offset of the entry in the data we read.
        const auto data_offset = cur_offset - cur_entry_offset;
        const auto data_size = (next_entry_offset - cur_entry_offset) - data_offset;
        ASSERT(data_size > 0);

        // Determine how much is left.
        const auto remaining_size = end_offset - cur_offset;
        const auto cur_size = static_cast<size_t>(std::min(remaining_size, data_size));
        ASSERT(cur_size <= size);

        // If necessary, perform decryption.
        if (cur_entry.encryption_value == Entry::Encryption::Encrypted) {
            // Make the CTR for the data we're decrypting.
            const auto counter_offset = m_counter_offset + cur_entry_offset + data_offset;
            NcaAesCtrUpperIv upper_iv = {
                .part = {.generation = static_cast<u32>(cur_entry.generation),
                         .secure_value = m_secure_value}};

            std::array<u8, IvSize> iv;
            AesCtrStorage::MakeIv(iv.data(), IvSize, upper_iv.value, counter_offset);

            // Decrypt.
            m_decryptor->Decrypt(cur_data, cur_size, m_key, iv);
        }

        // Advance.
        cur_data += cur_size;
        cur_offset += cur_size;
    }

    return size;
}

void SoftwareDecryptor::Decrypt(u8* buf, size_t buf_size,
                                const std::array<u8, AesCtrCounterExtendedStorage::KeySize>& key,
                                const std::array<u8, AesCtrCounterExtendedStorage::IvSize>& iv) {
    Core::Crypto::AESCipher<Core::Crypto::Key128, AesCtrCounterExtendedStorage::KeySize> cipher(
        key, Core::Crypto::Mode::CTR);
    cipher.SetIV(iv);
    cipher.Transcode(buf, buf_size, buf, Core::Crypto::Op::Decrypt);
}

} // namespace FileSys
