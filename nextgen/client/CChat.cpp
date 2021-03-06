#include "CChat.h"
#include "log/log.h"

CChat::CChat(const int iMaxMessages, const int iMaxHistory, const int iMaxMyHistory, const int iFontSize, const wchar_t * pszFontName, const bool bFontBold, const bool bFontItalic) : CD3DManager()
{
	m_pMessages = new MESSAGE*[iMaxHistory];
	memset(m_pMessages, 0, 4 * iMaxHistory);

	m_pMyMsgHistory = new wchar_t*[iMaxMyHistory];
	for(int i = 0; i < iMaxMyHistory; i++)
	{
		m_pMyMsgHistory[i] = new wchar_t[MAX_CHAT_MESSAGE_LENGTH];
		memset(m_pMyMsgHistory[i], 0, MAX_CHAT_MESSAGE_LENGTH * sizeof(wchar_t));
	}

	m_pMyMsg = new wchar_t[MAX_CHAT_MESSAGE_LENGTH];
	memset(m_pMyMsg, 0, MAX_CHAT_MESSAGE_LENGTH * sizeof(wchar_t));

	m_iMaxMessages = iMaxMessages;
	m_iMaxHistory = iMaxHistory;
	m_iMaxMyHistory = iMaxMyHistory;

	m_iScrollPos = 0;
	m_dwCursorPos = 0;
	m_iHistoryPos = -1;
	m_iMessagesCount = 0;

	m_iFontSize = iFontSize;
	m_pszFontName = pszFontName;
	m_bFontBold = bFontBold;
	m_bFontItalic = bFontItalic;

	m_bUserScroll = false;

	m_iFrameWidth = 0;
	m_iFrameHeight = 0;

	m_bEnterMessage = false;
	m_bChatShow = 2;

	this->SetChatColors();
	this->SetChatTransform();

	InitializeCriticalSection(&critSect);
}

CChat::~CChat()
{
	EnterCriticalSection(&critSect);
	for(int i = 0; i < m_iMaxHistory; i++)
	{
		if(m_pMessages[i]) 
		{
			delete m_pMessages[i];
			m_pMessages[i] = NULL;

		}
	}
	delete m_pMessages;
	for(int i = 0; i < m_iMaxMyHistory; i++)
	{
		if(m_pMyMsgHistory[i]) 
		{
			delete m_pMyMsgHistory[i];
			m_pMyMsgHistory[i] = NULL;

		}
	}
	delete m_pMyMsgHistory;
	delete m_pMyMsg;
	LeaveCriticalSection(&critSect);
	DeleteCriticalSection(&critSect);
}

void CChat::SetChatColors(D3DCOLOR dwFrameColor, D3DCOLOR dwScrollColor, D3DCOLOR dwScrollBackgroundColor, D3DCOLOR dwEnterBackgroundColor, D3DCOLOR dwEnterTextColor)
{
	m_dwFrameColor = dwFrameColor;
	m_dwScrollColor = dwScrollColor;
	m_dwScrollBackgroundColor = dwScrollBackgroundColor;
	m_dwEnterBackgroundColor = dwEnterBackgroundColor;
	m_dwEnterTextColor = dwEnterTextColor;
}

void CChat::SetChatTransform(float fPosX, float fPosY)
{
	m_fPosX = fPosX;
	m_fPosY = fPosY;
}

void CChat::OnCreateDevice(IDirect3DDevice9 * pd3dDevice, HWND hWnd)
{
	CD3DManager::OnCreateDevice(pd3dDevice, hWnd);

	RECT rct;
	GetWindowRect(hWnd, &rct);
	m_dwWidth = rct.right - rct.left;
	m_dwHeight = rct.bottom - rct.top;

	EnterCriticalSection(&critSect);

	D3DXCreateSprite(pd3dDevice, &m_pSprite);
	D3DXCreateLine(pd3dDevice, &m_pLine);
	m_pFont = new CFont(pd3dDevice, m_pszFontName, m_iFontSize, m_bFontBold, m_bFontItalic);

	LeaveCriticalSection(&critSect);
}

