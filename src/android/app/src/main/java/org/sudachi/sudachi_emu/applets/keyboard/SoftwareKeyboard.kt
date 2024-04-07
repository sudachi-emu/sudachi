// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.sudachi.sudachi_emu.applets.keyboard

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.view.KeyEvent
import android.view.View
import android.view.WindowInsets
import android.view.inputmethod.InputMethodManager
import androidx.annotation.Keep
import androidx.core.view.ViewCompat
import java.io.Serializable
import org.sudachi.sudachi_emu.NativeLibrary
import org.sudachi.sudachi_emu.R
import org.sudachi.sudachi_emu.applets.keyboard.ui.KeyboardDialogFragment

@Keep
object SoftwareKeyboard {
    lateinit var data: KeyboardData
    val dataLock = Object()

    private fun executeNormalImpl(config: KeyboardConfig) {
        val emulationActivity = NativeLibrary.sEmulationActivity.get()
        data = KeyboardData(SwkbdResult.Cancel.ordinal, "")
        val fragment = KeyboardDialogFragment.newInstance(config)
        fragment.show(emulationActivity!!.supportFragmentManager, KeyboardDialogFragment.TAG)
    }

    private fun executeInlineImpl(config: KeyboardConfig) {
        val emulationActivity = NativeLibrary.sEmulationActivity.get()

        val overlayView = emulationActivity!!.findViewById<View>(R.id.surface_input_overlay)
        val im =
            overlayView.context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        im.showSoftInput(overlayView, InputMethodManager.SHOW_FORCED)

        // There isn't a good way to know that the IMM is dismissed, so poll every 500ms to submit inline keyboard result.
        val handler = Handler(Looper.myLooper()!!)
        val delayMs = 500
        handler.postDelayed(
            object : Runnable {
                override fun run() {
                    val insets = ViewCompat.getRootWindowInsets(overlayView)
                    val isKeyboardVisible = insets!!.isVisible(WindowInsets.Type.ime())
                    if (isKeyboardVisible) {
                        handler.postDelayed(this, delayMs.toLong())
                        return
                    }

                    // No longer visible, submit the result.
                    NativeLibrary.submitInlineKeyboardInput(KeyEvent.KEYCODE_ENTER)
                }
            },
            delayMs.toLong()
        )
    }

    @JvmStatic
    fun executeNormal(config: KeyboardConfig): KeyboardData {
        NativeLibrary.sEmulationActivity.get()!!.runOnUiThread { executeNormalImpl(config) }
        synchronized(dataLock) {
            dataLock.wait()
        }
        return data
    }

    @JvmStatic
    fun executeInline(config: KeyboardConfig) {
        NativeLibrary.sEmulationActivity.get()!!.runOnUiThread { executeInlineImpl(config) }
    }

    // Corresponds to Service::AM::Applets::SwkbdType
    enum class SwkbdType {
        Normal,
        NumberPad,
        Qwerty,
        Unknown3,
        Latin,
        SimplifiedChinese,
        TraditionalChinese,
        Korean
    }

    // Corresponds to Service::AM::Applets::SwkbdPasswordMode
    enum class SwkbdPasswordMode {
        Disabled,
        Enabled
    }

    // Corresponds to Service::AM::Applets::SwkbdResult
    enum class SwkbdResult {
        Ok,
        Cancel
    }

    @Keep
    data class KeyboardConfig(
        var ok_text: String? = null,
        var header_text: String? = null,
        var sub_text: String? = null,
        var guide_text: String? = null,
        var initial_text: String? = null,
        var left_optional_symbol_key: Short = 0,
        var right_optional_symbol_key: Short = 0,
        var max_text_length: Int = 0,
        var min_text_length: Int = 0,
        var initial_cursor_position: Int = 0,
        var type: Int = 0,
        var password_mode: Int = 0,
        var text_draw_type: Int = 0,
        var key_disable_flags: Int = 0,
        var use_blur_background: Boolean = false,
        var enable_backspace_button: Boolean = false,
        var enable_return_button: Boolean = false,
        var disable_cancel_button: Boolean = false
    ) : Serializable

    // Corresponds to Frontend::KeyboardData
    @Keep
    data class KeyboardData(var result: Int, var text: String)
}
