/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RetroPlayer.h"

#include "FileItem.h"
#include "GUIInfoManager.h"
#include "RetroPlayerAutoSave.h"
#include "RetroPlayerInput.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "addons/AddonManager.h"
#include "cores/DataCacheCore.h"
#include "cores/IPlayerCallback.h"
#include "cores/RetroPlayer/cheevos/Cheevos.h"
#include "cores/RetroPlayer/guibridge/GUIGameRenderManager.h"
#include "cores/RetroPlayer/guiplayback/GUIPlaybackControl.h"
#include "cores/RetroPlayer/playback/IPlayback.h"
#include "cores/RetroPlayer/playback/RealtimePlayback.h"
#include "cores/RetroPlayer/playback/ReversiblePlayback.h"
#include "cores/RetroPlayer/process/RPProcessInfo.h"
#include "cores/RetroPlayer/rendering/RPRenderManager.h"
#include "cores/RetroPlayer/savestates/ISavestate.h"
#include "cores/RetroPlayer/savestates/SavestateDatabase.h"
#include "cores/RetroPlayer/streams/RPStreamManager.h"
#include "dialogs/GUIDialogYesNo.h"
#include "games/GameServices.h"
#include "games/GameSettings.h"
#include "games/GameUtils.h"
#include "games/addons/GameClient.h"
#include "games/addons/input/GameClientInput.h"
#include "games/tags/GameInfoTag.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/WindowIDs.h"
#include "input/actions/Action.h"
#include "input/actions/ActionIDs.h"
#include "interfaces/AnnouncementManager.h"
#include "messaging/ApplicationMessenger.h"
#include "settings/MediaSettings.h"
#include "threads/SingleLock.h"
#include "utils/JobManager.h"
#include "utils/MathUtils.h"
#include "utils/StringUtils.h"
#include "utils/log.h"
#include "windowing/WinSystem.h"

using namespace KODI;
using namespace GAME;
using namespace RETRO;

CRetroPlayer::CRetroPlayer(IPlayerCallback& callback)
  : IPlayer(callback),
    m_gameServices(CServiceBroker::GetGameServices()),
    m_fileItem(new CFileItem())
{
  ResetPlayback();
  CServiceBroker::GetWinSystem()->RegisterRenderLoop(this);
  CServiceBroker::GetAnnouncementManager()->AddAnnouncer(this);
}

CRetroPlayer::~CRetroPlayer()
{
  CServiceBroker::GetWinSystem()->UnregisterRenderLoop(this);
  CServiceBroker::GetAnnouncementManager()->RemoveAnnouncer(this);
  CloseFile();
}

