/*
 *  Copyright (C) 2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DialogInGameSaves.h"

#include "ServiceBroker.h"
#include "cores/RetroPlayer/guibridge/GUIGameRenderManager.h"
#include "cores/RetroPlayer/guibridge/GUIGameSettingsHandle.h"
#include "cores/RetroPlayer/playback/IPlayback.h"
#include "cores/RetroPlayer/savestates/ISavestate.h"
#include "cores/RetroPlayer/savestates/SavestateDatabase.h"
#include "games/dialogs/DialogGameDefines.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/WindowIDs.h"
#include "settings/GameSettings.h"
#include "settings/MediaSettings.h"
#include "utils/log.h"
//#include "utils/Variant.h"

using namespace KODI;
using namespace GAME;
using namespace RETRO;

CDialogInGameSaves::CDialogInGameSaves() : CDialogGameVideoSelect(WINDOW_DIALOG_IN_GAME_SAVES)
{
}

std::string CDialogInGameSaves::GetHeading()
{
  return g_localizeStrings.Get(35249); // "Saved games"
}

void CDialogInGameSaves::PreInit()
{
  m_savestates.Clear();

  InitSavedGames();

  // "Save progress"
  CFileItemPtr item = std::make_shared<CFileItem>(g_localizeStrings.Get(15314));

  item->SetArt("icon", "DefaultAddSource.png");
  item->SetPath("");

  // "Save progress to new save file"
  item->SetProperty(SAVESTATE_CAPTION, g_localizeStrings.Get(15315));

  m_savestates.AddFront(std::move(item), 0);
}

void CDialogInGameSaves::InitSavedGames()
{
  auto gameSettings = CServiceBroker::GetGameRenderManager().RegisterGameSettingsDialog();

  // save current game
  gameSettings->CreateSavestate(true);

  CSavestateDatabase db;
  db.GetSavestatesNav(m_savestates, gameSettings->GetPlayingGame(), gameSettings->GameClientID());

  m_savestates.Sort(SortByDate, SortOrderDescending);
}

void CDialogInGameSaves::GetItems(CFileItemList& items)
{
  for (const auto& item : m_savestates)
    items.Add(item);
}

void CDialogInGameSaves::OnItemFocus(unsigned int index)
{
  if (static_cast<int>(index) < m_savestates.Size())
    m_focusedItemIndex = index;
}

unsigned int CDialogInGameSaves::GetFocusedItem() const
{
  if (static_cast<int>(m_focusedItemIndex) < m_savestates.Size())
    return m_focusedItemIndex;

  return 0;
}

void CDialogInGameSaves::PostExit()
{
  m_savestates.Clear();
}

void CDialogInGameSaves::OnClickAction(unsigned int index)
{
  if (static_cast<int>(index) < m_savestates.Size())
  {
    CFileItemPtr savestate = m_savestates.Get(index);
    const std::string& savePath = savestate->GetPath();

    auto gameSettings = CServiceBroker::GetGameRenderManager().RegisterGameSettingsDialog();

    if (savePath.empty())
      gameSettings->CreateSavestate(false);
    else
      gameSettings->LoadSavestate(savePath);

    gameSettings->CloseOSD();
  }
}

void CDialogInGameSaves::OnContextMenu(unsigned int index)
{
  if (static_cast<int>(index) < m_savestates.Size())
  {
    CFileItemPtr savestate = m_savestates.Get(index);

    unsigned int i = 0;
    i++;
  }
}