void CChat::OnLostDevice()
{
	EnterCriticalSection(&critSect);

	if(m_pSprite) m_pSprite->OnLostDevice();
	if(m_pFont) m_pFont->OnLostDevice();
	if(m_pLine) m_pLine->OnLostDevice();

	LeaveCriticalSection(&critSect);
}

void CChat::OnResetDevice()
{
	EnterCriticalSection(&critSect);

	if(m_pSprite) m_pSprite->OnResetDevice();
	if(m_pFont) m_pFont->OnResetDevice();
	if(m_pLine) m_pLine->OnResetDevice();

	LeaveCriticalSection(&critSect);
}

void CChat::OnDraw()
{
	EnterCriticalSection(&critSect);

	if(m_bChatShow == 2)
	{
		m_iFrameHeight = 0;
		m_iFrameWidth = 0;
		for(int i = m_iScrollPos; i < m_iScrollPos + m_iMaxMessages && i < m_iMaxHistory; i++)
		{
			if(m_pMessages[i])
			{
				MESSAGE * pTmpMsg = m_pMessages[i];
				int iX = 0;
				while(pTmpMsg)
				{
					iX += m_pFont->GetTextWidth(pTmpMsg->msg);
					pTmpMsg = pTmpMsg->next;
				}
				if(iX > m_iFrameWidth) m_iFrameWidth = (float)iX;
			}
			else break;
			m_iFrameHeight += m_iFontSize + 2;
		}

		if(m_iFrameWidth > 0 && m_iFrameHeight > 0)
		{
			m_iFrameHeight += 8;
			m_iFrameWidth += 22;

			D3DXVECTOR2 d3dxVector[2];

			m_pLine->SetWidth(m_iFrameWidth);
			m_pLine->Begin();
			d3dxVector[0] = D3DXVECTOR2(m_fPosX + m_iFrameWidth/2, m_fPosY);
			d3dxVector[1] = D3DXVECTOR2(m_fPosX + m_iFrameWidth/2, m_fPosY + m_iFrameHeight);
			m_pLine->Draw(d3dxVector, 2, m_dwFrameColor);
			m_pLine->End();

			m_pLine->SetWidth(10);
			m_pLine->Begin();
			d3dxVector[0] = D3DXVECTOR2(m_fPosX + 6, m_fPosY + 1);
			d3dxVector[1] = D3DXVECTOR2(m_fPosX + 6, m_fPosY + m_iFrameHeight - 2);
			m_pLine->Draw(d3dxVector, 2, m_dwScrollBackgroundColor);
			m_pLine->End();

			float fPos = 0;
			float fScrollHeight = m_iFrameHeight - 4;
			if(m_iMessagesCount > m_iMaxMessages)
			{
				fScrollHeight *= (float)m_iMaxMessages / m_iMessagesCount;
				fPos = ((m_iFrameHeight - 4 - fScrollHeight) / (m_iMessagesCount - m_iMaxMessages)) * m_iScrollPos;
			}

			m_pLine->SetWidth(8);
			m_pLine->Begin();
			d3dxVector[0] = D3DXVECTOR2(m_fPosX + 7, m_fPosY + 2 + fPos);
			d3dxVector[1] = D3DXVECTOR2(m_fPosX + 7, m_fPosY + 2 + fPos + fScrollHeight);
			m_pLine->Draw(d3dxVector, 2, m_dwScrollColor);
			m_pLine->End();
		}
	}

	if(m_bEnterMessage || m_bChatShow >= 1)
	{
		int iY = 1;
		m_pSprite->Begin(D3DXSPRITE_ALPHABLEND);

		if(m_bChatShow >= 1)
		{
			D3DXVECTOR2 vTransformation = D3DXVECTOR2(m_fPosX + 15, m_fPosY + 2);
			D3DXVECTOR2 vScaling(1.0f, 1.0f);

			D3DXMATRIX mainMatrix;
			D3DXMatrixTransformation2D(&mainMatrix, 0, 0, &vScaling, 0, 0, &vTransformation);
			m_pSprite->SetTransform(&mainMatrix);

			for(int i = m_iScrollPos; i < m_iScrollPos + m_iMaxMessages && i < m_iMaxHistory; i++)
			{
				if(m_pMessages[i])
				{
					MESSAGE * pTmpMsg = m_pMessages[i];
					int iX = 1;
					while(pTmpMsg)
					{
						m_pFont->DrawText(pTmpMsg->msg, iX, iY, m_pSprite, pTmpMsg->color);
						iX += m_pFont->GetTextWidth(pTmpMsg->msg);
						pTmpMsg = pTmpMsg->next;
					}
				}
				else break;
				iY += m_iFontSize + 2;
			}
		}

		if(m_bEnterMessage)
		{
			iY += 4;

			int iWidth = m_pFont->GetTextWidth(m_pMyMsg);
			if(iWidth < 100) iWidth = 100;

			m_pLine->SetWidth(20);
			m_pLine->Begin();
			D3DXVECTOR2 d3dxVector[2];
			d3dxVector[0] = D3DXVECTOR2(m_fPosX, m_fPosY + iY + 14);
			d3dxVector[1] = D3DXVECTOR2(m_fPosX + iWidth + 20, m_fPosY + iY + 14);
			m_pLine->Draw(d3dxVector, 2, m_dwEnterBackgroundColor);
			m_pLine->End();

			m_pFont->DrawText(m_pMyMsg, 0, iY + 3, m_pSprite, m_dwEnterTextColor);
		}

		m_pSprite->End();
	}

	LeaveCriticalSection(&critSect);
}

