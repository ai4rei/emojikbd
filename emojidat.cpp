
#include <cwchar>
#include <string>

#include <windows.h>
#include <tchar.h>

#include <btypes.h>
#include <bvwide.h>
#include <tstring.h>
#include <w32co.h>

#include "3rd/sqlite3/sqlite3.h"

#include "emojidat.h"

#ifndef CP_UNICODE
#define CP_UNICODE 1200  /* mimeole.h et.al. */
#endif  /* CP_UNICODE */

#define TABLE_EMOJI         "`emoji`"
#define TABLE_EMOJI_KEYWORD "`emoji_keyword`"
#define TABLE_GROUP         "`group`"
#define TABLE_KEYWORD       "`keyword`"
#define TABLE_SUBGROUP      "`subgroup`"

HRESULT Sqlite3ToHResult(int const nErrorCode)
{
    switch(nErrorCode)
    {
    // non-error
    case SQLITE_OK: return S_OK;
    case SQLITE_ROW: return S_OK;//SQLITE_E_ROW
    case SQLITE_DONE: return S_FALSE;//SQLITE_E_DONE
    // error
    case SQLITE_ABORT: return SQLITE_E_ABORT;
    case SQLITE_AUTH: return SQLITE_E_AUTH;
    case SQLITE_BUSY: return SQLITE_E_BUSY;
    case SQLITE_CANTOPEN: return SQLITE_E_CANTOPEN;
    case SQLITE_CONSTRAINT: return SQLITE_E_CONSTRAINT;
    case SQLITE_CORRUPT: return SQLITE_E_CORRUPT;
    case SQLITE_EMPTY: return SQLITE_E_EMPTY;
    case SQLITE_ERROR: return SQLITE_E_ERROR;
    case SQLITE_FORMAT: return SQLITE_E_FORMAT;
    case SQLITE_FULL: return SQLITE_E_FULL;
    case SQLITE_INTERRUPT: return SQLITE_E_INTERRUPT;
    case SQLITE_IOERR: return SQLITE_E_IOERR;
    case SQLITE_LOCKED: return SQLITE_E_LOCKED;
    case SQLITE_MISMATCH: return SQLITE_E_MISMATCH;
    case SQLITE_MISUSE: return SQLITE_E_MISUSE;
    case SQLITE_NOLFS: return SQLITE_E_NOLFS;
    case SQLITE_NOMEM: return SQLITE_E_NOMEM;
    case SQLITE_NOTADB: return SQLITE_E_NOTADB;
    case SQLITE_NOTFOUND: return SQLITE_E_NOTFOUND;
    case SQLITE_NOTICE: return SQLITE_E_NOTICE;
    case SQLITE_PERM: return SQLITE_E_PERM;
    case SQLITE_PROTOCOL: return SQLITE_E_PROTOCOL;
    case SQLITE_RANGE: return SQLITE_E_RANGE;
    case SQLITE_READONLY: return SQLITE_E_READONLY;
    case SQLITE_SCHEMA: return SQLITE_E_SCHEMA;
    case SQLITE_TOOBIG: return SQLITE_E_TOOBIG;
    case SQLITE_WARNING: return SQLITE_E_WARNING;
    // unexpected & unknown
    case SQLITE_INTERNAL: return SQLITE_E_INTERNAL;
    default:
        break;
    }

    return E_UNEXPECTED;
}

