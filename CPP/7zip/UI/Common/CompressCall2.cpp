// CompressCall2.cpp

#include "StdAfx.h"

#ifndef Z7_EXTERNAL_CODECS

#include "../../../Common/MyException.h"
#include "../../../Common/IntToString.h"

#include "../../UI/Common/EnumDirItems.h"

#include "../../UI/FileManager/LangUtils.h"

#include "../../UI/GUI/BenchmarkDialog.h"
#include "../../UI/GUI/ExtractGUI.h"
#include "../../UI/GUI/UpdateGUI.h"
#include "../../UI/GUI/HashGUI.h"

#include "../../UI/GUI/ExtractRes.h"

#include "CompressCall.h"
#include "DismApi.h"

extern HWND g_HWND;

#define MY_TRY_BEGIN  HRESULT result; try {
#define MY_TRY_FINISH } \
  catch(CSystemException &e) { result = e.ErrorCode; } \
  catch(UString &s) { ErrorMessage(s); result = E_FAIL; } \
  catch(...) { result = E_FAIL; } \
  if (result != S_OK && result != E_ABORT) \
    ErrorMessageHRESULT(result);

static void ThrowException_if_Error(HRESULT res)
{
  if (res != S_OK)
    throw CSystemException(res);
}

#ifdef Z7_EXTERNAL_CODECS

#define CREATE_CODECS \
  CCodecs *codecs = new CCodecs; \
  CMyComPtr<ICompressCodecsInfo> compressCodecsInfo = codecs; \
  ThrowException_if_Error(codecs->Load()); \
  Codecs_AddHashArcHandler(codecs);

#define LOAD_EXTERNAL_CODECS \
    CExternalCodecs _externalCodecs; \
    _externalCodecs.GetCodecs = codecs; \
    _externalCodecs.GetHashers = codecs; \
    ThrowException_if_Error(_externalCodecs.Load());

#else

#define CREATE_CODECS \
  CCodecs *codecs = new CCodecs; \
  CMyComPtr<IUnknown> compressCodecsInfo = codecs; \
  ThrowException_if_Error(codecs->Load()); \
  Codecs_AddHashArcHandler(codecs);

#define LOAD_EXTERNAL_CODECS

#endif



 
UString GetQuotedString(const UString &s)
{
  UString s2 ('\"');
  s2 += s;
  s2 += '\"';
  return s2;
}

static void ErrorMessage(LPCWSTR message)
{
  MessageBoxW(g_HWND, message, L"7-Zip", MB_ICONERROR);
}

static void ErrorMessageHRESULT(HRESULT res)
{
  ErrorMessage(HResultToMessage(res));
}

static void ErrorLangMessage(UINT resourceID)
{
  ErrorMessage(LangString(resourceID));
}

HRESULT CompressFiles(
    const UString &arcPathPrefix,
    const UString &arcName,
    const UString &arcType,
    bool addExtension,
    const UStringVector &names,
    bool email, bool showDialog, bool /* waitFinish */)
{
  MY_TRY_BEGIN
  
  CREATE_CODECS

  CUpdateCallbackGUI callback;
  
  callback.Init();

  CUpdateOptions uo;
  uo.EMailMode = email;
  uo.SetActionCommand_Add();

  uo.ArcNameMode = (addExtension ? k_ArcNameMode_Add : k_ArcNameMode_Exact);

  CObjectVector<COpenType> formatIndices;
  if (!ParseOpenTypes(*codecs, arcType, formatIndices))
  {
    ErrorLangMessage(IDS_UNSUPPORTED_ARCHIVE_TYPE);
    return E_FAIL;
  }
  const UString arcPath = arcPathPrefix + arcName;
  if (!uo.InitFormatIndex(codecs, formatIndices, arcPath) ||
      !uo.SetArcPath(codecs, arcPath))
  {
    ErrorLangMessage(IDS_UPDATE_NOT_SUPPORTED);
    return E_FAIL;
  }

  NWildcard::CCensor censor;
  FOR_VECTOR (i, names)
  {
    censor.AddPreItem_NoWildcard(names[i]);
  }

  bool messageWasDisplayed = false;

  result = UpdateGUI(codecs,
      formatIndices, arcPath,
      censor, uo, showDialog, messageWasDisplayed, &callback, g_HWND);
  
  if (result != S_OK)
  {
    if (result != E_ABORT && messageWasDisplayed)
      return E_FAIL;
    throw CSystemException(result);
  }
  if (callback.FailedFiles.Size() > 0)
  {
    if (!messageWasDisplayed)
      throw CSystemException(E_FAIL);
    return E_FAIL;
  }
  
  MY_TRY_FINISH
  return S_OK;
}