bool CRetroPlayer::OpenFile(const CFileItem& file, const CPlayerOptions& options)
{
  *m_fileItem = file;

  std::string savestatePath;

  // When playing a game, set the game client that we'll use to open the game.
  // This will prompt the user to select a savestate if there are any.
  // If there are no savestates, or the user wants to create a new savestate
  // it will prompt the user to select a game client
  if (!GAME::CGameUtils::FillInGameClient(*m_fileItem, savestatePath))
  {
    CLog::Log(LOGINFO,
              "RetroPlayer[PLAYER]: No compatible game client selected, aborting playback");
    return false;
  }

  // Check if we should open in standalone mode
  const bool bStandalone = m_fileItem->GetPath().empty();

  m_processInfo.reset(CRPProcessInfo::CreateInstance());
  if (!m_processInfo)
  {
    CLog::Log(LOGERROR, "RetroPlayer[PLAYER]: Failed to create - no process info registered");
    return false;
  }

  m_processInfo->SetDataCache(&CServiceBroker::GetDataCacheCore());
  m_processInfo->ResetInfo();

  m_renderManager.reset(new CRPRenderManager(*m_processInfo));

  CSingleLock lock(m_mutex);

  if (IsPlaying())
    CloseFile();

  PrintGameInfo(*m_fileItem);

  bool bSuccess = false;

  std::string gameClientId = m_fileItem->GetGameInfoTag()->GetGameClient();

  ADDON::AddonPtr addon;
  if (gameClientId.empty())
  {
    CLog::Log(LOGERROR, "RetroPlayer[PLAYER]: Can't play game, no game client was passed!");
  }
  else if (!CServiceBroker::GetAddonMgr().GetAddon(gameClientId, addon, ADDON::ADDON_GAMEDLL,
                                                   ADDON::OnlyEnabled::YES))
  {
    CLog::Log(LOGERROR, "RetroPlayer[PLAYER]: Can't find add-on %s for game file!",
              gameClientId.c_str());
  }
  else
  {
    m_gameClient = std::static_pointer_cast<CGameClient>(addon);
    if (m_gameClient->Initialize())
    {
      m_streamManager.reset(new CRPStreamManager(*m_renderManager, *m_processInfo));

      m_input.reset(new CRetroPlayerInput(CServiceBroker::GetPeripherals()));

      if (!bStandalone)
      {
        std::string redactedPath = CURL::GetRedacted(m_fileItem->GetPath());
        CLog::Log(LOGINFO, "RetroPlayer[PLAYER]: Opening: %s", redactedPath.c_str());
        bSuccess = m_gameClient->OpenFile(*m_fileItem, *m_streamManager, m_input.get());
      }
      else
      {
        CLog::Log(LOGINFO, "RetroPlayer[PLAYER]: Opening standalone");
        bSuccess = m_gameClient->OpenStandalone(*m_streamManager, m_input.get());
      }

      if (bSuccess)
        CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Using game client %s", gameClientId.c_str());
      else
        CLog::Log(LOGERROR, "RetroPlayer[PLAYER]: Failed to open file using %s",
                  gameClientId.c_str());
    }
    else
      CLog::Log(LOGERROR, "RetroPlayer[PLAYER]: Failed to initialize %s", gameClientId.c_str());
  }

  if (bSuccess && !bStandalone)
  {
    CSavestateDatabase savestateDb;

    std::unique_ptr<ISavestate> save = CSavestateDatabase::AllocateSavestate();
    if (savestateDb.GetSavestate(savestatePath, *save))
    {
      // Check if game client is the same
      if (save->GameClientID() != m_gameClient->ID())
      {
        ADDON::AddonPtr addon;
        if (CServiceBroker::GetAddonMgr().GetAddon(save->GameClientID(), addon,
                                                   ADDON::ADDON_UNKNOWN, ADDON::OnlyEnabled::YES))
        {
          // Warn the user that continuing with a different game client will
          // overwrite the save
          bool dummy;
          if (!CGUIDialogYesNo::ShowAndGetInput(
                  438, StringUtils::Format(g_localizeStrings.Get(35217), addon->Name()), dummy, 222,
                  35218, 0))
            bSuccess = false;
        }
      }
    }
  }

  if (bSuccess)
  {
    // Switch to fullscreen
    MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_SWITCHTOFULLSCREEN);

    m_cheevos.reset(new CCheevos(m_gameClient.get(), *m_fileItem,
                                 m_gameServices.GameSettings().RAUsername(),
                                 m_gameServices.GameSettings().RAToken()));

    m_cheevos->EnableRichPresence();

    // Calls to external code could mutate file item, so make a copy
    CFileItem fileCopy(*m_fileItem);

    // Initialize gameplay
    CreatePlayback(m_gameServices.GameSettings().AutosaveEnabled(), savestatePath);
    RegisterWindowCallbacks();
    m_playbackControl.reset(new CGUIPlaybackControl(*this));
    m_callback.OnPlayBackStarted(fileCopy);
    m_callback.OnAVStarted(fileCopy);
    if (!bStandalone)
      m_autoSave.reset(new CRetroPlayerAutoSave(*this, m_gameServices.GameSettings()));

    // Set video framerate
    m_processInfo->SetVideoFps(static_cast<float>(m_gameClient->GetFrameRate()));
  }
  else
  {
    m_input.reset();
    m_streamManager.reset();
    if (m_gameClient)
      m_gameClient->Unload();
    m_gameClient.reset();
  }

  return bSuccess;
}

