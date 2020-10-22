//
// Copyright (c) 2017-2020 the rbfx project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
#include "../Precompiled.h"

#include "../Audio/Sound.h"
#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsEvents.h"
#include "../Graphics/Shader.h"
#include "../Graphics/VertexBuffer.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/Octree.h"
#include "../Graphics/Viewport.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Technique.h"
#include "../Input/Input.h"
#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/ResourceEvents.h"
#include "../Scene/Scene.h"
#ifdef URHO3D_SYSTEMUI
#include "../SystemUI/SystemUI.h"
#endif
#include "../RmlUI/RmlUI.h"
#include "../RmlUI/RmlRenderer.h"
#include "../RmlUI/RmlSystem.h"
#include "../RmlUI/RmlFile.h"
#include "../RmlUI/RmlEventListeners.h"
#include "../RmlUI/RmlMaterialComponent.h"
#include "../RmlUI/RmlTextureComponent.h"
#include "../RmlUI/RmlUIComponent.h"

#include <atomic>
#include <SDL/SDL.h>
#include <EASTL/fixed_vector.h>
#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

#include "../DebugNew.h"

namespace Urho3D
{

const char* RML_UI_CATEGORY = "Rml UI";

static MouseButton MakeTouchIDMask(int id)
{
    return static_cast<MouseButton>(1u << static_cast<MouseButtonFlags::Integer>(id)); // NOLINT(misc-misplaced-widening-cast)
}

static int MouseButtonSDLToRml(unsigned int button);
static int ModifiersSDLToRml(unsigned short modifier);

namespace Detail
{

/// Event instancer that translates some inline events to native Urho3D events.
class RmlEventListenerInstancer : public Rml::EventListenerInstancer
{
public:
    /// Create an instance of inline event listener, if applicable.
	Rml::EventListener* InstanceEventListener(const Rml::String& value, Rml::Element* element) override
    {
        if (auto* instancer = SoundEventListener::CreateInstancer(value, element))
            return instancer;

        if (auto* instancer = CustomEventListener::CreateInstancer(value, element))
            return instancer;

        return nullptr;
    }
};

class RmlContextInstancer : public Rml::ContextInstancer
{
public:
    /// Create instance of RmlContext.
    Rml::ContextPtr InstanceContext(const Rml::String& name) override
    {
        return Rml::ContextPtr(new Detail::RmlContext(name));
    }
    /// Free instance of RmlContext.
    void ReleaseContext(Rml::Context* context) override
    {
        delete static_cast<Detail::RmlContext*>(context);
    }

protected:
    /// RmlContextInstancer is static, nothing to release.
    void Release() override { }
};

class RmlPlugin : public Rml::Plugin
{
public:
    virtual ~RmlPlugin() = default;

    int GetEventClasses() override { return EVT_DOCUMENT; }