void CChat::OnBeginDraw()
{

}

void CChat::OnRelease()
{
	EnterCriticalSection(&critSect);

	if(m_pSprite) m_pSprite->Release();
	if(m_pFont) m_pFont->Release();
	if(m_pLine) m_pLine->Release();

	LeaveCriticalSection(&critSect);
}

void CChat::ScrollDownHistory()
{
	if(!m_bEnterMessage) return;
	if(m_iHistoryPos >= 0)
		m_iHistoryPos--;

	if(m_iHistoryPos == -1)
	{
		memset(m_pMyMsg, 0, sizeof(wchar_t) * MAX_CHAT_MESSAGE_LENGTH);
		m_pMyMsg[0] = L'█';
		m_dwCursorPos = 0;
		return;
	}

	if(m_pMyMsgHistory[m_iHistoryPos])
	{
		memset(m_pMyMsg, 0, sizeof(wchar_t) * MAX_CHAT_MESSAGE_LENGTH);
		wcscpy_s(m_pMyMsg, MAX_CHAT_MESSAGE_LENGTH, m_pMyMsgHistory[m_iHistoryPos]);
		m_dwCursorPos = wcslen(m_pMyMsg);
		m_pMyMsg[m_dwCursorPos] = L'█';
	}
}

void CChat::ScrollUpHistory()
{
	if(!m_bEnterMessage) return;
	if(!m_iHistoryPos + 1 == m_iMaxMyHistory) return;
	m_iHistoryPos++;

	if(m_pMyMsgHistory[m_iHistoryPos])
	{
		memset(m_pMyMsg, 0, sizeof(wchar_t) * MAX_CHAT_MESSAGE_LENGTH);
		wcscpy_s(m_pMyMsg, MAX_CHAT_MESSAGE_LENGTH, m_pMyMsgHistory[m_iHistoryPos]);
		m_dwCursorPos = wcslen(m_pMyMsg);
		m_pMyMsg[m_dwCursorPos] = L'█';
	}
}

