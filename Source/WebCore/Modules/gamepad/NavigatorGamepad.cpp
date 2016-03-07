/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NavigatorGamepad.h"

#if ENABLE(GAMEPAD)

#include "Gamepad.h"
#include "Navigator.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

NavigatorGamepad::NavigatorGamepad()
{
}

const char* NavigatorGamepad::supplementName()
{
    return "NavigatorGamepad";
}

NavigatorGamepad* NavigatorGamepad::from(Navigator* navigator)
{
    NavigatorGamepad* supplement = static_cast<NavigatorGamepad*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        auto newSupplement = std::make_unique<NavigatorGamepad>();
        supplement = newSupplement.get();
        provideTo(navigator, supplementName(), std::move(newSupplement));
    }
    return supplement;
}

const Vector<RefPtr<Gamepad>>& NavigatorGamepad::getGamepads(Navigator* navigator)
{
    return NavigatorGamepad::from(navigator)->gamepads();
}

const Vector<RefPtr<Gamepad>>& NavigatorGamepad::gamepads()
{
    return m_gamepads;
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