    void OnDocumentUnload(Rml::ElementDocument* document) override
    {
        RmlContext* rmlContext = static_cast<RmlContext*>(document->GetContext());
        RmlUI* ui = rmlContext->GetOwnerSubsystem();
        ui->OnDocumentUnload(document);
    }
};

}

/// Number of instances of RmlUI. Used to initialize and release RmlUi library.
static std::atomic<int> rmlInstanceCounter;

/// A standalone object which creates event instances for RmlUi.
static Detail::RmlEventListenerInstancer RmlEventListenerInstancerInstance;

/// A standalone object which creates Context instances for RmlUi.
static Detail::RmlContextInstancer RmlContextInstancerInstance;

/// Map engine keys to RmlUi keys. Note that top bit is cleared from key constants when they are used as array index.
static const ea::unordered_map<unsigned, uint16_t> keyMap{
    { SDLK_SPACE, Rml::Input::KI_SPACE },
    { SDLK_0, Rml::Input::KI_0 },
    { SDLK_1, Rml::Input::KI_1 },
    { SDLK_2, Rml::Input::KI_2 },
    { SDLK_3, Rml::Input::KI_3 },
    { SDLK_4, Rml::Input::KI_4 },
    { SDLK_5, Rml::Input::KI_5 },
    { SDLK_6, Rml::Input::KI_6 },
    { SDLK_7, Rml::Input::KI_7 },
    { SDLK_8, Rml::Input::KI_8 },
    { SDLK_9, Rml::Input::KI_9 },
    { SDLK_a, Rml::Input::KI_A },
    { SDLK_b, Rml::Input::KI_B },
    { SDLK_c, Rml::Input::KI_C },
    { SDLK_d, Rml::Input::KI_D },
    { SDLK_e, Rml::Input::KI_E },
    { SDLK_f, Rml::Input::KI_F },
    { SDLK_g, Rml::Input::KI_G },
    { SDLK_h, Rml::Input::KI_H },
    { SDLK_i, Rml::Input::KI_I },
    { SDLK_j, Rml::Input::KI_J },
    { SDLK_k, Rml::Input::KI_K },
    { SDLK_l, Rml::Input::KI_L },
    { SDLK_m, Rml::Input::KI_M },
    { SDLK_n, Rml::Input::KI_N },
    { SDLK_o, Rml::Input::KI_O },
    { SDLK_p, Rml::Input::KI_P },
    { SDLK_q, Rml::Input::KI_Q },
    { SDLK_r, Rml::Input::KI_R },
    { SDLK_s, Rml::Input::KI_S },
    { SDLK_t, Rml::Input::KI_T },
    { SDLK_u, Rml::Input::KI_U },
    { SDLK_v, Rml::Input::KI_V },
    { SDLK_w, Rml::Input::KI_W },
    { SDLK_x, Rml::Input::KI_X },
    { SDLK_y, Rml::Input::KI_Y },
    { SDLK_z, Rml::Input::KI_Z },
    { SDLK_SEMICOLON, Rml::Input::KI_OEM_1 },           // US standard keyboard; the ';:' key.
    { SDLK_EQUALS, Rml::Input::KI_OEM_PLUS },           // Any region; the '=+' key.
    { SDLK_COMMA, Rml::Input::KI_OEM_COMMA },           // Any region; the ',<' key.
    { SDLK_MINUS, Rml::Input::KI_OEM_MINUS },           // Any region; the '-_' key.
    { SDLK_PERIOD, Rml::Input::KI_OEM_PERIOD },         // Any region; the '.>' key.
    { SDLK_SLASH, Rml::Input::KI_OEM_2 },               // Any region; the '/?' key.
    { SDLK_LEFTBRACKET, Rml::Input::KI_OEM_4 },         // US standard keyboard; the '[{' key.
    { SDLK_BACKSLASH, Rml::Input::KI_OEM_5 },           // US standard keyboard; the '\|' key.
    { SDLK_RIGHTBRACKET, Rml::Input::KI_OEM_6 },        // US standard keyboard; the ']}' key.
    { SDLK_KP_0, Rml::Input::KI_NUMPAD0 },
    { SDLK_KP_1, Rml::Input::KI_NUMPAD1 },
    { SDLK_KP_2, Rml::Input::KI_NUMPAD2 },
    { SDLK_KP_3, Rml::Input::KI_NUMPAD3 },
    { SDLK_KP_4, Rml::Input::KI_NUMPAD4 },
    { SDLK_KP_5, Rml::Input::KI_NUMPAD5 },
    { SDLK_KP_6, Rml::Input::KI_NUMPAD6 },
    { SDLK_KP_7, Rml::Input::KI_NUMPAD7 },
    { SDLK_KP_8, Rml::Input::KI_NUMPAD8 },
    { SDLK_KP_9, Rml::Input::KI_NUMPAD9 },
    { SDLK_KP_ENTER, Rml::Input::KI_NUMPADENTER },
    { SDLK_KP_MULTIPLY, Rml::Input::KI_MULTIPLY },      // Asterisk on the numeric keypad.
    { SDLK_KP_PLUS, Rml::Input::KI_ADD },               // Plus on the numeric keypad.
    { SDLK_KP_SPACE, Rml::Input::KI_SEPARATOR },
    { SDLK_KP_MINUS, Rml::Input::KI_SUBTRACT },         // Minus on the numeric keypad.
    { SDLK_KP_DECIMAL, Rml::Input::KI_DECIMAL },        // Period on the numeric keypad.
    { SDLK_KP_DIVIDE, Rml::Input::KI_DIVIDE },          // Forward Slash on the numeric keypad.
    { SDLK_BACKSPACE, Rml::Input::KI_BACK },            // Backspace key.
    { SDLK_TAB, Rml::Input::KI_TAB },                   // Tab key.
    { SDLK_CLEAR, Rml::Input::KI_CLEAR },
    { SDLK_RETURN, Rml::Input::KI_RETURN },
    { SDLK_PAUSE, Rml::Input::KI_PAUSE },
    { SDLK_CAPSLOCK, Rml::Input::KI_CAPITAL },          // Capslock key.
    { SDLK_ESCAPE, Rml::Input::KI_ESCAPE },             // Escape key.
    { SDLK_PAGEUP, Rml::Input::KI_PRIOR },              // Page Up key.
    { SDLK_PAGEDOWN, Rml::Input::KI_NEXT },             // Page Down key.
    { SDLK_END, Rml::Input::KI_END },
    { SDLK_HOME, Rml::Input::KI_HOME },
    { SDLK_LEFT, Rml::Input::KI_LEFT },                 // Left Arrow key.
    { SDLK_UP, Rml::Input::KI_UP },                     // Up Arrow key.
    { SDLK_RIGHT, Rml::Input::KI_RIGHT },               // Right Arrow key.
    { SDLK_DOWN, Rml::Input::KI_DOWN },                 // Down Arrow key.
    { SDLK_SELECT, Rml::Input::KI_SELECT },
    { SDLK_PRINTSCREEN, Rml::Input::KI_SNAPSHOT },      // Print Screen key.
    { SDLK_INSERT, Rml::Input::KI_INSERT },
    { SDLK_DELETE, Rml::Input::KI_DELETE },
    { SDLK_HELP, Rml::Input::KI_HELP },
    { SDLK_LGUI, Rml::Input::KI_LWIN },                 // Left Windows key.
    { SDLK_RGUI, Rml::Input::KI_RWIN },                 // Right Windows key.
    { SDLK_APPLICATION, Rml::Input::KI_APPS },          // Applications key.
    { SDLK_POWER, Rml::Input::KI_POWER },
    { SDLK_SLEEP, Rml::Input::KI_SLEEP },
    { SDLK_F1, Rml::Input::KI_F1 },
    { SDLK_F2, Rml::Input::KI_F2 },
    { SDLK_F3, Rml::Input::KI_F3 },
    { SDLK_F4, Rml::Input::KI_F4 },
    { SDLK_F5, Rml::Input::KI_F5 },
    { SDLK_F6, Rml::Input::KI_F6 },
    { SDLK_F7, Rml::Input::KI_F7 },
    { SDLK_F8, Rml::Input::KI_F8 },
    { SDLK_F9, Rml::Input::KI_F9 },
    { SDLK_F10, Rml::Input::KI_F10 },
    { SDLK_F11, Rml::Input::KI_F11 },
    { SDLK_F12, Rml::Input::KI_F12 },
    { SDLK_F13, Rml::Input::KI_F13 },
    { SDLK_F14, Rml::Input::KI_F14 },
    { SDLK_F15, Rml::Input::KI_F15 },
    { SDLK_F16, Rml::Input::KI_F16 },
    { SDLK_F17, Rml::Input::KI_F17 },
    { SDLK_F18, Rml::Input::KI_F18 },
    { SDLK_F19, Rml::Input::KI_F19 },
    { SDLK_F20, Rml::Input::KI_F20 },
    { SDLK_F21, Rml::Input::KI_F21 },
    { SDLK_F22, Rml::Input::KI_F22 },
    { SDLK_F23, Rml::Input::KI_F23 },
    { SDLK_F24, Rml::Input::KI_F24 },
    { SDLK_NUMLOCKCLEAR, Rml::Input::KI_NUMLOCK },      // Numlock key.
    { SDLK_SCROLLLOCK, Rml::Input::KI_SCROLL },         // Scroll Lock key.
    { SDLK_LSHIFT, Rml::Input::KI_LSHIFT },
    { SDLK_RSHIFT, Rml::Input::KI_RSHIFT },
    { SDLK_LCTRL, Rml::Input::KI_LCONTROL },
    { SDLK_RCTRL, Rml::Input::KI_RCONTROL },
    { SDLK_LALT, Rml::Input::KI_LMENU },
    { SDLK_RALT, Rml::Input::KI_RMENU },
    { SDLK_MUTE, Rml::Input::KI_VOLUME_MUTE },
    { SDLK_VOLUMEDOWN, Rml::Input::KI_VOLUME_DOWN },
    { SDLK_VOLUMEUP, Rml::Input::KI_VOLUME_UP },
};

RmlUI::RmlUI(Context* context, const char* name)
    : Object(context)
    , name_(name)
{
    // Initializing first instance of RmlUI, initialize backend library as well.
    if (rmlInstanceCounter.fetch_add(1) == 0)
    {
        Rml::SetRenderInterface(new Detail::RmlRenderer(context_));
        Rml::SetSystemInterface(new Detail::RmlSystem(context_));
        Rml::SetFileInterface(new Detail::RmlFile(context_));
        Rml::Initialise();
        Rml::Factory::RegisterEventListenerInstancer(&RmlEventListenerInstancerInstance);
        Rml::Factory::RegisterContextInstancer(&RmlContextInstancerInstance);
    }
    rmlContext_ = static_cast<Detail::RmlContext*>(Rml::CreateContext(name_.c_str(), GetDesiredCanvasSize()));
    rmlContext_->SetOwnerSubsystem(this);

    if (auto* ui = GetSubsystem<RmlUI>())
        ui->siblingSubsystems_.push_back(WeakPtr(this));

    SubscribeToEvent(E_SDLRAWINPUT, &RmlUI::HandleInput);
    SubscribeToEvent(E_SCREENMODE, &RmlUI::HandleScreenMode);
    SubscribeToEvent(E_POSTUPDATE, &RmlUI::HandlePostUpdate);
    SubscribeToEvent(E_ENDALLVIEWSRENDER, &RmlUI::HandleEndAllViewsRender);

    SubscribeToEvent(E_FILECHANGED, &RmlUI::HandleResourceReloaded);
}

RmlUI::~RmlUI()
{
    if (auto* ui = GetSubsystem<RmlUI>())
        ui->siblingSubsystems_.erase_first(WeakPtr(this));

    if (rmlContext_ != nullptr)
    {
        if (!Rml::RemoveContext(rmlContext_->GetName()))
            URHO3D_LOGERROR("Removal of RmlUI context {} failed.", rmlContext_->GetName());
    }
    rmlContext_ = nullptr;

    if (rmlInstanceCounter.fetch_sub(1) == 1)
    {
        // Freeing last instance of RmlUI, deinitialize backend library.
        Rml::Factory::RegisterEventListenerInstancer(nullptr); // Set to a static object instance because there is no getter to delete it.
        auto* renderer = Rml::GetRenderInterface();
        auto* system = Rml::GetSystemInterface();
        auto* file = Rml::GetFileInterface();
        Rml::ReleaseTextures();
        Rml::Shutdown();
        delete renderer;
        delete system;
        delete file;
    }
}

Rml::ElementDocument* RmlUI::LoadDocument(const ea::string& path)
{
    return rmlContext_->LoadDocument(path);
}

void RmlUI::SetDebuggerVisible(bool visible)
{
    if (!debuggerInitialized_)
    {
        Rml::Debugger::Initialise(rmlContext_);
        debuggerInitialized_ = true;
    }
    Rml::Debugger::SetVisible(visible);
}

bool RmlUI::LoadFont(const ea::string& resourceName, bool fallback)
{
    return Rml::LoadFontFace(resourceName, fallback);
}

Rml::Context* RmlUI::GetRmlContext() const
{
    return rmlContext_;
}

void RmlUI::HandleScreenMode(StringHash, VariantMap& eventData)
{
    assert(rmlContext_ != nullptr);
    RmlCanvasResizedArgs args;
    args.oldSize_ = rmlContext_->GetDimensions();
    args.newSize_ = GetDesiredCanvasSize();
    rmlContext_->SetDimensions(args.newSize_);
    canvasResizedEvent_(this, args);
}

void RmlUI::HandleInput(StringHash eventType, VariantMap& eventData)
{
    using namespace SDLRawInput;
    if (eventData[P_LAYER].GetInt() != IL_MIDDLEWARE)
        return;

    const SDL_Event& evt = *static_cast<SDL_Event*>(eventData[P_SDLEVENT].GetVoidPtr());

    switch (evt.type)
    {
    case SDL_KEYDOWN:
    {
        auto* input = GetSubsystem<Input>();
        if (input->IsMouseGrabbed())
            return;
        auto it = keyMap.find(evt.key.keysym.sym);
        if (it == keyMap.end())
            return;
        Rml::Input::KeyIdentifier key = static_cast<Rml::Input::KeyIdentifier>(it->second);
        int modifiers = ModifiersSDLToRml(evt.key.keysym.mod);
        eventData[P_CONSUMED] = !rmlContext_->ProcessKeyDown(key, modifiers);
        if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER)
            rmlContext_->ProcessTextInput('\n');
        break;
    }

    case SDL_KEYUP:
    {
        auto* input = GetSubsystem<Input>();
        if (input->IsMouseGrabbed())
            return;
        auto it = keyMap.find(evt.key.keysym.sym);
        if (it == keyMap.end())
            return;
        Rml::Input::KeyIdentifier key = static_cast<Rml::Input::KeyIdentifier>(it->second);
        int modifiers = ModifiersSDLToRml(evt.key.keysym.mod);
        eventData[P_CONSUMED] = !rmlContext_->ProcessKeyUp(key, modifiers);
        break;
    }

    case SDL_TEXTINPUT:
        eventData[P_CONSUMED] = !rmlContext_->ProcessTextInput(&evt.text.text[0]);
        break;

    case SDL_MOUSEBUTTONDOWN:
    {
        auto* input = GetSubsystem<Input>();
        if (input->IsMouseGrabbed())
            return;
        int button = MouseButtonSDLToRml(evt.button.button);
        int modifiers = ModifiersSDLToRml(SDL_GetModState());
        eventData[P_CONSUMED] = !rmlContext_->ProcessMouseButtonDown(button, modifiers);
        break;
    }

    case SDL_MOUSEBUTTONUP:
    {
        int button = MouseButtonSDLToRml(evt.button.button);
        int modifiers = ModifiersSDLToRml(SDL_GetModState());
        eventData[P_CONSUMED] = !rmlContext_->ProcessMouseButtonUp(button, modifiers);
        break;
    }

    case SDL_MOUSEMOTION:
    {
        int modifiers = ModifiersSDLToRml(SDL_GetModState());
        IntVector2 pos(evt.motion.x, evt.motion.y);
        mouseMoveEvent_(this, pos);
        if (pos.x_ >= 0 && pos.y_ >= 0)
            eventData[P_CONSUMED] = !rmlContext_->ProcessMouseMove(pos.x_, pos.y_, modifiers);
    }

    case SDL_MOUSEWHEEL:
    {
        auto* input = GetSubsystem<Input>();
        if (input->IsMouseGrabbed())
            return;
        int modifiers = ModifiersSDLToRml(SDL_GetModState());
        eventData[P_CONSUMED] = !rmlContext_->ProcessMouseWheel(-evt.wheel.y, modifiers);
        break;
    }

    case SDL_FINGERDOWN:
    {
        if (evt.tfinger.touchId != SDL_TOUCH_MOUSEID)
        {
            auto* input = GetSubsystem<Input>();
            auto* graphics = GetSubsystem<Graphics>();
            if (input->IsMouseGrabbed())
                return;
            int modifiers = ModifiersSDLToRml(input->GetQualifiers());
            int button = MouseButtonSDLToRml(1 << (evt.tfinger.fingerId & 0x7ffffffu));
            IntVector2 pos(graphics->GetWidth() * evt.tfinger.x, graphics->GetHeight() * evt.tfinger.y);
            mouseMoveEvent_(this, pos);
            if (pos.x_ >= 0 && pos.y_ >= 0)
                rmlContext_->ProcessMouseMove(pos.x_, pos.y_, modifiers);
            eventData[P_CONSUMED] = !rmlContext_->ProcessMouseButtonDown(button, modifiers);
        }
        break;
    }

    case SDL_FINGERUP:
    {
        if (evt.tfinger.touchId != SDL_TOUCH_MOUSEID)
        {
            auto* input = GetSubsystem<Input>();
            auto* graphics = GetSubsystem<Graphics>();
            if (input->IsMouseGrabbed())
                return;
            int modifiers = ModifiersSDLToRml(input->GetQualifiers());
            int button = MouseButtonSDLToRml(1 << (evt.tfinger.fingerId & 0x7ffffffu));
            IntVector2 pos(graphics->GetWidth() * evt.tfinger.x, graphics->GetHeight() * evt.tfinger.y);
            mouseMoveEvent_(this, pos);
            if (pos.x_ >= 0 && pos.y_ >= 0)
                rmlContext_->ProcessMouseMove(pos.x_, pos.y_, modifiers);
            eventData[P_CONSUMED] = !rmlContext_->ProcessMouseButtonUp(button, modifiers);
            break;
        }
    }

    case SDL_FINGERMOTION:
    {
        if (evt.tfinger.touchId != SDL_TOUCH_MOUSEID)
        {
            auto* input = GetSubsystem<Input>();
            auto* graphics = GetSubsystem<Graphics>();
            if (input->IsMouseGrabbed())
                return;
            int modifiers = ModifiersSDLToRml(input->GetQualifiers());
            IntVector2 pos(graphics->GetWidth() * evt.tfinger.x, graphics->GetHeight() * evt.tfinger.y);
            mouseMoveEvent_(this, pos);
            if (pos.x_ >= 0 && pos.y_ >= 0)
                eventData[P_CONSUMED] = !rmlContext_->ProcessMouseMove(pos.x_, pos.y_, modifiers);
        }
        break;
    }
/*
    case SDL_JOYBUTTONDOWN:
        {
            using namespace JoystickButtonDown;

            unsigned button = evt.jbutton.button;
            SDL_JoystickID joystickID = evt.jbutton.which;
            JoystickState& state = joysticks_[joystickID];

            // Skip ordinary joystick event for a controller
            if (!state.controller_)
            {
                VariantMap& eventData = GetEventDataMap();
                eventData[P_JOYSTICKID] = joystickID;
                eventData[P_BUTTON] = button;

                if (button < state.buttons_.size())
                {
                    state.buttons_[button] = true;
                    state.buttonPress_[button] = true;
                    SendEvent(E_JOYSTICKBUTTONDOWN, eventData);
                }
            }
        }
        break;

    case SDL_JOYBUTTONUP:
        {
            using namespace JoystickButtonUp;

            unsigned button = evt.jbutton.button;
            SDL_JoystickID joystickID = evt.jbutton.which;
            JoystickState& state = joysticks_[joystickID];

            if (!state.controller_)
            {
                VariantMap& eventData = GetEventDataMap();
                eventData[P_JOYSTICKID] = joystickID;
                eventData[P_BUTTON] = button;

                if (button < state.buttons_.size())
                {
                    if (!state.controller_)
                        state.buttons_[button] = false;
                    SendEvent(E_JOYSTICKBUTTONUP, eventData);
                }
            }
        }
        break;

    case SDL_JOYAXISMOTION:
        {
            using namespace JoystickAxisMove;

            SDL_JoystickID joystickID = evt.jaxis.which;
            JoystickState& state = joysticks_[joystickID];

            if (!state.controller_)
            {
                VariantMap& eventData = GetEventDataMap();
                eventData[P_JOYSTICKID] = joystickID;
                eventData[P_AXIS] = evt.jaxis.axis;
                eventData[P_POSITION] = Clamp((float)evt.jaxis.value / 32767.0f, -1.0f, 1.0f);

                if (evt.jaxis.axis < state.axes_.size())
                {
                    // If the joystick is a controller, only use the controller axis mappings
                    // (we'll also get the controller event)
                    if (!state.controller_)
                        state.axes_[evt.jaxis.axis] = eventData[P_POSITION].GetFloat();
                    SendEvent(E_JOYSTICKAXISMOVE, eventData);
                }
            }
        }
        break;

    case SDL_JOYHATMOTION:
        {
            using namespace JoystickHatMove;

            SDL_JoystickID joystickID = evt.jaxis.which;
            JoystickState& state = joysticks_[joystickID];

            VariantMap& eventData = GetEventDataMap();
            eventData[P_JOYSTICKID] = joystickID;
            eventData[P_HAT] = evt.jhat.hat;
            eventData[P_POSITION] = evt.jhat.value;

            if (evt.jhat.hat < state.hats_.size())
            {
                state.hats_[evt.jhat.hat] = evt.jhat.value;
                SendEvent(E_JOYSTICKHATMOVE, eventData);
            }
        }
        break;

    case SDL_CONTROLLERBUTTONDOWN:
        {
            using namespace JoystickButtonDown;

            unsigned button = evt.cbutton.button;
            SDL_JoystickID joystickID = evt.cbutton.which;
            JoystickState& state = joysticks_[joystickID];

            VariantMap& eventData = GetEventDataMap();
            eventData[P_JOYSTICKID] = joystickID;
            eventData[P_BUTTON] = button;

            if (button < state.buttons_.size())
            {
                state.buttons_[button] = true;
                state.buttonPress_[button] = true;
                SendEvent(E_JOYSTICKBUTTONDOWN, eventData);
            }
        }
        break;

    case SDL_CONTROLLERBUTTONUP:
        {
            using namespace JoystickButtonUp;

            unsigned button = evt.cbutton.button;
            SDL_JoystickID joystickID = evt.cbutton.which;
            JoystickState& state = joysticks_[joystickID];

            VariantMap& eventData = GetEventDataMap();
            eventData[P_JOYSTICKID] = joystickID;
            eventData[P_BUTTON] = button;

            if (button < state.buttons_.size())
            {
                state.buttons_[button] = false;
                SendEvent(E_JOYSTICKBUTTONUP, eventData);
            }
        }
        break;

    case SDL_CONTROLLERAXISMOTION:
        {
            using namespace JoystickAxisMove;

            SDL_JoystickID joystickID = evt.caxis.which;
            JoystickState& state = joysticks_[joystickID];

            VariantMap& eventData = GetEventDataMap();
            eventData[P_JOYSTICKID] = joystickID;
            eventData[P_AXIS] = evt.caxis.axis;
            eventData[P_POSITION] = Clamp((float)evt.caxis.value / 32767.0f, -1.0f, 1.0f);

            if (evt.caxis.axis < state.axes_.size())
            {
                state.axes_[evt.caxis.axis] = eventData[P_POSITION].GetFloat();
                SendEvent(E_JOYSTICKAXISMOVE, eventData);
            }
        }
        break;
*/
    case SDL_DROPFILE:
    {
        if (auto* element = rmlContext_->GetHoverElement())
        {
            Rml::Dictionary args;
            args["path"] = evt.drop.file;
            element->DispatchEvent("dropfile", args);
        }
        SDL_free(evt.drop.file);
        break;
    }

    default:
        break;
    }
}

