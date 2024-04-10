// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.sudachi.sudachi_emu.utils

object GameMetadata {
    external fun getIsValid(path: String): Boolean

    external fun getTitle(path: String): String

    external fun getProgramId(path: String): String

    external fun getDeveloper(path: String): String

    external fun getVersion(path: String, reload: Boolean): String

    external fun getIcon(path: String): ByteArray

    external fun getIsHomebrew(path: String): Boolean

    external fun resetMetadata()
}
