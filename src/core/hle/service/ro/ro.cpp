// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mbedtls/sha256.h>

#include "common/scope_exit.h"
#include "core/hle/kernel/k_process.h"

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/ro/ro.h"
#include "core/hle/service/ro/ro_nro_utils.h"
#include "core/hle/service/ro/ro_results.h"
#include "core/hle/service/ro/ro_types.h"
#include "core/hle/service/server_manager.h"

namespace Service::RO {

namespace {

// Convenience definitions.
constexpr size_t MaxSessions = 0x3;
constexpr size_t MaxNrrInfos = 0x40;
constexpr size_t MaxNroInfos = 0x40;

constexpr u64 InvalidProcessId = 0xffffffffffffffffULL;
constexpr u64 InvalidContextId = 0xffffffffffffffffULL;

// Types.
using Sha256Hash = std::array<u8, 32>;

struct NroInfo {
    u64 base_address;
    u64 nro_heap_address;
    u64 nro_heap_size;
    u64 bss_heap_address;
    u64 bss_heap_size;
    u64 code_size;
    u64 rw_size;
    ModuleId module_id;
};

struct NrrInfo {
    u64 nrr_heap_address;
    u64 nrr_heap_size;

    // Verification.
    std::vector<Sha256Hash> hashes;
};

struct ProcessContext {
    constexpr ProcessContext() = default;

    void Initialize(Kernel::KProcess* process, u64 process_id) {
        ASSERT(!m_in_use);

        m_nro_in_use = {};
        m_nrr_in_use = {};
        m_nro_infos = {};
        m_nrr_infos = {};

        m_process = process;
        m_process_id = process_id;
        m_in_use = true;

        if (m_process) {
            m_process->Open();
        }
    }

    void Finalize() {
        ASSERT(m_in_use);

        if (m_process) {
            m_process->Close();
        }

        m_nro_in_use = {};
        m_nrr_in_use = {};
        m_nro_infos = {};
        m_nrr_infos = {};

        m_process = nullptr;
        m_process_id = InvalidProcessId;
        m_in_use = false;
    }

    Kernel::KProcess* GetProcess() const {
        return m_process;
    }

    u64 GetProcessId() const {
        return m_process_id;
    }

    bool IsFree() const {
        return !m_in_use;
    }

    u64 GetProgramId(Kernel::KProcess* other_process) const {
        // Automatically select a handle, allowing for override.
        if (other_process) {
            return other_process->GetProgramId();
        } else if (m_process) {
            return m_process->GetProgramId();
        } else {
            return 0;
        }
    }

    Result GetNrrInfoByAddress(NrrInfo** out, u64 nrr_heap_address) {
        for (size_t i = 0; i < MaxNrrInfos; i++) {
            if (m_nrr_in_use[i] && m_nrr_infos[i].nrr_heap_address == nrr_heap_address) {
                if (out != nullptr) {
                    *out = std::addressof(m_nrr_infos[i]);
                }
                R_SUCCEED();
            }
        }
        R_THROW(RO::ResultNotRegistered);
    }

    Result GetFreeNrrInfo(NrrInfo** out) {
        for (size_t i = 0; i < MaxNrrInfos; i++) {
            if (!m_nrr_in_use[i]) {
                if (out != nullptr) {
                    *out = std::addressof(m_nrr_infos[i]);
                }
                R_SUCCEED();
            }
        }
        R_THROW(RO::ResultTooManyNrr);
    }

    Result GetNroInfoByAddress(NroInfo** out, u64 nro_address) {
        for (size_t i = 0; i < MaxNroInfos; i++) {
            if (m_nro_in_use[i] && m_nro_infos[i].base_address == nro_address) {
                if (out != nullptr) {
                    *out = std::addressof(m_nro_infos[i]);
                }
                R_SUCCEED();
            }
        }
        R_THROW(RO::ResultNotLoaded);
    }

    Result GetNroInfoByModuleId(NroInfo** out, const ModuleId* module_id) {
        for (size_t i = 0; i < MaxNroInfos; i++) {
            if (m_nro_in_use[i] && std::memcmp(std::addressof(m_nro_infos[i].module_id), module_id,
                                               sizeof(*module_id)) == 0) {
                if (out != nullptr) {
                    *out = std::addressof(m_nro_infos[i]);
                }
                R_SUCCEED();
            }
        }
        R_THROW(RO::ResultNotLoaded);
    }

    Result GetFreeNroInfo(NroInfo** out) {
        for (size_t i = 0; i < MaxNroInfos; i++) {
            if (!m_nro_in_use[i]) {
                if (out != nullptr) {
                    *out = std::addressof(m_nro_infos[i]);
                }
                R_SUCCEED();
            }
        }
        R_THROW(RO::ResultTooManyNro);
    }

