// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.sudachi.sudachi_emu.applets.keyboard.ui

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.text.InputFilter
import android.text.InputType
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.sudachi.sudachi_emu.R
import org.sudachi.sudachi_emu.applets.keyboard.SoftwareKeyboard
import org.sudachi.sudachi_emu.applets.keyboard.SoftwareKeyboard.KeyboardConfig
import org.sudachi.sudachi_emu.databinding.DialogEditTextBinding
import org.sudachi.sudachi_emu.utils.SerializableHelper.serializable

class KeyboardDialogFragment : DialogFragment() {
    private lateinit var binding: DialogEditTextBinding
    private lateinit var config: KeyboardConfig

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        binding = DialogEditTextBinding.inflate(layoutInflater)
        config = requireArguments().serializable(CONFIG)!!

        // Set up the input
        binding.editText.hint = config.initial_text
        binding.editText.isSingleLine = !config.enable_return_button
        binding.editText.filters =
            arrayOf<InputFilter>(InputFilter.LengthFilter(config.max_text_length))

        // Handle input type
        var inputType: Int
        when (config.type) {
            SoftwareKeyboard.SwkbdType.Normal.ordinal,
            SoftwareKeyboard.SwkbdType.Qwerty.ordinal,
            SoftwareKeyboard.SwkbdType.Unknown3.ordinal,
            SoftwareKeyboard.SwkbdType.Latin.ordinal,
            SoftwareKeyboard.SwkbdType.SimplifiedChinese.ordinal,
            SoftwareKeyboard.SwkbdType.TraditionalChinese.ordinal,
            SoftwareKeyboard.SwkbdType.Korean.ordinal -> {
                inputType = InputType.TYPE_CLASS_TEXT
                if (config.password_mode == SoftwareKeyboard.SwkbdPasswordMode.Enabled.ordinal) {
                    inputType = inputType or InputType.TYPE_TEXT_VARIATION_PASSWORD
                }
            }
            SoftwareKeyboard.SwkbdType.NumberPad.ordinal -> {
                inputType = InputType.TYPE_CLASS_NUMBER
                if (config.password_mode == SoftwareKeyboard.SwkbdPasswordMode.Enabled.ordinal) {
                    inputType = inputType or InputType.TYPE_NUMBER_VARIATION_PASSWORD
                }
            }
            else -> {
                inputType = InputType.TYPE_CLASS_TEXT
                if (config.password_mode == SoftwareKeyboard.SwkbdPasswordMode.Enabled.ordinal) {
                    inputType = inputType or InputType.TYPE_TEXT_VARIATION_PASSWORD
                }
            }
        }
        binding.editText.inputType = inputType

        val headerText =
            config.header_text!!.ifEmpty { resources.getString(R.string.software_keyboard) }
        val okText =
            config.ok_text!!.ifEmpty { resources.getString(R.string.submit) }

        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(headerText)
            .setView(binding.root)
            .setPositiveButton(okText) { _, _ ->
                SoftwareKeyboard.data.result = SoftwareKeyboard.SwkbdResult.Ok.ordinal
                SoftwareKeyboard.data.text = binding.editText.text.toString()
            }
            .setNegativeButton(resources.getString(android.R.string.cancel)) { _, _ ->
                SoftwareKeyboard.data.result = SoftwareKeyboard.SwkbdResult.Cancel.ordinal
            }
            .create()
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        synchronized(SoftwareKeyboard.dataLock) {
            SoftwareKeyboard.dataLock.notifyAll()
        }
    }

    companion object {
        const val TAG = "KeyboardDialogFragment"
        const val CONFIG = "keyboard_config"

        fun newInstance(config: KeyboardConfig?): KeyboardDialogFragment {
            val frag = KeyboardDialogFragment()
            val args = Bundle()
            args.putSerializable(CONFIG, config)
            frag.arguments = args
            return frag
        }
    }
}