class CEmoji
    : public TUnkImpl< IEmoji >
{
private:
    typedef CEmoji CSelf;

protected:
    UINT m_uID;
    UINT m_uOrder;
    std::wstring m_sCodePoints;
    std::wstring m_sTTS;

protected:
    virtual ~CEmoji()
    {
    }

    CEmoji()
        : m_uID(0)
        , m_uOrder(0)
    {
    }

    STDMETHODIMP_(VOID) SetID(UINT uID)
    {
        m_uID = uID;
    }

    STDMETHODIMP_(VOID) SetOrder(UINT uOrder)
    {
        m_uOrder = uOrder;
    }

    STDMETHODIMP_(VOID) SetCodePoints(LPCWSTR pwszCodePoints)
    {
        m_sCodePoints.assign(pwszCodePoints);
    }

    STDMETHODIMP_(VOID) SetTTS(LPCWSTR pwszTTS)
    {
        m_sTTS.assign(pwszTTS);
    }

    static STDMETHODIMP FromStmt(sqlite3_stmt* pstmt, IEmoji** ppEmojiOut)
    {
        HRESULT hr;

        if(pstmt!=NULL && ppEmojiOut!=NULL)
        {
            CSelf* pEmoji = new CSelf;

            ppEmojiOut[0] = NULL;

            hr = pEmoji!=NULL ? S_OK : E_OUTOFMEMORY;
            if(!FAILED(hr))
            {
                pEmoji->SetID(sqlite3_column_int(pstmt, 0));
                pEmoji->SetCodePoints(static_cast< LPCWSTR >(sqlite3_column_text16(pstmt, 1)));
                pEmoji->SetOrder(sqlite3_column_int(pstmt, 2));
                pEmoji->SetTTS(static_cast< LPCWSTR >(sqlite3_column_text16(pstmt, 3)));
                CoSafeAddRef(static_cast< IEmoji* >(pEmoji), ppEmojiOut);
                CoSafeRelease(&pEmoji);
            }
        }
        else
        {
            hr = E_INVALIDARG;
        }


        return hr;
    }

public:
    STDMETHODIMP_(UINT) GetID() override
    {
        return m_uID;
    }

    STDMETHODIMP_(UINT) GetOrder() override
    {
        return m_uOrder;
    }

    STDMETHODIMP_(LPCWSTR) GetCodePoints() override
    {
        return m_sCodePoints.c_str();
    }

    STDMETHODIMP_(LPCWSTR) GetTTS() override
    {
        return m_sTTS.c_str();
    }

    friend class CEmojiDataSqlite3;
    friend class CEnumEmoji;
};

class CEnumEmoji
    : public TUnkImpl< IEnumEmoji >
{
private:
    typedef CEnumEmoji CSelf;

protected:
    sqlite3_stmt* m_pstmt;

protected:
    virtual ~CEnumEmoji()
    {
        AssertHere(sqlite3_finalize(m_pstmt)==SQLITE_OK);
        m_pstmt = NULL;
    }

    CEnumEmoji(sqlite3_stmt* pstmt)
        : m_pstmt(pstmt)
    {
    }

    static STDMETHODIMP FromStmt(sqlite3_stmt** ppstmt, IEnumEmoji** ppEnumEmojiOut)
    {
        HRESULT hr;

        if(ppstmt!=NULL && ppstmt[0]!=NULL && ppEnumEmojiOut!=NULL)
        {
            CSelf* pEnum = new CSelf(ppstmt[0]);

            ppEnumEmojiOut[0] = NULL;

            hr = pEnum!=NULL ? S_OK : E_OUTOFMEMORY;
            if(!FAILED(hr))
            {
                ppstmt[0] = NULL;
                CoSafeAddRef(static_cast< IEnumEmoji* >(pEnum), ppEnumEmojiOut);
                CoSafeRelease(&pEnum);
            }
        }
        else
        {
            hr = E_INVALIDARG;
        }

        return hr;
    }

public:
    STDMETHODIMP Next(ULONG celt, IEmoji** rgelt, ULONG* pceltFetched) override
    {
        HRESULT hr;

        if(rgelt!=NULL && (celt==1UL || pceltFetched!=NULL))
        {
            ULONG celtFetched;

            hr = S_OK;

            for(celtFetched = 0; celtFetched<celt; celtFetched++)
            {
                hr = Sqlite3ToHResult(sqlite3_step(m_pstmt));
                if(FAILED(hr) || hr==S_FALSE)
                {
                    break;
                }

                hr = CEmoji::FromStmt(m_pstmt, &rgelt[celtFetched]);
                if(FAILED(hr))
                {
                    break;
                }
            }

            if(FAILED(hr))
            {
                while(celtFetched>0UL)
                {
                    CoSafeRelease(&rgelt[celtFetched-1UL]);
                    celtFetched--;
                }
            }
            else
            if(pceltFetched!=NULL)
            {
                pceltFetched[0] = celtFetched;
            }
        }
        else
        {
            hr = E_INVALIDARG;
        }

        return hr;
    }

    STDMETHODIMP Skip(ULONG celt) override
    {
        HRESULT hr = S_OK;
        ULONG celtSkipped;

        for(celtSkipped = 0; celtSkipped<celt; celtSkipped++)
        {
            hr = Sqlite3ToHResult(sqlite3_step(m_pstmt));
            if(FAILED(hr) || hr==S_FALSE)
            {
                break;
            }
        }

        return hr;
    }

    STDMETHODIMP Reset() override
    {
        HRESULT hr;

        hr = Sqlite3ToHResult(sqlite3_reset(m_pstmt));

        return hr;
    }

    STDMETHODIMP Clone(IEnumEmoji** ppEnumOut) override
    {
        HRESULT hr;

        if(ppEnumOut!=NULL)
        {
            ppEnumOut[0] = NULL;
            hr = E_NOTIMPL;
        }
        else
        {
            hr = E_INVALIDARG;
        }

        return hr;
    }

    friend class CEmojiDataSqlite3;
};