void RmlUI::HandlePostUpdate(StringHash, VariantMap& eventData)
{
    Update(eventData[PostUpdate::P_TIMESTEP].GetFloat());
}

void RmlUI::HandleDropFile(StringHash, VariantMap& eventData)
{
    using namespace DropFile;
    auto* input = GetSubsystem<Input>();

    // Sending the UI variant of the event only makes sense if the OS cursor is visible (not locked to window center)
    if (!input->IsMouseVisible())
        return;

    if (auto* element = rmlContext_->GetHoverElement())
    {
        Rml::Dictionary args;
        args["path"] = eventData[P_FILENAME].GetString().c_str();
        element->DispatchEvent("dropfile", args);
    }
}

void RmlUI::HandleEndAllViewsRender(StringHash, VariantMap& eventData)
{
    if (isRendering_)
        Render();
}

void RmlUI::SetRenderTarget(RenderSurface* target, const Color& clearColor)
{
    renderSurface_ = target;
    clearColor_ = clearColor;
    RmlCanvasResizedArgs args;
    args.oldSize_ = rmlContext_->GetDimensions();
    args.newSize_ = GetDesiredCanvasSize();
    rmlContext_->SetDimensions(args.newSize_);
    canvasResizedEvent_(this, args);
}

void RmlUI::SetRenderTarget(Texture2D* target, const Color& clearColor)
{
    SetRenderTarget(target ? target->GetRenderSurface() : nullptr, clearColor);
}