static HRESULT ExtractGroupCommand(const UStringVector &arcPaths,
    bool showDialog, CExtractOptions &eo, const char *kType = NULL)
{
  MY_TRY_BEGIN
  
  CREATE_CODECS

  CExtractCallbackImp *ecs = new CExtractCallbackImp;
  CMyComPtr<IFolderArchiveExtractCallback> extractCallback = ecs;
  
  ecs->Init();
  
  // eo.CalcCrc = options.CalcCrc;
  
  UStringVector arcPathsSorted;
  UStringVector arcFullPathsSorted;
  {
    NWildcard::CCensor arcCensor;
    FOR_VECTOR (i, arcPaths)
    {
      arcCensor.AddPreItem_NoWildcard(arcPaths[i]);
    }
    arcCensor.AddPathsToCensor(NWildcard::k_RelatPath);
    CDirItemsStat st;
    EnumerateDirItemsAndSort(arcCensor, NWildcard::k_RelatPath, UString(),
        arcPathsSorted, arcFullPathsSorted,
        st,
        NULL // &scan: change it!!!!
        );
  }
  
  CObjectVector<COpenType> formatIndices;
  if (kType)
  {
    if (!ParseOpenTypes(*codecs, UString(kType), formatIndices))
    {
      throw CSystemException(E_INVALIDARG);
      // ErrorLangMessage(IDS_UNSUPPORTED_ARCHIVE_TYPE);
      // return E_INVALIDARG;
    }
  }
  
  NWildcard::CCensor censor;
  {
    censor.AddPreItem_Wildcard();
  }
  
  censor.AddPathsToCensor(NWildcard::k_RelatPath);

  bool messageWasDisplayed = false;

  ecs->MultiArcMode = (arcPathsSorted.Size() > 1);

  result = ExtractGUI(codecs,
      formatIndices, CIntVector(),
      arcPathsSorted, arcFullPathsSorted,
      censor.Pairs.Front().Head, eo, NULL, showDialog, messageWasDisplayed, ecs, g_HWND);

  if (result != S_OK)
  {
    if (result != E_ABORT && messageWasDisplayed)
      return E_FAIL;
    throw CSystemException(result);
  }
  return ecs->IsOK() ? S_OK : E_FAIL;
  
  MY_TRY_FINISH
  return result;
}

void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder,
    bool showDialog, bool elimDup, UInt32 writeZone)
{
  CExtractOptions eo;
  eo.OutputDir = us2fs(outFolder);
  eo.TestMode = false;
  eo.ElimDup.Val = elimDup;
  eo.ElimDup.Def = elimDup;
  if (writeZone != (UInt32)(Int32)-1)
    eo.ZoneMode = (NExtract::NZoneIdMode::EEnum)writeZone;
  ExtractGroupCommand(arcPaths, showDialog, eo);
}

void TestArchives(const UStringVector &arcPaths, bool hashMode)
{
  CExtractOptions eo;
  eo.TestMode = true;
  ExtractGroupCommand(arcPaths,
      true, // showDialog
      eo,
      hashMode ? "hash" : NULL);
}

FString GuidArchives(UStringVector& arcPaths, bool hashMode)
{
  CExtractOptions eo;
  eo.TestMode = true;
  ExtractGroupCommand(arcPaths,
    false, // showDialog
    eo,
    hashMode ? "hash" : NULL);
  arcPaths = eo.MountImages;
  return eo.WimGuid;
}

bool GetMountedImageInfo_NT(UStringVector& mountPaths, UStringVector& mountImages);

static FString HrToString(HRESULT hr) {
    wchar_t ii[100];
    ConvertInt64ToString(hr, ii);
    FString hrs = FString(ii);
    return hrs;
}

