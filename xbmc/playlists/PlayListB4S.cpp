/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlayListB4S.h"

#include "Util.h"
#include "filesystem/File.h"
#include "music/tags/MusicInfoTag.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/XBMCTinyXML.h"
#include "utils/XMLUtils.h"
#include "utils/log.h"

#include <iostream>
#include <string>

using namespace XFILE;
using namespace PLAYLIST;

/* ------------------------ example b4s playlist file ---------------------------------
 <?xml version="1.0" encoding='UTF-8' standalone="yes"?>
 <WinampXML>
 <!-- Generated by: Nullsoft Winamp3 version 3.0d -->
  <playlist num_entries="2" label="Playlist 001">
   <entry Playstring="file:E:\Program Files\Winamp3\demo.mp3">
    <Name>demo</Name>
    <Length>5982</Length>
   </entry>
   <entry Playstring="file:E:\Program Files\Winamp3\demo.mp3">
    <Name>demo</Name>
    <Length>5982</Length>
   </entry>
  </playlist>
 </WinampXML>
------------------------ end of example b4s playlist file ---------------------------------*/
CPlayListB4S::CPlayListB4S(void) = default;

CPlayListB4S::~CPlayListB4S(void) = default;


bool CPlayListB4S::LoadData(std::istream& stream)
{
  CXBMCTinyXML xmlDoc;

  stream >> xmlDoc;

  if (xmlDoc.Error())
  {
    CLog::Log(LOGERROR, "Unable to parse B4S info Error: {}", xmlDoc.ErrorDesc());
    return false;
  }

  TiXmlElement* pRootElement = xmlDoc.RootElement();
  if (!pRootElement ) return false;

  TiXmlElement* pPlayListElement = pRootElement->FirstChildElement("playlist");
  if (!pPlayListElement ) return false;
  m_strPlayListName = XMLUtils::GetAttribute(pPlayListElement, "label");

  TiXmlElement* pEntryElement = pPlayListElement->FirstChildElement("entry");

  if (!pEntryElement) return false;
  while (pEntryElement)
  {
    std::string strFileName = XMLUtils::GetAttribute(pEntryElement, "Playstring");
    size_t iColon = strFileName.find(':');
    if (iColon != std::string::npos)
    {
      iColon++;
      strFileName.erase(0, iColon);
    }
    if (strFileName.size())
    {
      TiXmlNode* pNodeInfo = pEntryElement->FirstChild("Name");
      TiXmlNode* pNodeLength = pEntryElement->FirstChild("Length");
      long lDuration = 0;
      if (pNodeLength)
      {
        lDuration = atol(pNodeLength->FirstChild()->Value());
      }
      if (pNodeInfo)
      {
        std::string strInfo = pNodeInfo->FirstChild()->Value();
        strFileName = URIUtils::SubstitutePath(strFileName);
        CUtil::GetQualifiedFilename(m_strBasePath, strFileName);
        CFileItemPtr newItem(new CFileItem(strInfo));
        newItem->SetPath(strFileName);
        newItem->GetMusicInfoTag()->SetDuration(lDuration);
        Add(newItem);
      }
    }
    pEntryElement = pEntryElement->NextSiblingElement();
  }
  return true;
}

void CPlayListB4S::Save(const std::string& strFileName) const
{
  if (!m_vecItems.size()) return ;
  std::string strPlaylist = strFileName;
  strPlaylist = CUtil::MakeLegalPath(strPlaylist);
  CFile file;
  if (!file.OpenForWrite(strPlaylist, true))
  {
    CLog::Log(LOGERROR, "Could not save B4S playlist: [{}]", strPlaylist.c_str());
    return ;
  }
  std::string write;
  write += StringUtils::Format("<?xml version={}1.0{} encoding='UTF-8' standalone={}yes{}?>\n", 34,
                               34, 34, 34);
  write += StringUtils::Format("<WinampXML>\n");
  write += StringUtils::Format("  <playlist num_entries=\"{0}\" label=\"{1}\">\n",
                               m_vecItems.size(), m_strPlayListName);
  for (int i = 0; i < (int)m_vecItems.size(); ++i)
  {
    const CFileItemPtr item = m_vecItems[i];
    write += StringUtils::Format("    <entry Playstring={}file:{}{}>\n", 34, item->GetPath(), 34);
    write += StringUtils::Format("      <Name>{}</Name>\n", item->GetLabel().c_str());
    write +=
        StringUtils::Format("      <Length>{}</Length>\n", item->GetMusicInfoTag()->GetDuration());
  }
  write += StringUtils::Format("  </playlist>\n");
  write += StringUtils::Format("</WinampXML>\n");
  file.Write(write.c_str(), write.size());
  file.Close();
}