void RmlUI::SetRenderTarget(std::nullptr_t, const Color& clearColor)
{
    SetRenderTarget(static_cast<RenderSurface*>(nullptr), clearColor);
}

IntVector2 RmlUI::GetDesiredCanvasSize() const
{
    RenderSurface* renderSurface = renderSurface_;
    if (renderSurface)
        return { renderSurface->GetWidth(), renderSurface->GetHeight() };
    else if (Graphics* graphics = GetSubsystem<Graphics>())
        return { graphics->GetWidth(), graphics->GetHeight() };

    // Irrelevant. Canvas will soon be resized once actual screen mode comes in.
    return {512, 512};
}

bool RmlUI::IsHovered() const
{
    Rml::Element* hover = rmlContext_->GetHoverElement();
    return hover != nullptr && hover != rmlContext_->GetRootElement();
}

bool RmlUI::IsInputCaptured() const
{
    if (IsInputCapturedInternal())
        return true;
    for (RmlUI* other : siblingSubsystems_)
    {
        if (other->IsInputCapturedInternal())
            return true;
    }
    return false;
}

bool RmlUI::IsInputCapturedInternal() const
{
    if (Rml::Element* element = rmlContext_->GetFocusElement())
    {
        const ea::string& tag = element->GetTagName();
        return tag == "input" || tag == "textarea" || tag == "select";
    }
    return false;
}