bool GetMountedImageInfo_NT(UStringVector& mountPaths, UStringVector& mountImages)
{
    HRESULT hr = S_OK;
    HRESULT hrLocal = S_OK;

    // Initialize the API
    hr = DismInitialize(DismLogErrorsWarningsInfo, L"C:\\MyLogFile.txt", NULL);
    if (FAILED(hr))
    {
        FString message = L"DismInitialize Failed: ";
        OutputDebugStringW(message + HrToString(hr));

        goto Cleanup;
    }

    DismMountedImageInfo* ImageInfo;
    UINT ImageInfoCount;

    hr = DismGetMountedImageInfo(&ImageInfo, &ImageInfoCount);

    if (FAILED(hr))
    {
        OutputDebugStringW(L"DismGetMountedImageInfo Failed: " + HrToString(hr));
        goto Cleanup;
    }

    for (unsigned i = 0; i < ImageInfoCount; i++) {
        OutputDebugStringW(L"ImageInfo[%d].MountPath: " + FString(ImageInfo[i].MountPath));
        OutputDebugStringW(L"ImageInfo[%d].ImageFilePath: " + FString(ImageInfo[i].ImageFilePath));
        OutputDebugStringW(L"ImageInfo[%d].ImageIndex: " + ImageInfo[i].ImageIndex);
        OutputDebugStringW(L"ImageInfo[%d].MountMode: " + ImageInfo[i].MountMode);
        OutputDebugStringW(L"ImageInfo[%d].MountStatus: " + ImageInfo[i].MountStatus);
        mountPaths.Add(FString(ImageInfo[i].MountPath));
        mountImages.Add(FString(ImageInfo[i].ImageFilePath));
    }

Cleanup:

    // Shutdown the DISM API to free up remaining resources
    hrLocal = DismShutdown();
    if (FAILED(hrLocal))
    {
        FString message = L"DismShutdown Failed: ";
        OutputDebugStringW(message + HrToString(hr));
    }

    FString message = L"Return code is: ";
    OutputDebugStringW(message + HrToString(hr));

    if (hr == S_OK) {
        return true;
    }
    else {
        return false;
    }
}

void GetMountedImageInfo(UStringVector& mountPaths, UStringVector& mountImages)
{
    GetMountedImageInfo_NT(mountPaths, mountImages);
}

void CalcChecksum(const UStringVector &paths,
    const UString &methodName,
    const UString &arcPathPrefix,
    const UString &arcFileName)
{
  MY_TRY_BEGIN

  if (!arcFileName.IsEmpty())
  {
    CompressFiles(
      arcPathPrefix,
      arcFileName,
      UString("hash"),
      false, // addExtension,
      paths,
      false, // email,
      false, // showDialog,
      false  // waitFinish
      );
    return;
  }
  
  CREATE_CODECS
  LOAD_EXTERNAL_CODECS

  NWildcard::CCensor censor;
  FOR_VECTOR (i, paths)
  {
    censor.AddPreItem_NoWildcard(paths[i]);
  }

  censor.AddPathsToCensor(NWildcard::k_RelatPath);
  bool messageWasDisplayed = false;

  CHashOptions options;
  options.Methods.Add(methodName);

  /*
  if (!arcFileName.IsEmpty())
    options.HashFilePath = arcPathPrefix + arcFileName;
  */

  result = HashCalcGUI(EXTERNAL_CODECS_VARS_L censor, options, messageWasDisplayed);
  if (result != S_OK)
  {
    if (result != E_ABORT && messageWasDisplayed)
      return; //  E_FAIL;
    throw CSystemException(result);
  }
  
  MY_TRY_FINISH
  return; //  result;
}

void Benchmark(bool totalMode)
{
  MY_TRY_BEGIN
  
  CREATE_CODECS
  LOAD_EXTERNAL_CODECS
  
  CObjectVector<CProperty> props;
  if (totalMode)
  {
    CProperty prop;
    prop.Name = "m";
    prop.Value = "*";
    props.Add(prop);
  }
  result = Benchmark(
      EXTERNAL_CODECS_VARS_L
      props,
      k_NumBenchIterations_Default,
      g_HWND);
  
  MY_TRY_FINISH
}

#endif