    Result ValidateHasNroHash(u64 base_address, const NroHeader* nro_header) const {
        // Calculate hash.
        Sha256Hash hash;
        {
            const u64 size = nro_header->GetSize();

            std::vector<u8> nro_data(size);
            m_process->GetMemory().ReadBlock(base_address, nro_data.data(), size);

            mbedtls_sha256_ret(nro_data.data(), size, hash.data(), 0);
        }

        for (size_t i = 0; i < MaxNrrInfos; i++) {
            // Ensure we only check NRRs that are used.
            if (!m_nrr_in_use[i]) {
                continue;
            }

            // Locate the hash within the hash list.
            const auto hash_it = std::ranges::find(m_nrr_infos[i].hashes, hash);
            if (hash_it == m_nrr_infos[i].hashes.end()) {
                continue;
            }

            // The hash is valid!
            R_SUCCEED();
        }

        R_THROW(RO::ResultNotAuthorized);
    }

    Result ValidateNro(ModuleId* out_module_id, u64* out_rx_size, u64* out_ro_size,
                       u64* out_rw_size, u64 base_address, u64 expected_nro_size,
                       u64 expected_bss_size) {
        // Ensure we have a process to work on.
        R_UNLESS(m_process != nullptr, RO::ResultInvalidProcess);

        // Read the NRO header.
        NroHeader header{};
        m_process->GetMemory().ReadBlock(base_address, std::addressof(header), sizeof(header));

        // Validate header.
        R_UNLESS(header.IsMagicValid(), RO::ResultInvalidNro);

        // Read sizes from header.
        const u64 nro_size = header.GetSize();
        const u64 text_ofs = header.GetTextOffset();
        const u64 text_size = header.GetTextSize();
        const u64 ro_ofs = header.GetRoOffset();
        const u64 ro_size = header.GetRoSize();
        const u64 rw_ofs = header.GetRwOffset();
        const u64 rw_size = header.GetRwSize();
        const u64 bss_size = header.GetBssSize();

        // Validate sizes meet expected.
        R_UNLESS(nro_size == expected_nro_size, RO::ResultInvalidNro);
        R_UNLESS(bss_size == expected_bss_size, RO::ResultInvalidNro);

        // Validate all sizes are aligned.
        R_UNLESS(Common::IsAligned(text_size, Core::Memory::SUDACHI_PAGESIZE),
                 RO::ResultInvalidNro);
        R_UNLESS(Common::IsAligned(ro_size, Core::Memory::SUDACHI_PAGESIZE), RO::ResultInvalidNro);
        R_UNLESS(Common::IsAligned(rw_size, Core::Memory::SUDACHI_PAGESIZE), RO::ResultInvalidNro);
        R_UNLESS(Common::IsAligned(bss_size, Core::Memory::SUDACHI_PAGESIZE), RO::ResultInvalidNro);

        // Validate sections are in order.
        R_UNLESS(text_ofs <= ro_ofs, RO::ResultInvalidNro);
        R_UNLESS(ro_ofs <= rw_ofs, RO::ResultInvalidNro);

        // Validate sections are sequential and contiguous.
        R_UNLESS(text_ofs == 0, RO::ResultInvalidNro);
        R_UNLESS(text_ofs + text_size == ro_ofs, RO::ResultInvalidNro);
        R_UNLESS(ro_ofs + ro_size == rw_ofs, RO::ResultInvalidNro);
        R_UNLESS(rw_ofs + rw_size == nro_size, RO::ResultInvalidNro);

        // Verify NRO hash.
        R_TRY(this->ValidateHasNroHash(base_address, std::addressof(header)));

        // Check if NRO has already been loaded.
        const ModuleId* module_id = header.GetModuleId();
        R_UNLESS(R_FAILED(this->GetNroInfoByModuleId(nullptr, module_id)), RO::ResultAlreadyLoaded);

        // Apply patches to NRO.
        // LocateAndApplyIpsPatchesToModule(module_id, static_cast<u8*>(mapped_memory), nro_size);

        // Copy to output.
        *out_module_id = *module_id;
        *out_rx_size = text_size;
        *out_ro_size = ro_size;
        *out_rw_size = rw_size;
        R_SUCCEED();
    }

    void SetNrrInfoInUse(const NrrInfo* info, bool in_use) {
        ASSERT(std::addressof(m_nrr_infos[0]) <= info &&
               info <= std::addressof(m_nrr_infos[MaxNrrInfos - 1]));
        const size_t index = info - std::addressof(m_nrr_infos[0]);
        m_nrr_in_use[index] = in_use;
    }

