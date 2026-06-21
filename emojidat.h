
#ifndef EMOJIDAT_H
#define EMOJIDAT_H

#undef  INTERFACE
#define INTERFACE IEmoji
DECLARE_INTERFACE_IID_(IEmoji, IUnknown, "2e0dcfa0-6278-4104-81de-2e73244b1265")
{
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppOut) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    STDMETHOD_(UINT, GetID)(THIS) PURE;
    STDMETHOD_(UINT, GetOrder)(THIS) PURE;
    STDMETHOD_(LPCWSTR, GetCodePoints)(THIS) PURE;
    STDMETHOD_(LPCWSTR, GetTTS)(THIS) PURE;
};

#undef  INTERFACE
#define INTERFACE IEnumEmoji
DECLARE_INTERFACE_IID_(IEnumEmoji, IUnknown, "2e0dcfa0-6278-4104-81de-2e73244b1266")
{
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppOut) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    STDMETHOD(Next)(THIS_ ULONG celt, IEmoji** rgelt, ULONG* pceltFetched) PURE;
    STDMETHOD(Skip)(THIS_ ULONG celt) PURE;
    STDMETHOD(Reset)(THIS) PURE;
    STDMETHOD(Clone)(THIS_ IEnumEmoji** ppEnumOut) PURE;
};

#undef  INTERFACE
#define INTERFACE IEmojiData
DECLARE_INTERFACE_IID_(IEmojiData, IUnknown, "2e0dcfa0-6278-4104-81de-2e73244b1264")
{
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppOut) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    STDMETHOD_(UINT, GetCount)(THIS) PURE;
    STDMETHOD(GetEmoji)(THIS_ UINT uIndex, IEmoji** ppEmojiOut) PURE;
    STDMETHOD(GetEmojiByOrder)(THIS_ UINT uOrder, IEmoji** ppEmojiOut) PURE;
    STDMETHOD(EnumEmoji)(THIS_ IEnumEmoji** ppEnumOut) PURE;
    STDMETHOD(EnumEmojiByKeyword)(THIS_ LPCWSTR lpszKeyword, IEnumEmoji** ppEnumOut) PURE;
};

STDAPI GetEmojiDatabaseData(IEmojiData** ppDataOut);

#endif  /* EMOJIDAT_H */
