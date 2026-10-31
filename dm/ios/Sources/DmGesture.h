// The swipe touch gamepad's way into the host's input (GesturePad.swift -> DmGamepad.m). Both calls come from the main thread and are published
// with the other touch controls, as dm_input_set (dm_input.h) takes them: mask is nn::hid::NpadButton bits, the stick is ±32767 with up positive.
#pragma once
#include <stdint.h>

/// Presses or releases the buttons in mask.
void dm_gesture_button(uint64_t mask, int pressed);

/// The right stick as the swipe layout holds it; 0, 0 lets it go.
void dm_gesture_stick(int32_t x, int32_t y);