    void SetNroInfoInUse(const NroInfo* info, bool in_use) {
        ASSERT(std::addressof(m_nro_infos[0]) <= info &&
               info <= std::addressof(m_nro_infos[MaxNroInfos - 1]));
        const size_t index = info - std::addressof(m_nro_infos[0]);
        m_nro_in_use[index] = in_use;
    }

private:
    std::array<bool, MaxNroInfos> m_nro_in_use{};
    std::array<bool, MaxNrrInfos> m_nrr_in_use{};
    std::array<NroInfo, MaxNroInfos> m_nro_infos{};
    std::array<NrrInfo, MaxNrrInfos> m_nrr_infos{};
    Kernel::KProcess* m_process{};
    u64 m_process_id{InvalidProcessId};
    bool m_in_use{};
};

Result ValidateAddressAndNonZeroSize(u64 address, u64 size) {
    R_UNLESS(Common::IsAligned(address, Core::Memory::SUDACHI_PAGESIZE), RO::ResultInvalidAddress);
    R_UNLESS(size != 0, RO::ResultInvalidSize);
    R_UNLESS(Common::IsAligned(size, Core::Memory::SUDACHI_PAGESIZE), RO::ResultInvalidSize);
    R_UNLESS(address < address + size, RO::ResultInvalidSize);
    R_SUCCEED();
}

Result ValidateAddressAndSize(u64 address, u64 size) {
    R_UNLESS(Common::IsAligned(address, Core::Memory::SUDACHI_PAGESIZE), RO::ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, Core::Memory::SUDACHI_PAGESIZE), RO::ResultInvalidSize);
    R_UNLESS(size == 0 || address < address + size, RO::ResultInvalidSize);
    R_SUCCEED();
}

class RoContext {
public:
    explicit RoContext() = default;

    Result RegisterProcess(size_t* out_context_id, Kernel::KProcess* process, u64 process_id) {
        // Validate process id.
        R_UNLESS(process->GetProcessId() == process_id, RO::ResultInvalidProcess);

        // Check if a process context already exists.
        R_UNLESS(this->GetContextByProcessId(process_id) == nullptr, RO::ResultInvalidSession);

        // Allocate a context to manage the process handle.
        *out_context_id = this->AllocateContext(process, process_id);

        R_SUCCEED();
    }

    Result ValidateProcess(size_t context_id, u64 process_id) {
        const ProcessContext* ctx = this->GetContextById(context_id);
        R_UNLESS(ctx != nullptr, RO::ResultInvalidProcess);
        R_UNLESS(ctx->GetProcessId() == process_id, RO::ResultInvalidProcess);
        R_SUCCEED();
    }

    void UnregisterProcess(size_t context_id) {
        this->FreeContext(context_id);
    }

    Result RegisterModuleInfo(size_t context_id, u64 nrr_address, u64 nrr_size, NrrKind nrr_kind,
                              bool enforce_nrr_kind) {
        // Get context.
        ProcessContext* context = this->GetContextById(context_id);
        ASSERT(context != nullptr);

        // Validate address/size.
        R_TRY(ValidateAddressAndNonZeroSize(nrr_address, nrr_size));

        // Check we have space for a new NRR.
        NrrInfo* nrr_info = nullptr;
        R_TRY(context->GetFreeNrrInfo(std::addressof(nrr_info)));

        // Ensure we have a valid process to read from.
        Kernel::KProcess* process = context->GetProcess();
        R_UNLESS(process != nullptr, RO::ResultInvalidProcess);

        // Read NRR.
        NrrHeader header{};
        process->GetMemory().ReadBlock(nrr_address, std::addressof(header), sizeof(header));

        // Set NRR info.
        context->SetNrrInfoInUse(nrr_info, true);
        nrr_info->nrr_heap_address = nrr_address;
        nrr_info->nrr_heap_size = nrr_size;

        // Read NRR hash list.
        nrr_info->hashes.resize(header.GetNumHashes());
        process->GetMemory().ReadBlock(nrr_address + header.GetHashesOffset(),
                                       nrr_info->hashes.data(),
                                       sizeof(Sha256Hash) * header.GetNumHashes());

        R_SUCCEED();
    }

