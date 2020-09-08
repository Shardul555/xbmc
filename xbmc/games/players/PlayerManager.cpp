/*
 *      Copyright (C) 2017 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "PlayerManager.h"

#include "ServiceBroker.h"
#include "input/InputManager.h"
#include "peripherals/Peripherals.h"

using namespace KODI;
using namespace GAME;

CPlayerManager::CPlayerManager(PERIPHERALS::CPeripherals& peripheralManager,
                               CInputManager& inputManager)
  : m_peripheralManager(peripheralManager), m_inputManager(inputManager)
{
  m_peripheralManager.RegisterObserver(this);
  m_inputManager.RegisterKeyboardDriverHandler(this);
  m_inputManager.RegisterMouseDriverHandler(this);
}

CPlayerManager::~CPlayerManager()
{
  m_inputManager.UnregisterMouseDriverHandler(this);
  m_inputManager.UnregisterKeyboardDriverHandler(this);
  m_peripheralManager.UnregisterObserver(this);
}

void CPlayerManager::Notify(const Observable& obs, const ObservableMessage msg)
{
  switch (msg)
  {
    case ObservableMessagePeripheralsChanged:
    {
      PERIPHERALS::EventLockHandlePtr lock = CServiceBroker::GetPeripherals().RegisterEventLock();

      ProcessJoysticks();

      OnJoystickEvent();
      break;
    }
    default:
      break;
  }
}

void CGameClientInput::ProcessJoysticks()
{
  PERIPHERALS::PeripheralVector joysticks;
  CServiceBroker::GetPeripherals().GetPeripheralsWithFeature(joysticks,
                                                             PERIPHERALS::FEATURE_JOYSTICK);

  // Update expired joysticks
  PortMap portMapCopy = m_portMap;
  for (auto& it : portMapCopy)
  {
    JOYSTICK::IInputProvider* inputProvider = it.first;
    CGameClientJoystick* gameJoystick = it.second;

    const bool bExpired =
        std::find_if(joysticks.begin(), joysticks.end(),
                     [inputProvider](const PERIPHERALS::PeripheralPtr& joystick) {
                       return inputProvider ==
                              static_cast<JOYSTICK::IInputProvider*>(joystick.get());
                     }) == joysticks.end();

    if (bExpired)
    {
      gameJoystick->UnregisterInput(nullptr);
      m_portMap.erase(inputProvider);
    }
  }

  // Perform the port mapping
  PortMap newPortMap = MapJoysticks(joysticks, m_joysticks);

  // Update connected joysticks
  for (auto& peripheralJoystick : joysticks)
  {
    // Upcast to input interface
    JOYSTICK::IInputProvider* inputProvider = peripheralJoystick.get();

    auto itConnectedPort = newPortMap.find(inputProvider);
    auto itDisconnectedPort = m_portMap.find(inputProvider);

    CGameClientJoystick* newJoystick =
        itConnectedPort != newPortMap.end() ? itConnectedPort->second : nullptr;
    CGameClientJoystick* oldJoystick =
        itDisconnectedPort != m_portMap.end() ? itDisconnectedPort->second : nullptr;

    if (oldJoystick != newJoystick)
    {
      // Unregister old input handler
      if (oldJoystick != nullptr)
      {
        oldJoystick->UnregisterInput(inputProvider);
        m_portMap.erase(itDisconnectedPort);
      }

      // Register new handler
      if (newJoystick != nullptr)
      {
        newJoystick->RegisterInput(inputProvider);
        m_portMap[inputProvider] = newJoystick;
      }
    }
  }
}

CGameClientInput::PortMap CGameClientInput::MapJoysticks(
    const PERIPHERALS::PeripheralVector& peripheralJoysticks,
    const JoystickMap& gameClientjoysticks) const
{
  PortMap result;

  //! @todo Preserve existing joystick ports

  // Sort by order of last button press
  PERIPHERALS::PeripheralVector sortedJoysticks = peripheralJoysticks;
  std::sort(sortedJoysticks.begin(), sortedJoysticks.end(),
            [](const PERIPHERALS::PeripheralPtr& lhs, const PERIPHERALS::PeripheralPtr& rhs) {
              if (lhs->LastActive().IsValid() && !rhs->LastActive().IsValid())
                return true;
              if (!lhs->LastActive().IsValid() && rhs->LastActive().IsValid())
                return false;

              return lhs->LastActive() > rhs->LastActive();
            });

  unsigned int i = 0;
  for (const auto& it : gameClientjoysticks)
  {
    if (i >= peripheralJoysticks.size())
      break;

    // Check topology player limit
    const int playerLimit = m_topology->PlayerLimit();
    if (playerLimit >= 0 && static_cast<int>(i) >= playerLimit)
      break;

    // Dereference iterators
    const PERIPHERALS::PeripheralPtr& peripheralJoystick = sortedJoysticks[i++];
    const std::unique_ptr<CGameClientJoystick>& gameClientJoystick = it.second;

    // Map input provider to input handler
    result[peripheralJoystick.get()] = gameClientJoystick.get();
    gameClientJoystick->SetSource(peripheralJoystick->ControllerProfile());
  }

  return result;
}

bool CPlayerManager::OnKeyPress(const CKey& key)
{
  OnKeyboardAction();
  return false;
}

void CPlayerManager::OnKeyRelease(const CKey& key)
{
  OnKeyboardAction();
}

bool CPlayerManager::OnPosition(int x, int y)
{
  OnMouseAction();
  return false;
}

bool CPlayerManager::OnButtonPress(MOUSE::BUTTON_ID button)
{
  OnMouseAction();
  return false;
}

void CPlayerManager::OnButtonRelease(MOUSE::BUTTON_ID button)
{
  OnMouseAction();
}

void CPlayerManager::OnJoystickEvent()
{
  //! @todo
  using namespace PERIPHERALS;

  PeripheralVector peripherals;
  m_peripheralManager.GetPeripheralsWithFeature(peripherals, FEATURE_JOYSTICK);
}

void CPlayerManager::OnKeyboardAction()
{
  if (!m_bHasKeyboard)
  {
    m_bHasKeyboard = true;

    //! @todo Notify of state update
    using namespace PERIPHERALS;

    PeripheralVector peripherals;
    m_peripheralManager.GetPeripheralsWithFeature(peripherals, FEATURE_KEYBOARD);
  }
}

void CPlayerManager::OnMouseAction()
{
  if (!m_bHasMouse)
  {
    m_bHasMouse = true;

    //! @todo Notify of state update
    using namespace PERIPHERALS;

    PeripheralVector peripherals;
    m_peripheralManager.GetPeripheralsWithFeature(peripherals, FEATURE_MOUSE);
  }
}