void RmlUI::Render()
{
    Graphics* graphics = GetSubsystem<Graphics>();
    if (!graphics || !graphics->IsInitialized())
        return;

    URHO3D_PROFILE("RenderUI");
    graphics->ResetRenderTargets();
    if (renderSurface_)
    {
        graphics->SetDepthStencil(renderSurface_->GetLinkedDepthStencil());
        graphics->SetRenderTarget(0, renderSurface_);
        graphics->SetViewport(IntRect(0, 0, renderSurface_->GetWidth(), renderSurface_->GetHeight()));

        if (clearColor_.a_ > 0)
            graphics->Clear(CLEAR_COLOR, clearColor_);
    }
    else
        graphics->SetRenderTarget(0, (RenderSurface*)nullptr);

    rmlContext_->Render();
}

void RmlUI::OnDocumentUnload(Rml::ElementDocument* document)
{
    documentClosedEvent_(this, document);
}

void RmlUI::Update(float timeStep)
{
    (void)timeStep;
    URHO3D_PROFILE("UpdateUI");

    if (rmlContext_)
        rmlContext_->Update();
}

void RmlUI::HandleResourceReloaded(StringHash eventType, VariantMap& eventData)
{
    (void)eventType;
    using namespace FileChanged;
    const ea::string& fileName = eventData[P_FILENAME].GetString();
    Detail::RmlFile* file = static_cast<Detail::RmlFile*>(Rml::GetFileInterface());
    if (file->IsFileLoaded(fileName))
    {
        file->ClearLoadedFiles();

        Rml::ReleaseTextures();
        Rml::Factory::ClearStyleSheetCache();
        Rml::Factory::ClearTemplateCache();

        ea::fixed_vector<Rml::ElementDocument*, 64> unloadingDocuments;
        for (int i = 0; i < rmlContext_->GetNumDocuments(); i++)
            unloadingDocuments.push_back(rmlContext_->GetDocument(i));

        for (Rml::ElementDocument* document : unloadingDocuments)
            ReloadDocument(document);
    }
}