bool CRetroPlayer::CloseFile(bool reopen /* = false */)
{
  CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Closing file");

  m_autoSave.reset();

  UnregisterWindowCallbacks();

  m_playbackControl.reset();

  CSingleLock lock(m_mutex);

  if (m_gameClient && m_gameServices.GameSettings().AutosaveEnabled())
  {
    std::string savePath = m_playback->CreateSavestate(true);
    if (!savePath.empty())
      CLog::Log(LOGDEBUG, "RetroPlayer[SAVE]: Saved state to %s",
                CURL::GetRedacted(savePath).c_str());
    else
      CLog::Log(LOGDEBUG, "RetroPlayer[SAVE]: Failed to save state at close");
  }

  m_playback.reset();

  if (m_gameClient)
    m_gameClient->CloseFile();

  m_input.reset();
  m_streamManager.reset();

  m_cheevos.reset();

  if (m_gameClient)
    m_gameClient->Unload();
  m_gameClient.reset();

  m_renderManager.reset();
  m_processInfo.reset();

  CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Playback ended");
  m_callback.OnPlayBackEnded();

  return true;
}

bool CRetroPlayer::IsPlaying() const
{
  if (m_gameClient)
    return m_gameClient->IsPlaying();
  return false;
}

bool CRetroPlayer::CanPause()
{
  return m_playback->CanPause();
}

void CRetroPlayer::Pause()
{
  if (!CanPause())
    return;

  float speed;

  if (m_playback->GetSpeed() == 0.0)
    speed = 1.0f;
  else
    speed = 0.0f;

  SetSpeed(speed);
}

bool CRetroPlayer::CanSeek()
{
  return m_playback->CanSeek();
}

void CRetroPlayer::Seek(bool bPlus /* = true */,
                        bool bLargeStep /* = false */,
                        bool bChapterOverride /* = false */)
{
  if (!CanSeek())
    return;

  if (m_gameClient)
  {
    //! @todo
    /*
    if (bPlus)
    {
      if (bLargeStep)
        m_playback->BigSkipForward();
      else
        m_playback->SmallSkipForward();
    }
    else
    {
      if (bLargeStep)
        m_playback->BigSkipBackward();
      else
        m_playback->SmallSkipBackward();
    }
    */
  }
}

void CRetroPlayer::SeekPercentage(float fPercent /* = 0 */)
{
  if (!CanSeek())
    return;

  if (fPercent < 0.0f)
    fPercent = 0.0f;
  else if (fPercent > 100.0f)
    fPercent = 100.0f;

  uint64_t totalTime = GetTotalTime();
  if (totalTime != 0)
    SeekTime(static_cast<int64_t>(totalTime * fPercent / 100.0f));
}

float CRetroPlayer::GetCachePercentage()
{
  const float cacheMs = static_cast<float>(m_playback->GetCacheTimeMs());
  const float totalMs = static_cast<float>(m_playback->GetTotalTimeMs());

  if (totalMs != 0.0f)
    return cacheMs / totalMs * 100.0f;

  return 0.0f;
}

void CRetroPlayer::SetMute(bool bOnOff)
{
  if (m_streamManager)
    m_streamManager->EnableAudio(!bOnOff);
}

void CRetroPlayer::SeekTime(int64_t iTime /* = 0 */)
{
  if (!CanSeek())
    return;

  m_playback->SeekTimeMs(static_cast<unsigned int>(iTime));
}

bool CRetroPlayer::SeekTimeRelative(int64_t iTime)
{
  if (!CanSeek())
    return false;

  SeekTime(GetTime() + iTime);

  return true;
}

uint64_t CRetroPlayer::GetTime()
{
  return m_playback->GetTimeMs();
}

uint64_t CRetroPlayer::GetTotalTime()
{
  return m_playback->GetTotalTimeMs();
}

void CRetroPlayer::SetSpeed(float speed)
{
  if (m_playback->GetSpeed() != speed)
  {
    if (speed == 1.0f)
      m_callback.OnPlayBackResumed();
    else if (speed == 0.0f)
      m_callback.OnPlayBackPaused();

    SetSpeedInternal(static_cast<double>(speed));

    if (speed == 0.0f)
    {
      const int dialogId = CServiceBroker::GetGUI()->GetWindowManager().GetActiveWindowOrDialog();
      if (dialogId == WINDOW_FULLSCREEN_GAME)
      {
        CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Opening OSD via speed change (%f)", speed);
        OpenOSD();
      }
    }
    else
    {
      CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Closing OSD via speed change (%f)", speed);
      CloseOSD();
    }
  }
}