void CChat::MoveCursorLeft()
{
	if(!m_bEnterMessage) return;
	if(m_dwCursorPos <= 0) return;
	m_dwCursorPos--;
	wchar_t sChar = m_pMyMsg[m_dwCursorPos];
	m_pMyMsg[m_dwCursorPos] = m_pMyMsg[m_dwCursorPos + 1];
	m_pMyMsg[m_dwCursorPos + 1] = sChar;
}

void CChat::MoveCursorRight()
{
	if(!m_bEnterMessage) return;
	if(m_dwCursorPos >= wcslen(m_pMyMsg)) return;
	m_dwCursorPos++;
	wchar_t sChar = m_pMyMsg[m_dwCursorPos];
	m_pMyMsg[m_dwCursorPos] = m_pMyMsg[m_dwCursorPos - 1];
	m_pMyMsg[m_dwCursorPos - 1] = sChar;
}

bool CChat::IsMessageEnterActive()
{
	return m_bEnterMessage;
}

bool CChat::IsMessageNotNull()
{
	return (m_pMyMsg[0] != 0);
}

void CChat::ChangeEnterMessageState()
{
	m_bEnterMessage = !m_bEnterMessage;
	if(m_bEnterMessage)
	{
		if(m_pMyMsg[0] == 0)
		{
			m_pMyMsg[0] = L'█';
			m_dwCursorPos = 0;
		}
	}
	else
	{
		if(m_dwCursorPos == 0)
		{
			m_pMyMsg[0] = 0;
		}
	}
	m_iHistoryPos = -1;
}

void CChat::ChangeChatState()
{
	m_bChatShow++;
	if(m_bChatShow > 2) m_bChatShow = 0;
}

void CChat::AddCharToMessage(wchar_t sChar)
{
	if(!m_bEnterMessage) return;
	if(sChar < 32) return;

	int len = wcslen(m_pMyMsg);
	for(DWORD i = (len < (MAX_CHAT_MESSAGE_LENGTH - 1) ? len : (MAX_CHAT_MESSAGE_LENGTH - 1)); i >= m_dwCursorPos + 1; i--)
	{
		m_pMyMsg[i] = m_pMyMsg[i - 1]; 
	}
	m_pMyMsg[m_dwCursorPos] = sChar;
	m_dwCursorPos++;
}

void CChat::DeleteCharFromMessage()
{
	if(!m_bEnterMessage) return;
	if(m_dwCursorPos == 0) return;
	int len = wcslen(m_pMyMsg);
	for(int i = m_dwCursorPos; i <= len && i < MAX_CHAT_MESSAGE_LENGTH; i++)
	{
		m_pMyMsg[i - 1] = m_pMyMsg[i]; 
	}
	m_dwCursorPos--;
}

void CChat::GetMyMessage(wchar_t * pszMsg)
{
	int len = wcslen(m_pMyMsg);
	for(int i = m_dwCursorPos + 1; i <= len && i < MAX_CHAT_MESSAGE_LENGTH; i++)
	{
		m_pMyMsg[i-1] = m_pMyMsg[i]; 
	}

	for(int i = m_iMaxMyHistory - 2; i >= 0; i--)
	{
		memcpy(m_pMyMsgHistory[i+1], m_pMyMsgHistory[i], sizeof(wchar_t) * MAX_CHAT_MESSAGE_LENGTH);
	}

	
	memcpy(m_pMyMsgHistory[0], m_pMyMsg, sizeof(wchar_t) * MAX_CHAT_MESSAGE_LENGTH);
	wcscpy_s(pszMsg, MAX_CHAT_MESSAGE_LENGTH, m_pMyMsg);

	memset(m_pMyMsg, 0, sizeof(wchar_t) * MAX_CHAT_MESSAGE_LENGTH);
	m_dwCursorPos = 0;
}