Rml::ElementDocument* RmlUI::ReloadDocument(Rml::ElementDocument* document)
{
    assert(document != nullptr);
    assert(document->GetContext() == rmlContext_);

    Vector2 pos = document->GetAbsoluteOffset(Rml::Box::BORDER);
    Vector2 size = document->GetBox().GetSize(Rml::Box::CONTENT);
    Rml::ModalFlag modal = document->IsModal() ? Rml::ModalFlag::Modal : Rml::ModalFlag::None;
    Rml::FocusFlag focus = Rml::FocusFlag::Auto;
    bool visible = document->IsVisible();
    if (Rml::Element* element = rmlContext_->GetFocusElement())
        focus = element->GetOwnerDocument() == document ? Rml::FocusFlag::Document : focus;

    document->Close();

    Rml::ElementDocument* newDocument = rmlContext_->LoadDocument(document->GetSourceURL());
    newDocument->SetProperty(Rml::PropertyId::Left, Rml::Property(pos.x_, Rml::Property::PX));
    newDocument->SetProperty(Rml::PropertyId::Top, Rml::Property(pos.y_, Rml::Property::PX));
    newDocument->SetProperty(Rml::PropertyId::Width, Rml::Property(size.x_, Rml::Property::PX));
    newDocument->SetProperty(Rml::PropertyId::Height, Rml::Property(size.y_, Rml::Property::PX));
    newDocument->UpdateDocument();

    if (visible)
        newDocument->Show(modal, focus);

    RmlDocumentReloadedArgs args;
    args.unloadedDocument_ = document;
    args.loadedDocument_ = newDocument;
    documentReloaded_(this, args);

    return newDocument;
}