bool CRetroPlayer::OnAction(const CAction& action)
{
  switch (action.GetID())
  {
    case ACTION_PLAYER_RESET:
    {
      if (m_gameClient)
      {
        float speed = static_cast<float>(m_playback->GetSpeed());

        m_playback->SetSpeed(0.0);

        CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Sending reset command via ACTION_PLAYER_RESET");
        m_cheevos->ResetRuntime();
        m_gameClient->Input().HardwareReset();

        // If rewinding or paused, begin playback
        if (speed <= 0.0f)
          speed = 1.0f;

        SetSpeed(speed);
      }
      return true;
    }
    case ACTION_SHOW_OSD:
    {
      if (m_gameClient)
      {
        CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Closing OSD via ACTION_SHOW_OSD");
        CloseOSD();
        return true;
      }
    }
    default:
      break;
  }

  return false;
}

std::string CRetroPlayer::GetPlayerState()
{
  std::string savestatePath;

  if (m_autoSave)
  {
    savestatePath = m_playback->CreateSavestate(true);
    if (savestatePath.empty())
    {
      CLog::Log(LOGDEBUG, "RetroPlayer[SAVE]: Continuing without saving");
      m_autoSave.reset();
    }
  }
  return savestatePath;
}

bool CRetroPlayer::SetPlayerState(const std::string& state)
{
  return m_playback->LoadSavestate(state);
}

void CRetroPlayer::FrameMove()
{
  if (m_renderManager)
    m_renderManager->FrameMove();

  if (m_playbackControl)
    m_playbackControl->FrameMove();

  if (m_processInfo)
    m_processInfo->SetPlayTimes(0, GetTime(), 0, GetTotalTime());
}

void CRetroPlayer::Render(bool clear, uint32_t alpha /* = 255 */, bool gui /* = true */)
{
  // Performed by callbacks
}

bool CRetroPlayer::IsRenderingVideo()
{
  return true;
}

bool CRetroPlayer::HasGameAgent()
{
  if (m_gameClient)
    return m_gameClient->Input().HasAgent();

  return false;
}

std::string CRetroPlayer::GameClientID() const
{
  if (m_gameClient)
    return m_gameClient->ID();

  return "";
}

std::string CRetroPlayer::GetPlayingGame() const
{
  if (m_gameClient)
    return m_gameClient->GetGamePath();

  return "";
}

std::string CRetroPlayer::CreateSavestate(bool autosave)
{
  return m_playback->CreateSavestate(autosave);
}

bool CRetroPlayer::LoadSavestate(const std::string& path)
{
  if (m_playback)
    return m_playback->LoadSavestate(path);

  return false;
}

void CRetroPlayer::CloseOSDCallback()
{
  CloseOSD();
}

void CRetroPlayer::SetPlaybackSpeed(double speed)
{
  if (m_playback)
  {
    if (m_playback->GetSpeed() != speed)
    {
      if (speed == 1.0)
      {
        IPlayerCallback* callback = &m_callback;
        CJobManager::GetInstance().Submit([callback]() { callback->OnPlayBackResumed(); },
                                          CJob::PRIORITY_NORMAL);
      }
      else if (speed == 0.0)
      {
        IPlayerCallback* callback = &m_callback;
        CJobManager::GetInstance().Submit([callback]() { callback->OnPlayBackPaused(); },
                                          CJob::PRIORITY_NORMAL);
      }
    }
  }

  SetSpeedInternal(speed);
}

void CRetroPlayer::EnableInput(bool bEnable)
{
  if (m_input)
    m_input->EnableInput(bEnable);
}

bool CRetroPlayer::IsAutoSaveEnabled() const
{
  return m_playback->GetSpeed() > 0.0;
}

std::string CRetroPlayer::CreateAutosave()
{
  return m_playback->CreateSavestate(true);
}