class CEmojiDataSqlite3
    : public TUnkImpl< IEmojiData >
{
protected:
    sqlite3* m_db;

protected:
    virtual ~CEmojiDataSqlite3()
    {
        if(m_db!=NULL)
        {
            AssertHere(sqlite3_close(m_db)==SQLITE_OK);
            m_db = NULL;
        }
    }

    CEmojiDataSqlite3()
        : m_db(NULL)
    {
    }

public:
    STDMETHODIMP_(UINT) GetCount() override
    {
        HRESULT hr;
        sqlite3_stmt* pstmt = NULL;
        UINT uCount = 0;

        hr = Sqlite3ToHResult(sqlite3_prepare_v2(m_db,
            "SELECT COUNT(1) AS `count` FROM " TABLE_EMOJI "", -1, &pstmt, NULL));
        if(!FAILED(hr))
        {
            hr = Sqlite3ToHResult(sqlite3_step(pstmt));
            if(!FAILED(hr))
            {
                uCount = sqlite3_column_int(pstmt, 0);
            }

            AssertHere(sqlite3_finalize(pstmt)==SQLITE_OK);
            pstmt = NULL;
        }

        return uCount;
    }

    STDMETHODIMP GetEmoji(UINT uIndex, IEmoji** ppEmojiOut) override
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP GetEmojiByOrder(UINT uOrder, IEmoji** ppEmojiOut) override
    {
        HRESULT hr;
        sqlite3_stmt* pstmt = NULL;

        if(ppEmojiOut!=NULL)
        {
            ppEmojiOut[0] = NULL;

            hr = Sqlite3ToHResult(sqlite3_prepare_v2(m_db,
                "SELECT `id`, `cp`, `order`, `tts` FROM " TABLE_EMOJI " WHERE `order` = ?", -1, &pstmt, NULL));
            if(!FAILED(hr))
            {
                hr = Sqlite3ToHResult(sqlite3_bind_int(pstmt, 1, uOrder));
                if(!FAILED(hr))
                {
                    hr = Sqlite3ToHResult(sqlite3_step(pstmt));
                    if(hr==S_FALSE)
                    {
                        hr = SQLITE_E_NOTFOUND;
                    }
                    if(!FAILED(hr))
                    {
                        hr = CEmoji::FromStmt(pstmt, ppEmojiOut);
                    }
                }

                AssertHere(sqlite3_finalize(pstmt)==SQLITE_OK);
                pstmt = NULL;
            }
        }
        else
        {
            hr = E_INVALIDARG;
        }

        return hr;
    }

    STDMETHODIMP EnumEmoji(IEnumEmoji** ppEnumOut)
    {
        HRESULT hr;
        sqlite3_stmt* pstmt = NULL;

        if(ppEnumOut!=NULL)
        {
            ppEnumOut[0] = NULL;

            hr = Sqlite3ToHResult(sqlite3_prepare_v2(m_db,
                "SELECT `id`, `cp`, `order`, `tts` FROM " TABLE_EMOJI "", -1, &pstmt, NULL));
            if(!FAILED(hr))
            {
                hr = CEnumEmoji::FromStmt(&pstmt, ppEnumOut);

                if(pstmt!=NULL)
                {// enumerator may take ownership
                    AssertHere(sqlite3_finalize(pstmt)==SQLITE_OK);
                    pstmt = NULL;
                }
            }
        }
        else
        {
            hr = E_INVALIDARG;
        }

        return hr;
    }

    STDMETHODIMP EnumEmojiByKeyword(LPCWSTR lpszKeyword, IEnumEmoji** ppEnumOut)
    {
        HRESULT hr;
        sqlite3_stmt* pstmt = NULL;

        if(lpszKeyword!=NULL && ppEnumOut!=NULL)
        {
            ppEnumOut[0] = NULL;

            hr = Sqlite3ToHResult(sqlite3_prepare_v2(m_db,
                "SELECT `e`.`id`, `e`.`cp`, `e`.`order`, `e`.`tts` FROM " TABLE_EMOJI " AS `e` "
                "JOIN " TABLE_EMOJI_KEYWORD " AS `ekw` ON `e`.`id` = `ekw`.`emoji` "
                "JOIN " TABLE_KEYWORD " AS `kw` ON `ekw`.`keyword` = `kw`.`id` WHERE lower(`kw`.`name`) = lower(?1) OR `e`.`cp` = ?1 GROUP BY `e`.`id`", -1, &pstmt, NULL));
            if(!FAILED(hr))
            {
                hr = Sqlite3ToHResult(sqlite3_bind_text16(pstmt, 1, lpszKeyword, -1, SQLITE_TRANSIENT));
                if(!FAILED(hr))
                {
                    hr = CEnumEmoji::FromStmt(&pstmt, ppEnumOut);
                }

                if(pstmt!=NULL)
                {// enumerator may take ownership
                    AssertHere(sqlite3_finalize(pstmt)==SQLITE_OK);
                    pstmt = NULL;
                }
            }
        }
        else
        {
            hr = E_INVALIDARG;
        }

        return hr;
    }

    STDMETHODIMP Init(std::tstring const& sBackendName)
    {
        HRESULT hr;
        char* pu8szBackendName = NULL;
        size_t uDummy = 0;

        if(BvWideFromTo(sBackendName.c_str(), (sBackendName.length()+1U)*sizeof(TCHAR), CP_TCHAR, CP_UTF8, (void**)&pu8szBackendName, &uDummy))
        {
            hr = Sqlite3ToHResult(sqlite3_open(pu8szBackendName, &m_db));
            BvWideFreeA(&pu8szBackendName);
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }

        return hr;
    }

    friend HRESULT STDAPICALLTYPE GetEmojiDatabaseData(IEmojiData** ppDataOut);
};

STDAPI GetEmojiDatabaseData(IEmojiData** ppDataOut)
{
    HRESULT hr;
    CEmojiDataSqlite3* pData = new CEmojiDataSqlite3;

    hr = pData!=NULL ? S_OK : E_OUTOFMEMORY;
    if(!FAILED(hr))
    {
        hr = pData->Init(_T("emojitab.db"));
        if(!FAILED(hr))
        {
            CoSafeAddRef(static_cast< IEmojiData* >(pData), ppDataOut);
        }

        CoSafeRelease(&pData);
    }

    return hr;
}
