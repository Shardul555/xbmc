/*
 *  Copyright (C) 2016-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIDialogSelectGameClient.h"

#include "dialogs/GUIDialogSelect.h"
#include "filesystem/AddonsDirectory.h"
#include "games/addons/GameClient.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/WindowIDs.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

using namespace KODI;
using namespace KODI::MESSAGING;
using namespace GAME;

std::string CGUIDialogSelectGameClient::ShowAndGetGameClient(const std::string& gamePath,
                                                             const GameClientVector& candidates,
                                                             const GameClientVector& installable)
{
  std::string gameClient;

  LogGameClients(candidates, installable);

  std::string extension = URIUtils::GetExtension(gamePath);

  // "Select emulator for {0:s}"
  CGUIDialogSelect* dialog =
      GetDialog(StringUtils::Format(g_localizeStrings.Get(35258), extension));
  if (dialog != nullptr)
  {
    // Turn the addons into items
    CFileItemList items;
    CFileItemList installableItems;
    for (const auto& candidate : candidates)
    {
      CFileItemPtr item(XFILE::CAddonsDirectory::FileItemFromAddon(candidate, candidate->ID()));
      item->SetLabel2(g_localizeStrings.Get(35257)); // "Installed"
      items.Add(std::move(item));
    }
    for (const auto& addon : installable)
    {
      CFileItemPtr item(XFILE::CAddonsDirectory::FileItemFromAddon(addon, addon->ID()));
      installableItems.Add(std::move(item));
    }
    items.Sort(SortByLabel, SortOrderAscending);
    installableItems.Sort(SortByLabel, SortOrderAscending);

    items.Append(installableItems);

    dialog->SetItems(items);

    dialog->Open();

    // If the "Get More" button has been pressed, show a list of installable addons
    if (dialog->IsConfirmed())
    {
      int selectedIndex = dialog->GetSelectedItem();

      if (0 <= selectedIndex && selectedIndex < items.Size())
      {
        gameClient = items[selectedIndex]->GetPath();

        CLog::Log(LOGDEBUG, "Select game client dialog: User selected emulator %s",
                  gameClient.c_str());
      }
      else
      {
        CLog::Log(LOGDEBUG, "Select game client dialog: User selected invalid emulator %d",
                  selectedIndex);
      }
    }
    else
    {
      CLog::Log(LOGDEBUG, "Select game client dialog: User cancelled game client installation");
    }
  }

  return gameClient;
}

bool CGUIDialogSelectGameClient::Install(const std::string& gameClient)
{
  // If the addon isn't installed we need to install it
  bool bInstalled = CServiceBroker::GetAddonMgr().IsAddonInstalled(gameClient);
  if (!bInstalled)
  {
    ADDON::AddonPtr installedAddon;
    bInstalled =
        ADDON::CAddonInstaller::GetInstance().InstallModal(gameClient, installedAddon, false);
    if (!bInstalled)
    {
      CLog::Log(LOGERROR, "Select game client dialog: Failed to install %s", gameClient.c_str());
      // "Error"
      // "Failed to install add-on."
      HELPERS::ShowOKDialogText(257, 35256);
    }
  }

  return bInstalled;
}

bool CGUIDialogSelectGameClient::Enable(const std::string& gameClient)
{
  bool bSuccess = true;

  if (CServiceBroker::GetAddonMgr().IsAddonDisabled(gameClient))
    bSuccess = CServiceBroker::GetAddonMgr().EnableAddon(gameClient);

  return bSuccess;
}

CGUIDialogSelect* CGUIDialogSelectGameClient::GetDialog(const std::string& title)
{
  CGUIDialogSelect* dialog =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(
          WINDOW_DIALOG_SELECT);
  if (dialog != nullptr)
  {
    dialog->Reset();
    dialog->SetHeading(CVariant{title});
    dialog->SetUseDetails(true);
  }

  return dialog;
}

void CGUIDialogSelectGameClient::LogGameClients(const GameClientVector& candidates,
                                                const GameClientVector& installable)
{
  CLog::Log(LOGDEBUG, "Select game client dialog: Found %u candidates",
            static_cast<unsigned int>(candidates.size()));
  for (const auto& gameClient : candidates)
    CLog::Log(LOGDEBUG, "Adding %s as a candidate", gameClient->ID().c_str());

  if (!installable.empty())
  {
    CLog::Log(LOGDEBUG, "Select game client dialog: Found %u installable clients",
              static_cast<unsigned int>(installable.size()));
    for (const auto& gameClient : installable)
      CLog::Log(LOGDEBUG, "Adding %s as an installable client", gameClient->ID().c_str());
  }
}