    Result UnregisterModuleInfo(size_t context_id, u64 nrr_address) {
        // Get context.
        ProcessContext* context = this->GetContextById(context_id);
        ASSERT(context != nullptr);

        // Validate address.
        R_UNLESS(Common::IsAligned(nrr_address, Core::Memory::SUDACHI_PAGESIZE),
                 RO::ResultInvalidAddress);

        // Check the NRR is loaded.
        NrrInfo* nrr_info = nullptr;
        R_TRY(context->GetNrrInfoByAddress(std::addressof(nrr_info), nrr_address));

        // Nintendo does this unconditionally, whether or not the actual unmap succeeds.
        context->SetNrrInfoInUse(nrr_info, false);
        *nrr_info = {};

        R_SUCCEED();
    }

    Result MapManualLoadModuleMemory(u64* out_address, size_t context_id, u64 nro_address,
                                     u64 nro_size, u64 bss_address, u64 bss_size) {
        // Get context.
        ProcessContext* context = this->GetContextById(context_id);
        ASSERT(context != nullptr);

        // Validate address/size.
        R_TRY(ValidateAddressAndNonZeroSize(nro_address, nro_size));
        R_TRY(ValidateAddressAndSize(bss_address, bss_size));

        const u64 total_size = nro_size + bss_size;
        R_UNLESS(total_size >= nro_size, RO::ResultInvalidSize);
        R_UNLESS(total_size >= bss_size, RO::ResultInvalidSize);

        // Check we have space for a new NRO.
        NroInfo* nro_info = nullptr;
        R_TRY(context->GetFreeNroInfo(std::addressof(nro_info)));
        nro_info->nro_heap_address = nro_address;
        nro_info->nro_heap_size = nro_size;
        nro_info->bss_heap_address = bss_address;
        nro_info->bss_heap_size = bss_size;

        // Map the NRO.
        R_TRY(MapNro(std::addressof(nro_info->base_address), context->GetProcess(), nro_address,
                     nro_size, bss_address, bss_size, generate_random));
        ON_RESULT_FAILURE {
            UnmapNro(context->GetProcess(), nro_info->base_address, nro_address, nro_size,
                     bss_address, bss_size);
        };

        // Validate the NRO (parsing region extents).
        u64 rx_size = 0, ro_size = 0, rw_size = 0;
        R_TRY(context->ValidateNro(std::addressof(nro_info->module_id), std::addressof(rx_size),
                                   std::addressof(ro_size), std::addressof(rw_size),
                                   nro_info->base_address, nro_size, bss_size));

        // Set NRO perms.
        R_TRY(SetNroPerms(context->GetProcess(), nro_info->base_address, rx_size, ro_size,
                          rw_size + bss_size));

        context->SetNroInfoInUse(nro_info, true);
        nro_info->code_size = rx_size + ro_size;
        nro_info->rw_size = rw_size;
        *out_address = nro_info->base_address;
        R_SUCCEED();
    }

    Result UnmapManualLoadModuleMemory(size_t context_id, u64 nro_address) {
        // Get context.
        ProcessContext* context = this->GetContextById(context_id);
        ASSERT(context != nullptr);

        // Validate address.
        R_UNLESS(Common::IsAligned(nro_address, Core::Memory::SUDACHI_PAGESIZE),
                 RO::ResultInvalidAddress);

        // Check the NRO is loaded.
        NroInfo* nro_info = nullptr;
        R_TRY(context->GetNroInfoByAddress(std::addressof(nro_info), nro_address));

        // Unmap.
        const NroInfo nro_backup = *nro_info;
        {
            // Nintendo does this unconditionally, whether or not the actual unmap succeeds.
            context->SetNroInfoInUse(nro_info, false);
            std::memset(nro_info, 0, sizeof(*nro_info));
        }
        R_RETURN(UnmapNro(context->GetProcess(), nro_backup.base_address,
                          nro_backup.nro_heap_address, nro_backup.code_size + nro_backup.rw_size,
                          nro_backup.bss_heap_address, nro_backup.bss_heap_size));
    }

private:
    std::array<ProcessContext, MaxSessions> process_contexts;
    std::mt19937_64 generate_random;

    // Context Helpers.
    ProcessContext* GetContextById(size_t context_id) {
        if (context_id == InvalidContextId) {
            return nullptr;
        }

        ASSERT(context_id < process_contexts.size());
        return std::addressof(process_contexts[context_id]);
    }

    ProcessContext* GetContextByProcessId(u64 process_id) {
        for (size_t i = 0; i < MaxSessions; i++) {
            if (process_contexts[i].GetProcessId() == process_id) {
                return std::addressof(process_contexts[i]);
            }
        }
        return nullptr;
    }