static int MouseButtonSDLToRml(unsigned int button)
{
    int rmlButton = -1;
    switch (button)
    {
    case SDL_BUTTON_LEFT:   rmlButton = 0; break;
    case SDL_BUTTON_MIDDLE: rmlButton = 2; break;
    case SDL_BUTTON_RIGHT:  rmlButton = 1; break;
    case SDL_BUTTON_X1:     rmlButton = 3; break;
    case SDL_BUTTON_X2:     rmlButton = 4; break;
    default:                           break;
    }
    return rmlButton;
}

static int ModifiersSDLToRml(unsigned short modifier)
{
    int rmlModifiers = 0;
    if (modifier & KMOD_ALT)
        rmlModifiers |= Rml::Input::KeyModifier::KM_ALT;
    if (modifier & KMOD_CTRL)
        rmlModifiers |= Rml::Input::KeyModifier::KM_CTRL;
    if (modifier & KMOD_SHIFT)
        rmlModifiers |= Rml::Input::KeyModifier::KM_SHIFT;
    return rmlModifiers;
}

void RegisterRmlUILibrary(Context* context)
{
    context->RegisterFactory<RmlUI>();
    RmlUIComponent::RegisterObject(context);
    RmlTextureComponent::RegisterObject(context);
    RmlMaterialComponent::RegisterObject(context);
}

}