void CRetroPlayer::Announce(ANNOUNCEMENT::AnnouncementFlag flag,
                            const std::string& sender,
                            const std::string& message,
                            const CVariant& data)
{
  // Announce() is called at the end of a chain to update the currently-playing
  // file.
  //
  // Updates to current file metadata should find their way to CApplication.
  // This can be accomplished off-thread by sending a TMSG_SET_PLAYER_ITEM
  // message via app messenger.
  //
  // When CApplication receives a file metadata update, it in turn updates
  // the state of CGUIInfoManager.
  //
  // CGUIInfoManager fires an "OnChanged" event for the info update. Publishers
  // of file item metadata should be subscribed to this event to receive info
  // updates from other publishers, as well as info added by CApplication
  // during the playback lifecycle.

  if (flag == ANNOUNCEMENT::Info && message == "OnChanged")
  {
    const CGUIInfoManager& infoMgr = CServiceBroker::GetGUI()->GetInfoManager();
    *m_fileItem = infoMgr.GetCurrentItem();
  }
}

void CRetroPlayer::SetSpeedInternal(double speed)
{
  OnSpeedChange(speed);

  if (speed == 0.0)
    m_playback->PauseAsync();
  else
    m_playback->SetSpeed(speed);
}

void CRetroPlayer::OnSpeedChange(double newSpeed)
{
  m_streamManager->EnableAudio(newSpeed == 1.0);
  m_input->SetSpeed(newSpeed);
  m_renderManager->SetSpeed(newSpeed);
  m_processInfo->SetSpeed(static_cast<float>(newSpeed));
}

void CRetroPlayer::CreatePlayback(bool bRestoreState, const std::string& savestatePath)
{
  if (m_gameClient->RequiresGameLoop())
  {
    m_playback->Deinitialize();
    m_playback.reset(new CReversiblePlayback(m_gameClient.get(), *m_renderManager, m_cheevos.get(),
                                             m_gameClient->GetFrameRate(),
                                             m_gameClient->GetSerializeSize()));
  }
  else
    ResetPlayback();

  if (bRestoreState)
  {
    const bool bStandalone = m_gameClient->GetGamePath().empty();
    if (!bStandalone)
    {
      CLog::Log(LOGDEBUG, "RetroPlayer[SAVE]: Loading savestate");

      if (!SetPlayerState(savestatePath))
        CLog::Log(LOGERROR, "RetroPlayer[SAVE]: Failed to load savestate");
    }
  }

  m_playback->Initialize();
}

void CRetroPlayer::ResetPlayback()
{
  // Called from the constructor, m_playback might not be initialized
  if (m_playback)
    m_playback->Deinitialize();

  m_playback.reset(new CRealtimePlayback);
}

void CRetroPlayer::OpenOSD()
{
  CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_DIALOG_GAME_OSD);
}

void CRetroPlayer::CloseOSD()
{
  CServiceBroker::GetGUI()->GetWindowManager().CloseDialogs(true);
}

void CRetroPlayer::RegisterWindowCallbacks()
{
  m_gameServices.GameRenderManager().RegisterPlayer(m_renderManager->GetGUIRenderTargetFactory(),
                                                    m_renderManager.get(), this);
}

void CRetroPlayer::UnregisterWindowCallbacks()
{
  m_gameServices.GameRenderManager().UnregisterPlayer();
}

void CRetroPlayer::PrintGameInfo(const CFileItem& file) const
{
  const CGameInfoTag* tag = file.GetGameInfoTag();
  if (tag)
  {
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: ---------------------------------------");
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Game tag loaded");
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: URL: %s", tag->GetURL().c_str());
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Title: %s", tag->GetTitle().c_str());
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Platform: %s", tag->GetPlatform().c_str());
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Genres: %s",
              StringUtils::Join(tag->GetGenres(), ", ").c_str());
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Developer: %s", tag->GetDeveloper().c_str());
    if (tag->GetYear() > 0)
      CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Year: %u", tag->GetYear());
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Game Code: %s", tag->GetID().c_str());
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Region: %s", tag->GetRegion().c_str());
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Publisher: %s", tag->GetPublisher().c_str());
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Format: %s", tag->GetFormat().c_str());
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Cartridge type: %s", tag->GetCartridgeType().c_str());
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: Game client: %s", tag->GetGameClient().c_str());
    CLog::Log(LOGDEBUG, "RetroPlayer[PLAYER]: ---------------------------------------");
  }
}