    size_t AllocateContext(Kernel::KProcess* process, u64 process_id) {
        // Find a free process context.
        for (size_t i = 0; i < MaxSessions; i++) {
            ProcessContext* context = std::addressof(process_contexts[i]);

            if (context->IsFree()) {
                context->Initialize(process, process_id);
                return i;
            }
        }

        // Failure to find a free context is actually an abort condition.
        UNREACHABLE();
    }

    void FreeContext(size_t context_id) {
        if (ProcessContext* context = GetContextById(context_id); context != nullptr) {
            context->Finalize();
        }
    }
};

class RoInterface : public ServiceFramework<RoInterface> {
public:
    explicit RoInterface(Core::System& system_, const char* name_, std::shared_ptr<RoContext> ro,
                         NrrKind nrr_kind)
        : ServiceFramework{system_, name_}, m_ro(ro), m_context_id(InvalidContextId),
          m_nrr_kind(nrr_kind) {

        // clang-format off
        static const FunctionInfo functions[] = {
            {0,  C<&RoInterface::MapManualLoadModuleMemory>, "MapManualLoadModuleMemory"},
            {1,  C<&RoInterface::UnmapManualLoadModuleMemory>, "UnmapManualLoadModuleMemory"},
            {2,  C<&RoInterface::RegisterModuleInfo>, "RegisterModuleInfo"},
            {3,  C<&RoInterface::UnregisterModuleInfo>, "UnregisterModuleInfo"},
            {4,  C<&RoInterface::RegisterProcessHandle>, "RegisterProcessHandle"},
            {10, C<&RoInterface::RegisterProcessModuleInfo>, "RegisterProcessModuleInfo"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

    ~RoInterface() {
        m_ro->UnregisterProcess(m_context_id);
    }

    Result MapManualLoadModuleMemory(Out<u64> out_load_address, ClientProcessId client_pid,
                                     u64 nro_address, u64 nro_size, u64 bss_address, u64 bss_size) {
        R_TRY(m_ro->ValidateProcess(m_context_id, *client_pid));
        R_RETURN(m_ro->MapManualLoadModuleMemory(out_load_address.Get(), m_context_id, nro_address,
                                                 nro_size, bss_address, bss_size));
    }

    Result UnmapManualLoadModuleMemory(ClientProcessId client_pid, u64 nro_address) {
        R_TRY(m_ro->ValidateProcess(m_context_id, *client_pid));
        R_RETURN(m_ro->UnmapManualLoadModuleMemory(m_context_id, nro_address));
    }

    Result RegisterModuleInfo(ClientProcessId client_pid, u64 nrr_address, u64 nrr_size) {
        R_TRY(m_ro->ValidateProcess(m_context_id, *client_pid));
        R_RETURN(
            m_ro->RegisterModuleInfo(m_context_id, nrr_address, nrr_size, NrrKind::User, true));
    }

    Result UnregisterModuleInfo(ClientProcessId client_pid, u64 nrr_address) {
        R_TRY(m_ro->ValidateProcess(m_context_id, *client_pid));
        R_RETURN(m_ro->UnregisterModuleInfo(m_context_id, nrr_address));
    }

    Result RegisterProcessHandle(ClientProcessId client_pid,
                                 InCopyHandle<Kernel::KProcess> process) {
        // Register the process.
        R_RETURN(m_ro->RegisterProcess(std::addressof(m_context_id), process.Get(), *client_pid));
    }

    Result RegisterProcessModuleInfo(ClientProcessId client_pid, u64 nrr_address, u64 nrr_size,
                                     InCopyHandle<Kernel::KProcess> process) {
        // Validate the process.
        R_TRY(m_ro->ValidateProcess(m_context_id, *client_pid));

        // Register the module.
        R_RETURN(m_ro->RegisterModuleInfo(m_context_id, nrr_address, nrr_size, m_nrr_kind,
                                          m_nrr_kind == NrrKind::JitPlugin));
    }

private:
    std::shared_ptr<RoContext> m_ro{};
    size_t m_context_id{};
    NrrKind m_nrr_kind{};
};

} // namespace

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    auto ro = std::make_shared<RoContext>();

    const auto RoInterfaceFactoryForUser = [&, ro] {
        return std::make_shared<RoInterface>(system, "ldr:ro", ro, NrrKind::User);
    };

    const auto RoInterfaceFactoryForJitPlugin = [&, ro] {
        return std::make_shared<RoInterface>(system, "ro:1", ro, NrrKind::JitPlugin);
    };

    server_manager->RegisterNamedService("ldr:ro", std::move(RoInterfaceFactoryForUser));
    server_manager->RegisterNamedService("ro:1", std::move(RoInterfaceFactoryForJitPlugin));

    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::RO
