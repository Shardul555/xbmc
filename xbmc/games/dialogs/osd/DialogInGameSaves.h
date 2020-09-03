/*
 *  Copyright (C) 2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "DialogGameVideoSelect.h"
#include "FileItem.h"

namespace KODI
{
namespace GAME
{
class CDialogInGameSaves : public CDialogGameVideoSelect
{
public:
  CDialogInGameSaves();
  ~CDialogInGameSaves() override = default;

protected:
  // implementation of CDialogGameVideoSelect
  std::string GetHeading() override;
  void PreInit() override;
  void GetItems(CFileItemList& items) override;
  void OnItemFocus(unsigned int index) override;
  void OnClickAction(unsigned int index) override;
  void OnContextMenu(unsigned int index) override;
  unsigned int GetFocusedItem() const override;
  void PostExit() override;

private:
  void InitSavedGames();

  CFileItemList m_savestates;
  unsigned int m_focusedItemIndex = 0;
};
} // namespace GAME
} // namespace KODI