void CChat::AddMessage(wchar_t * pszMsg)
{
	if(!pszMsg) return;

	// Color: 0001 AARR GGBB

	EnterCriticalSection(&critSect);

	MESSAGE ** ppTmpMsg = NULL;
	for(int i = 0; i < m_iMaxHistory; i++)
	{
		if(!m_pMessages[i]) 
		{
			ppTmpMsg = &m_pMessages[i];
			break;
		}
	}

	if(!ppTmpMsg)
	{
		delete m_pMessages[0];
		m_pMessages[0] = NULL;

		for(int i = 1; i < m_iMaxHistory; i++)
		{
			m_pMessages[i - 1] = m_pMessages[i];
			m_pMessages[i] = NULL;
		}
		ppTmpMsg = &m_pMessages[m_iMaxHistory - 1];
	}

	if(!ppTmpMsg)
	{
		Log::Error("Can't add message to chat");
		LeaveCriticalSection(&critSect);
		return;
	}

	wchar_t * szTmpTxt = pszMsg;
	D3DCOLOR Color = 0xFFFFFFFF;

	int i = 0;
	while(pszMsg[i] != NULL)
	{
		if(pszMsg[i] == 1)
		{
			D3DCOLOR tColor = Color;
			Color = *(DWORD *)(pszMsg + i + 1);

			pszMsg[i] = 0;
			pszMsg[i + 1] = 0;
			pszMsg[i + 2] = 0;

			if(i != 0)
			{
				*ppTmpMsg = new MESSAGE(szTmpTxt, tColor);
				ppTmpMsg = &(*ppTmpMsg)->next;
			}

			i += 3;
			
			szTmpTxt = pszMsg + i;

			i++;
			continue;
		}
		i++;
	}

	if(szTmpTxt[0] != 0)
	{
		*ppTmpMsg = new MESSAGE(szTmpTxt, Color);
	}

	if(m_iMessagesCount < m_iMaxHistory)
		m_iMessagesCount++;

	if(!m_bUserScroll) this->ScrollEnd();
	LeaveCriticalSection(&critSect);
}

void CChat::DeleteMessage(const int index)
{
	if(index < 0 || index > m_iMaxHistory) return;

	EnterCriticalSection(&critSect);
	if(m_pMessages[index]) 
	{
		delete m_pMessages[index];
		m_pMessages[index] = NULL;
	}
	if(m_iMessagesCount > 0)
		m_iMessagesCount--;
	LeaveCriticalSection(&critSect);
}

void CChat::Clear()
{
	EnterCriticalSection(&critSect);
	for(int i = 0; i < m_iMaxHistory; i++)
	{
		delete m_pMessages[i];
		m_pMessages[i] = NULL;
	}
	m_iMessagesCount = 0;
	this->ScrollEnd();
	LeaveCriticalSection(&critSect);
}

bool CChat::ScrollDown()
{
	if(m_iScrollPos + m_iMaxMessages >= m_iMaxHistory) return false;
	if(!m_pMessages[m_iScrollPos + m_iMaxMessages]) return false;

	m_bUserScroll = true;
	m_iScrollPos++;

	int i = 0;
	for(i = 0; i < m_iMaxHistory; i++)
	{
		if(!m_pMessages[i]) break;
	}

	if(i - m_iMaxMessages < 0) 
	{
		if(m_iScrollPos == 0) m_bUserScroll = false;
	}
	else
	{
		if(m_iScrollPos == i - m_iMaxMessages) m_bUserScroll = false;
	}

	return true;
}

bool CChat::ScrollUp()
{
	if(m_iScrollPos <= 0) return false;
	m_bUserScroll = true;
	m_iScrollPos--;
	return true;
}

void CChat::ScrollEnd()
{
	m_bUserScroll = false;

	int i = 0;
	for(i = 0; i < m_iMaxHistory; i++)
	{
		if(!m_pMessages[i]) break;
	}

	if(i - m_iMaxMessages < 0) 
	{
		m_iScrollPos = 0;
	}
	else
	{
		m_iScrollPos = i - m_iMaxMessages;
	}
}